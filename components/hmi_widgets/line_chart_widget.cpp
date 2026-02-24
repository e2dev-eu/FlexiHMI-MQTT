#include "line_chart_widget.h"
#include "mqtt_manager.h"
#include <esp_log.h>
#include <cstdlib>

static const char *TAG = "LineChartWidget";

LineChartWidget::LineChartWidget(const std::string& id, int x, int y, int w, int h, cJSON* properties, lv_obj_t* parent) {
    m_id = id;
    int initial_value = 0;
    lv_color_t line_color = lv_palette_main(LV_PALETTE_BLUE);

    if (properties) {
        cJSON* min_item = cJSON_GetObjectItem(properties, "min");
        if (min_item && cJSON_IsNumber(min_item)) {
            m_min_value = min_item->valueint;
        }

        cJSON* max_item = cJSON_GetObjectItem(properties, "max");
        if (max_item && cJSON_IsNumber(max_item)) {
            m_max_value = max_item->valueint;
        }

        cJSON* points_item = cJSON_GetObjectItem(properties, "points");
        if (points_item && cJSON_IsNumber(points_item)) {
            m_point_count = points_item->valueint;
        }

        cJSON* value_item = cJSON_GetObjectItem(properties, "value");
        if (value_item && cJSON_IsNumber(value_item)) {
            initial_value = value_item->valueint;
        }

        cJSON* mqtt_topic = cJSON_GetObjectItem(properties, "mqtt_topic");
        if (mqtt_topic && cJSON_IsString(mqtt_topic)) {
            m_mqtt_topic = mqtt_topic->valuestring;
        }

        cJSON* color_item = cJSON_GetObjectItem(properties, "color");
        if (color_item && cJSON_IsString(color_item)) {
            const char* color_str = color_item->valuestring;
            if (color_str[0] == '#') {
                uint32_t color = strtol(color_str + 1, NULL, 16);
                line_color = lv_color_hex(color);
            }
        }
    }

    if (m_min_value > m_max_value) {
        int temp = m_min_value;
        m_min_value = m_max_value;
        m_max_value = temp;
    }

    if (m_point_count < 1) {
        m_point_count = 1;
    }

    if (initial_value < m_min_value) initial_value = m_min_value;
    if (initial_value > m_max_value) initial_value = m_max_value;
    m_pending_value = initial_value;

    lv_obj_t* parent_obj = parent ? parent : lv_screen_active();
    m_lvgl_obj = lv_chart_create(parent_obj);
    if (!m_lvgl_obj) {
        ESP_LOGE(TAG, "Failed to create line chart widget: %s", id.c_str());
        return;
    }

    lv_obj_set_pos(m_lvgl_obj, x, y);
    lv_obj_set_size(m_lvgl_obj, w, h);

    lv_chart_set_type(m_lvgl_obj, LV_CHART_TYPE_LINE);
    lv_chart_set_range(m_lvgl_obj, LV_CHART_AXIS_PRIMARY_Y, m_min_value, m_max_value);
    lv_chart_set_point_count(m_lvgl_obj, m_point_count);
    lv_chart_set_div_line_count(m_lvgl_obj, 5, 5);

    m_series = lv_chart_add_series(m_lvgl_obj, line_color, LV_CHART_AXIS_PRIMARY_Y);
    if (!m_series) {
        ESP_LOGE(TAG, "Failed to create line chart series: %s", id.c_str());
        return;
    }

    for (int i = 0; i < m_point_count; i++) {
        lv_chart_set_next_value(m_lvgl_obj, m_series, initial_value);
    }
    lv_chart_refresh(m_lvgl_obj);

    if (!m_mqtt_topic.empty()) {
        m_subscription_handle = MQTTManager::getInstance().subscribe(m_mqtt_topic, 0,
            [this](const std::string& topic, const std::string& payload) {
                this->onMqttMessage(topic, payload);
            });

        if (m_subscription_handle != 0) {
            ESP_LOGI(TAG, "Line chart %s subscribed to %s", id.c_str(), m_mqtt_topic.c_str());
        }
    }

    ESP_LOGI(TAG, "Created line chart widget: %s at (%d,%d) size (%dx%d), points=%d range=[%d,%d]",
             id.c_str(), x, y, w, h, m_point_count, m_min_value, m_max_value);
}

LineChartWidget::~LineChartWidget() {
    cancelAsync(async_update_cb, this);
    if (m_subscription_handle != 0) {
        MQTTManager::getInstance().unsubscribe(m_subscription_handle);
        m_subscription_handle = 0;
    }

    if (m_lvgl_obj && lv_obj_is_valid(m_lvgl_obj)) {
        lv_obj_delete(m_lvgl_obj);
        m_lvgl_obj = nullptr;
    }
}

void LineChartWidget::onMqttMessage(const std::string& topic, const std::string& payload) {
    int value = atoi(payload.c_str());
    if (value < m_min_value) value = m_min_value;
    if (value > m_max_value) value = m_max_value;

    m_pending_value = value;
    scheduleAsync(async_update_cb, this);
}

void LineChartWidget::async_update_cb(void* user_data) {
    LineChartWidget* widget = static_cast<LineChartWidget*>(user_data);
    if (!widget) {
        return;
    }
    widget->markAsyncComplete();
    widget->pushValue(widget->m_pending_value);
}

void LineChartWidget::pushValue(int value) {
    if (!m_lvgl_obj || !lv_obj_is_valid(m_lvgl_obj) || !m_series) {
        return;
    }

    lv_chart_set_next_value(m_lvgl_obj, m_series, value);
    lv_chart_refresh(m_lvgl_obj);
}
