/*
 * Copyright (c) 2026 Conclusive Engineering
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/display.h>
#include <lvgl_input_device.h>
#include <lvgl.h>
#include <font/lv_symbol_def.h>

#include <stdio.h>
#include <string.h>
#include <zephyr/kernel.h>

#define LOG_LEVEL CONFIG_LOG_DEFAULT_LEVEL
#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(app);

#ifdef CONFIG_LV_Z_POINTER_INPUT
static const struct device *lvgl_pointer = DEVICE_DT_GET(
		DT_COMPAT_GET_ANY_STATUS_OKAY(zephyr_lvgl_pointer_input));
#endif

/* Screen dimensions */
#define SCREEN_W 240
#define SCREEN_H 320

/* Tab bar height is ~40px, content area height = 320 - 40 = 280 */
#define TAB_H    40
#define CONT_H   (SCREEN_H - TAB_H)

/* ------------------------------------------------------------------ */
/* Shared state                                                         */
/* ------------------------------------------------------------------ */
static uint32_t uptime_sec;
static lv_obj_t *uptime_label;
static lv_obj_t *slider_val_label;
static lv_obj_t *spinbox;

/* ------------------------------------------------------------------ */
/* Tab 1 – Controls: slider + checkbox + switch                        */
/* ------------------------------------------------------------------ */
static void slider_event_cb(lv_event_t *e) {
	lv_obj_t *slider = lv_event_get_target(e);
	char buf[16];
	snprintf(buf, sizeof(buf), "Val: %d", (int) lv_slider_get_value(slider));
	lv_label_set_text(slider_val_label, buf);
}

static void create_tab_controls(lv_obj_t *parent) {
	/* Make the parent scrollable with a simple flex column */
	lv_obj_set_flex_flow(parent, LV_FLEX_FLOW_COLUMN);
	lv_obj_set_flex_align(parent, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
			LV_FLEX_ALIGN_CENTER);
	lv_obj_set_style_pad_row(parent, 14, 0);
	lv_obj_set_style_pad_top(parent, 12, 0);

	/* --- Slider --- */
	lv_obj_t *lbl = lv_label_create(parent);
	lv_label_set_text(lbl, "Brightness");

	lv_obj_t *slider = lv_slider_create(parent);
	lv_obj_set_width(slider, SCREEN_W - 40);
	lv_slider_set_range(slider, 0, 100);
	lv_slider_set_value(slider, 50, LV_ANIM_OFF);
	lv_obj_add_event_cb(slider, slider_event_cb, LV_EVENT_VALUE_CHANGED, NULL);

	slider_val_label = lv_label_create(parent);
	lv_label_set_text(slider_val_label, "Val: 50");

	/* --- Checkbox --- */
	lv_obj_t *cb = lv_checkbox_create(parent);
	lv_checkbox_set_text(cb, "Enable notifications");
	lv_obj_add_state(cb, LV_STATE_CHECKED);

	/* --- Toggle switch --- */
	lv_obj_t *sw_label = lv_label_create(parent);
	lv_label_set_text(sw_label, "Dark mode");

	lv_obj_t *sw = lv_switch_create(parent);
	lv_obj_add_state(sw, LV_STATE_CHECKED);
}

