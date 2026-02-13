#include "hmi_widget.h"

void HMIWidget::scheduleAsync(lv_async_cb_t cb, void* user_data) {
	if (m_async_pending) {
		return;
	}
	m_async_pending = true;
	lv_async_call(cb, user_data);
}

void HMIWidget::cancelAsync(lv_async_cb_t cb, void* user_data) {
	lv_async_call_cancel(cb, user_data);
	m_async_pending = false;
}

void HMIWidget::markAsyncComplete() {
	m_async_pending = false;
}
