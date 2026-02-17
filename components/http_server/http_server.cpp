#include "http_server.h"

#include <string.h>
#include "esp_http_server.h"
#include "esp_log.h"
#include "lvgl.h"
#include "esp_lv_adapter.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "http_server";
static httpd_handle_t s_httpd = nullptr;

#if LV_USE_SNAPSHOT
static void write_le16(uint8_t *out, uint16_t value)
{
    out[0] = (uint8_t)(value & 0xFF);
    out[1] = (uint8_t)((value >> 8) & 0xFF);
}

static void write_le32(uint8_t *out, uint32_t value)
{
    out[0] = (uint8_t)(value & 0xFF);
    out[1] = (uint8_t)((value >> 8) & 0xFF);
    out[2] = (uint8_t)((value >> 16) & 0xFF);
    out[3] = (uint8_t)((value >> 24) & 0xFF);
}
#endif

static esp_err_t send_bmp_snapshot(httpd_req_t *req)
{
#if !LV_USE_SNAPSHOT
    httpd_resp_send_err(req, HTTPD_501_METHOD_NOT_IMPLEMENTED, "LVGL snapshot support is disabled");
    return ESP_FAIL;
#else
    lv_draw_buf_t *snapshot = nullptr;

    if (esp_lv_adapter_lock(pdMS_TO_TICKS(2000)) != ESP_OK) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "LVGL lock failed");
        return ESP_FAIL;
    }

    lv_obj_t *screen = lv_screen_active();
    snapshot = lv_snapshot_take(screen, LV_COLOR_FORMAT_RGB565);
    esp_lv_adapter_unlock();

    if (!snapshot) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Snapshot failed");
        return ESP_FAIL;
    }

    const uint32_t width = snapshot->header.w;
    const uint32_t height = snapshot->header.h;
    const uint32_t stride = snapshot->header.stride;
    const uint32_t row_bytes = width * 2;
    const uint32_t row_padded = (row_bytes + 3U) & ~3U;
    const uint32_t pixel_bytes = row_padded * height;
    const uint32_t header_bytes = 14 + 40 + 12;
    const uint32_t file_size = header_bytes + pixel_bytes;

    uint8_t header[66] = {0};
    header[0] = 'B';
    header[1] = 'M';
    write_le32(&header[2], file_size);
    write_le32(&header[10], header_bytes);

    write_le32(&header[14], 40);                // BITMAPINFOHEADER size
    write_le32(&header[18], width);
    const int32_t top_down_height = -(int32_t)height;
    write_le32(&header[22], (uint32_t)top_down_height); // top-down
    write_le16(&header[26], 1);                 // planes
    write_le16(&header[28], 16);                // bpp
    write_le32(&header[30], 3);                 // BI_BITFIELDS
    write_le32(&header[34], pixel_bytes);
    write_le32(&header[38], 2835);              // 72 DPI
    write_le32(&header[42], 2835);
    write_le32(&header[54], 0xF800);            // red mask
    write_le32(&header[58], 0x07E0);            // green mask
    write_le32(&header[62], 0x001F);            // blue mask

    httpd_resp_set_type(req, "image/bmp");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");

    bool ok = true;
    const uint8_t padding[3] = {0};
    if (httpd_resp_send_chunk(req, (const char *)header, sizeof(header)) != ESP_OK) {
        httpd_resp_send_chunk(req, nullptr, 0);
        ok = false;
    }

    if (ok) {
        for (uint32_t y = 0; y < height; ++y) {
            const uint8_t *row = snapshot->data + (y * stride);
            if (httpd_resp_send_chunk(req, (const char *)row, row_bytes) != ESP_OK) {
                httpd_resp_send_chunk(req, nullptr, 0);
                ok = false;
                break;
            }
            if (row_padded > row_bytes) {
                uint32_t pad = row_padded - row_bytes;
                if (httpd_resp_send_chunk(req, (const char *)padding, pad) != ESP_OK) {
                    httpd_resp_send_chunk(req, nullptr, 0);
                    ok = false;
                    break;
                }
            }
        }
    }

    if (ok) {
        httpd_resp_send_chunk(req, nullptr, 0);
    }

    if (esp_lv_adapter_lock(pdMS_TO_TICKS(2000)) == ESP_OK) {
        lv_draw_buf_destroy(snapshot);
        esp_lv_adapter_unlock();
    } else {
        lv_draw_buf_destroy(snapshot);
    }
    return ok ? ESP_OK : ESP_FAIL;
#endif
}

static esp_err_t screenshot_handler(httpd_req_t *req)
{
    return send_bmp_snapshot(req);
}

static esp_err_t api_index_handler(httpd_req_t *req)
{
    static const char *payload = "{\"endpoints\":[\"/api/screenshot\"]}\n";
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    return httpd_resp_send(req, payload, HTTPD_RESP_USE_STRLEN);
}

esp_err_t http_server_start(void)
{
    if (s_httpd) {
        ESP_LOGW(TAG, "HTTP server already running");
        return ESP_OK;
    }

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.uri_match_fn = httpd_uri_match_wildcard;
    config.stack_size = 8192;

    if (httpd_start(&s_httpd, &config) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start HTTP server");
        return ESP_FAIL;
    }

    const httpd_uri_t screenshot_uri = {
        .uri = "/api/screenshot",
        .method = HTTP_GET,
        .handler = screenshot_handler,
        .user_ctx = nullptr,
    };

    const httpd_uri_t api_index_uri = {
        .uri = "/api",
        .method = HTTP_GET,
        .handler = api_index_handler,
        .user_ctx = nullptr,
    };

    esp_err_t err = httpd_register_uri_handler(s_httpd, &api_index_uri);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register /api: %s", esp_err_to_name(err));
        httpd_stop(s_httpd);
        s_httpd = nullptr;
        return err;
    }

    err = httpd_register_uri_handler(s_httpd, &screenshot_uri);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register /api/screenshot: %s", esp_err_to_name(err));
        httpd_stop(s_httpd);
        s_httpd = nullptr;
        return err;
    }

    ESP_LOGI(TAG, "HTTP server started: /api/screenshot");
    return ESP_OK;
}

void http_server_stop(void)
{
    if (!s_httpd) {
        return;
    }

    httpd_stop(s_httpd);
    s_httpd = nullptr;
    ESP_LOGI(TAG, "HTTP server stopped");
}