/* ------------------------------------------------------------------ */
/* Tab 2 – Info: status labels + uptime counter                        */
/* ------------------------------------------------------------------ */
static void create_tab_info(lv_obj_t *parent) {
	lv_obj_set_flex_flow(parent, LV_FLEX_FLOW_COLUMN);
	lv_obj_set_flex_align(parent, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START,
			LV_FLEX_ALIGN_START);
	lv_obj_set_style_pad_row(parent, 10, 0);
	lv_obj_set_style_pad_all(parent, 12, 0);

	/* Title */
	lv_obj_t *title = lv_label_create(parent);
	lv_label_set_text(title, "System Information");
	lv_obj_set_style_text_font(title, &lv_font_montserrat_14, 0);

	/* Temperature row */
	lv_obj_t *temp = lv_label_create(parent);
	lv_label_set_text(temp, LV_SYMBOL_WARNING "  Temp:    25 °C");

	/* Status row */
	lv_obj_t *status = lv_label_create(parent);
	lv_label_set_text(status, LV_SYMBOL_OK "  Status:  OK");

	/* Battery row */
	lv_obj_t *batt = lv_label_create(parent);
	lv_label_set_text(batt, LV_SYMBOL_BATTERY_FULL "  Battery: 87 %");

	/* Wifi row */
	lv_obj_t *wifi = lv_label_create(parent);
	lv_label_set_text(wifi, LV_SYMBOL_WIFI "  Network: Connected");

	/* Divider */
	lv_obj_t *line = lv_obj_create(parent);
	lv_obj_set_size(line, SCREEN_W - 24, 1);
	lv_obj_set_style_bg_color(line, lv_color_hex(0x888888), 0);
	lv_obj_set_style_border_width(line, 0, 0);

	/* Uptime (updated in main loop) */
	lv_obj_t *up_lbl = lv_label_create(parent);
	lv_label_set_text(up_lbl, LV_SYMBOL_REFRESH "  Uptime:");

	uptime_label = lv_label_create(parent);
	lv_label_set_text(uptime_label, "0 s");
}

/* ------------------------------------------------------------------ */
/* Tab 3 – Settings: dropdown + spinbox + apply button                 */
/* ------------------------------------------------------------------ */
static void apply_btn_cb(lv_event_t *e) {
	ARG_UNUSED(e);
	LOG_INF("Settings applied: spinbox value = %d",
			(int)lv_spinbox_get_value(spinbox));
}

static void spinbox_inc_cb(lv_event_t *e) {
	ARG_UNUSED(e);
	int value = lv_spinbox_get_value(spinbox) + 1;
	lv_spinbox_set_value(spinbox, value);
	LOG_INF("Settings applied(inc): spinbox value = %d", value);
}

static void spinbox_dec_cb(lv_event_t *e) {
	ARG_UNUSED(e);
	int value = lv_spinbox_get_value(spinbox) - 1;
	if (value < 0) {
		value = 0;
	}
	lv_spinbox_set_value(spinbox, value);
	LOG_INF("Settings applied(inc): spinbox value = %d", value);
}

static void create_tab_settings(lv_obj_t *parent) {
	lv_obj_set_flex_flow(parent, LV_FLEX_FLOW_COLUMN);
	lv_obj_set_flex_align(parent, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
			LV_FLEX_ALIGN_CENTER);
	lv_obj_set_style_pad_row(parent, 12, 0);
	lv_obj_set_style_pad_top(parent, 12, 0);

	/* Dropdown */
	lv_obj_t *dd_lbl = lv_label_create(parent);
	lv_label_set_text(dd_lbl, "Display language");

	lv_obj_t *dd = lv_dropdown_create(parent);
	lv_dropdown_set_options(dd, "English\nDeutsch\nFrancais\nEspanol");
	lv_obj_set_width(dd, SCREEN_W - 40);

	/* Spinbox for timeout */
	lv_obj_t *sp_lbl = lv_label_create(parent);
	lv_label_set_text(sp_lbl, "Sleep timeout (s)");

	/* Spinbox + increment/decrement row */
	lv_obj_t *sp_row = lv_obj_create(parent);
	lv_obj_set_size(sp_row, SCREEN_W - 40, 46);
	lv_obj_set_style_border_width(sp_row, 0, 0);
	lv_obj_set_style_pad_all(sp_row, 0, 0);
	lv_obj_set_flex_flow(sp_row, LV_FLEX_FLOW_ROW);
	lv_obj_set_flex_align(sp_row, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
			LV_FLEX_ALIGN_CENTER);

	lv_obj_t *btn_dec = lv_button_create(sp_row);
	lv_obj_set_size(btn_dec, 36, 36);
	lv_obj_t *btn_dec_lbl = lv_label_create(btn_dec);
	lv_label_set_text(btn_dec_lbl, LV_SYMBOL_MINUS);
	lv_obj_center(btn_dec_lbl);

	spinbox = lv_spinbox_create(sp_row);
	lv_spinbox_set_range(spinbox, 5, 300);
	lv_spinbox_set_value(spinbox, 30);
	lv_spinbox_set_digit_count(spinbox, 3);
	lv_obj_set_width(spinbox, 80);

	lv_obj_t *btn_inc = lv_button_create(sp_row);
	lv_obj_set_size(btn_inc, 36, 36);
	lv_obj_t *btn_inc_lbl = lv_label_create(btn_inc);
	lv_label_set_text(btn_inc_lbl, LV_SYMBOL_PLUS);
	lv_obj_center(btn_inc_lbl);

	/* Wire increment / decrement */

	lv_obj_add_event_cb(btn_inc, spinbox_inc_cb, LV_EVENT_CLICKED, NULL);
	lv_obj_add_event_cb(btn_dec, spinbox_dec_cb, LV_EVENT_CLICKED, NULL);
	/* Apply button */
	lv_obj_t *apply_btn = lv_button_create(parent);
	lv_obj_set_size(apply_btn, SCREEN_W - 40, 40);
	lv_obj_t *apply_lbl = lv_label_create(apply_btn);
	lv_label_set_text(apply_lbl, "Apply Settings");
	lv_obj_center(apply_lbl);
	lv_obj_add_event_cb(apply_btn, apply_btn_cb, LV_EVENT_CLICKED, NULL);
}

/* ------------------------------------------------------------------ */
/* main                                                                 */
/* ------------------------------------------------------------------ */
int main(void) {
	const struct device *display_dev;
	int ret;

	display_dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_display));
	if (!device_is_ready(display_dev)) {
		LOG_ERR("Device not ready, aborting test");
		return 0;
	}

	/* ---- Tab view ---- */
	lv_obj_t *tabview = lv_tabview_create(lv_screen_active());
	lv_tabview_set_tab_bar_position(tabview, LV_DIR_TOP);
	lv_tabview_set_tab_bar_size(tabview, TAB_H);
	lv_obj_set_size(tabview, SCREEN_W, SCREEN_H);

	lv_obj_t *tab2 = lv_tabview_add_tab(tabview, LV_SYMBOL_LIST " Info");
	lv_obj_t *tab1 = lv_tabview_add_tab(tabview,
			LV_SYMBOL_SETTINGS " Controls");
	lv_obj_t *tab3 = lv_tabview_add_tab(tabview, LV_SYMBOL_EDIT " Settings");

	create_tab_info(tab2);
	create_tab_controls(tab1);
	create_tab_settings(tab3);

#ifdef CONFIG_LV_Z_POINTER_INPUT
	if (!device_is_ready(lvgl_pointer)) {
		LOG_WRN("LVGL pointer input device not ready");
	} else {
		lv_obj_t *cursor_obj = lv_label_create(lv_screen_active());
		LOG_INF("Touch screen ready: %s", lvgl_pointer->name);

		/* Set a built-in symbol (e.g., SYMBOL_RIGHT or an ASCII arrow "<") */
		lv_label_set_text(cursor_obj, LV_SYMBOL_RIGHT);

		/* Prevent the cursor from blocking clicks underneath it */
		lv_obj_remove_flag(cursor_obj, LV_OBJ_FLAG_CLICKABLE);
		/* Pointer input feeds events directly to LVGL — no group needed. */
		/* Optionally set cursor object: */
		lv_indev_set_cursor(lvgl_input_get_indev(lvgl_pointer), cursor_obj);
	}
#endif
	/* Turn display on */
	lv_timer_handler();
	ret = display_blanking_off(display_dev);
	if (ret < 0 && ret != -ENOSYS) {
		LOG_ERR("Failed to turn blanking off (error %d)", ret);
		return 0;
	}

	/* ---- Main loop: update LVGL + uptime counter ---- */
	uint32_t tick = 0;

	while (1) {
		lv_timer_handler();

		/* Update uptime label once per second (100 * 10 ms = 1 s) */
		if ((tick % 100) == 0U) {
			uptime_sec = tick / 100U;
			if (uptime_label) {
				char buf[20];
				snprintf(buf, sizeof(buf), "%u s", uptime_sec);
				lv_label_set_text(uptime_label, buf);
			}
		}

		++tick;
		k_sleep(K_MSEC(10));
	}
}
