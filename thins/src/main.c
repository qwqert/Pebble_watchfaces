#include <pebble.h>

/*#define CONFIG_SHOW_SECOND*/
#define CONFIG_SHOW_TEXT

// Layout
#define MARGIN 				5
#define THICKNESS_MIN 		3
#define THICKNESS_SEC 		1
#define HAND_LENGTH_SEC 	65
#define HAND_LENGTH_MIN 	65
#define HAND_LENGTH_HOUR 	45

typedef struct {
#ifdef CONFIG_SHOW_TEXT
	int mday;
	int wday;
#endif
	int hours;
	int minutes;
#ifdef CONFIG_SHOW_SECOND
	int seconds;
#endif
} Time;

static Time s_time;
static Window *s_main_window;
static Layer *s_canvas_layer, *s_bg_layer;
#ifdef CONFIG_SHOW_TEXT
static TextLayer *s_day_in_month_layer;
static TextLayer *s_day_in_week_layer;
static TextLayer *s_battery_layer;
static char s_day_in_month_buffer[3];
static char s_battery_buffer[3];
static const char *s_day_in_week_string[] = {
    "Sun", "Mon", "Tue", "Wed", "Thr", "Fri", "Sat"
};
#endif
static bool bluetoothConnected = false;

static void bg_update_proc(Layer *layer, GContext *ctx) {
	GRect bounds = layer_get_bounds(layer);
	GPoint center = grect_center_point(&bounds);

	for(int h = 0; h < 12; h++) {
		for(int y = 0; y < THICKNESS_MIN; y++) {
			for(int x = 0; x < THICKNESS_MIN; x++) {
				GPoint point = (GPoint) {
					.x = (int16_t)(sin_lookup(TRIG_MAX_ANGLE * h / 12) * (int32_t)(3 * HAND_LENGTH_SEC) / TRIG_MAX_RATIO) + center.x,
					.y = (int16_t)(-cos_lookup(TRIG_MAX_ANGLE * h / 12) * (int32_t)(3 * HAND_LENGTH_SEC) / TRIG_MAX_RATIO) + center.y,
				};

				graphics_context_set_stroke_color(ctx, GColorWhite);
				graphics_draw_line(ctx, GPoint(center.x + x, center.y + y), GPoint(point.x + x, point.y + y));
			}
		}
	}

	// Draw minute markers
	for(int h = 0; h < 5; h++) {
		for(int y = 0; y < THICKNESS_SEC; y++) {
			for(int x = 0; x < THICKNESS_SEC; x++) {
				GPoint point = (GPoint) {
					.x = (int16_t)(sin_lookup(TRIG_MAX_ANGLE * (s_time.minutes / 5) / 12 + TRIG_MAX_ANGLE * h / 60) * \
							(int32_t)(3 * HAND_LENGTH_SEC) / TRIG_MAX_RATIO) + center.x,
					.y = (int16_t)(-cos_lookup(TRIG_MAX_ANGLE * (s_time.minutes / 5) / 12 + TRIG_MAX_ANGLE * h / 60) * \
							(int32_t)(3 * HAND_LENGTH_SEC) / TRIG_MAX_RATIO) + center.y,
				};

				graphics_context_set_stroke_color(ctx, GColorWhite);
				graphics_draw_line(ctx, GPoint(center.x + x, center.y + y), GPoint(point.x + x, point.y + y));
			}
		}
	}

	// Make markers
	graphics_context_set_fill_color(ctx, GColorBlack);
	graphics_fill_rect(ctx, GRect(MARGIN, MARGIN, bounds.size.w - (2 * MARGIN), bounds.size.h - (2 * MARGIN)), 0, GCornerNone);
}

static GPoint make_hand_point(int quantity, int intervals, int len, GPoint center) {
	return (GPoint) {
		.x = (int16_t)(sin_lookup(TRIG_MAX_ANGLE * quantity / intervals) * (int32_t)len / TRIG_MAX_RATIO) + center.x,
		.y = (int16_t)(-cos_lookup(TRIG_MAX_ANGLE * quantity / intervals) * (int32_t)len / TRIG_MAX_RATIO) + center.y,
	};
}

static void draw_proc(Layer *layer, GContext *ctx) {
	GRect bounds = layer_get_bounds(layer);
	GPoint center = grect_center_point(&bounds);  

	// Plot hand ends
#ifdef CONFIG_SHOW_SECOND
	GPoint second_hand_long = make_hand_point(s_time.seconds, 60, HAND_LENGTH_SEC, center);
	GPoint second_hand_short = make_hand_point(s_time.seconds, 60, -20, center);
#endif
	GPoint minute_hand_long = make_hand_point(s_time.minutes, 60, HAND_LENGTH_MIN, center);

	float minute_angle = TRIG_MAX_ANGLE * s_time.minutes / 60;
	float hour_angle;

	s_time.hours -= (s_time.hours > 12) ? 12 : 0;
	hour_angle = TRIG_MAX_ANGLE * s_time.hours / 12;
	hour_angle += (minute_angle / TRIG_MAX_ANGLE) * (TRIG_MAX_ANGLE / 12);

	// Hour is more accurate
	GPoint hour_hand_long = (GPoint) {
		.x = (int16_t)(sin_lookup(hour_angle) * (int32_t)HAND_LENGTH_HOUR / TRIG_MAX_RATIO) + center.x,
		.y = (int16_t)(-cos_lookup(hour_angle) * (int32_t)HAND_LENGTH_HOUR / TRIG_MAX_RATIO) + center.y,
	};

	// Draw hands
	graphics_context_set_stroke_color(ctx, GColorWhite);

	for(int y = 0; y < THICKNESS_MIN; y++) {
		for(int x = 0; x < THICKNESS_MIN; x++) {
			graphics_draw_line(ctx, GPoint(center.x + x, center.y + y), GPoint(minute_hand_long.x + x, minute_hand_long.y + y));
			graphics_draw_line(ctx, GPoint(center.x + x, center.y + y), GPoint(hour_hand_long.x + x, hour_hand_long.y + y));
		}
	}

	// Draw second hand
#ifdef CONFIG_SHOW_SECOND
	for(int y = 0; y < THICKNESS_MIN - 1; y++) {
		for(int x = 0; x < THICKNESS_MIN - 1; x++) {
			graphics_context_set_stroke_color(ctx, GColorWhite);
			graphics_draw_line(ctx, GPoint(center.x + x, center.y + y), GPoint(second_hand_short.x + x, second_hand_short.y + y));

			// Draw second hand tip
			graphics_context_set_stroke_color(ctx, GColorWhite);
			graphics_draw_line(ctx, GPoint(second_hand_short.x + x, second_hand_short.y + y), GPoint(second_hand_long.x + x, second_hand_long.y + y));
		}
	}
#endif

	// Center
	graphics_context_set_fill_color(ctx, GColorWhite);
	graphics_fill_circle(ctx, GPoint(center.x + 1, center.y + 1), 4);
	if (!bluetoothConnected) {
		graphics_context_set_fill_color(ctx, GColorBlack);
		graphics_fill_circle(ctx, GPoint(center.x + 1, center.y + 1), 2);
	}
}

void tick_handler(struct tm *tick_time, TimeUnits units_changed) {
#ifdef CONFIG_SHOW_TEXT
	s_time.mday = tick_time->tm_mday;
	s_time.wday = tick_time->tm_wday;
#endif
	s_time.hours = tick_time->tm_hour;
	s_time.minutes = tick_time->tm_min;
#ifdef CONFIG_SHOW_SECOND
	s_time.seconds = tick_time->tm_sec;
#endif

#ifdef CONFIG_SHOW_TEXT
	snprintf(s_day_in_month_buffer, sizeof(s_day_in_month_buffer), "%02d", s_time.mday);
	text_layer_set_text(s_day_in_month_layer, s_day_in_month_buffer);

	text_layer_set_text(s_day_in_week_layer, s_day_in_week_string[s_time.wday]);
#endif

	layer_mark_dirty(s_canvas_layer);
}

#ifdef CONFIG_SHOW_TEXT
void battery_state_handler(BatteryChargeState charge) {
	if (charge.charge_percent == 100)
		strcpy(s_battery_buffer, "FU");
	else
		snprintf(s_battery_buffer, sizeof(s_battery_buffer), "%02d", charge.charge_percent);
	text_layer_set_text(s_battery_layer, s_battery_buffer);
}
#endif

void bluetooth_connection_handler(bool connected) {
    bluetoothConnected = connected;
	if (!bluetoothConnected) {
		vibes_cancel();
		vibes_short_pulse();
	}
	layer_mark_dirty(s_canvas_layer);
}

static void main_window_load(Window *window)
{
	Layer *window_layer = window_get_root_layer(window);
	GRect bounds = layer_get_bounds(window_layer);

	s_bg_layer = layer_create(bounds);
	layer_set_update_proc(s_bg_layer, bg_update_proc);
	layer_add_child(window_layer, s_bg_layer);

#ifdef CONFIG_SHOW_TEXT
	s_day_in_month_layer = text_layer_create(GRect(90, 51, 44, 40));
	text_layer_set_text_alignment(s_day_in_month_layer, GTextAlignmentCenter);
	text_layer_set_font(s_day_in_month_layer, fonts_get_system_font(FONT_KEY_GOTHIC_24));
	text_layer_set_text_color(s_day_in_month_layer, GColorWhite);
	text_layer_set_background_color(s_day_in_month_layer, GColorClear);
	layer_add_child(window_layer, text_layer_get_layer(s_day_in_month_layer));

	s_day_in_week_layer = text_layer_create(GRect(90, 76, 44, 40));
	text_layer_set_text_alignment(s_day_in_week_layer, GTextAlignmentCenter);
	text_layer_set_font(s_day_in_week_layer, fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD));
	text_layer_set_text_color(s_day_in_week_layer, GColorWhite);
	text_layer_set_background_color(s_day_in_week_layer,  GColorClear);
	layer_add_child(window_layer, text_layer_get_layer(s_day_in_week_layer));

	s_battery_layer = text_layer_create(GRect(90, 86, 44, 40));
	text_layer_set_text_alignment(s_battery_layer, GTextAlignmentCenter);
	text_layer_set_font(s_battery_layer, fonts_get_system_font(FONT_KEY_GOTHIC_24));
	text_layer_set_text_color(s_battery_layer, GColorWhite);
	text_layer_set_background_color(s_battery_layer, GColorClear);
	layer_add_child(window_layer, text_layer_get_layer(s_battery_layer));
#endif

	s_canvas_layer = layer_create(bounds);
	layer_set_update_proc(s_canvas_layer, draw_proc);
	layer_add_child(window_layer, s_canvas_layer);

#ifdef CONFIG_SHOW_TEXT
	snprintf(s_day_in_month_buffer, sizeof(s_day_in_month_buffer), "%02d", s_time.mday);
	text_layer_set_text(s_day_in_month_layer, s_day_in_month_buffer);

	text_layer_set_text(s_day_in_week_layer, s_day_in_week_string[s_time.wday]);

    BatteryChargeState initial = battery_state_service_peek();
	if (initial.charge_percent == 100)
		strcpy(s_battery_buffer, "FU");
	else
		snprintf(s_battery_buffer, sizeof(s_battery_buffer), "%02d", initial.charge_percent);
	text_layer_set_text(s_battery_layer, s_battery_buffer);
#endif
}

static void main_window_unload(Window *window) {
#ifdef CONFIG_SHOW_TEXT
	text_layer_destroy(s_battery_layer);
	text_layer_destroy(s_day_in_month_layer);
	text_layer_destroy(s_day_in_week_layer);
#endif

	layer_destroy(s_canvas_layer);
	layer_destroy(s_bg_layer);
}

static void init() {
	time_t t = time(NULL);
	struct tm *tm_now = localtime(&t);
	s_time.hours = tm_now->tm_hour;
	s_time.minutes = tm_now->tm_min;
#ifdef CONFIG_SHOW_SECOND
	s_time.seconds = tm_now->tm_sec;  
#endif
#ifdef CONFIG_SHOW_TEXT
	s_time.mday = tm_now->tm_mday;
	s_time.wday = tm_now->tm_wday;
#endif

	// Window
	s_main_window = window_create();
	window_set_window_handlers(s_main_window, (WindowHandlers) {
		.load = main_window_load,
		.unload = main_window_unload
	});
	window_stack_push(s_main_window, true /* Animated */);
	window_set_background_color(s_main_window, GColorBlack);

#ifdef CONFIG_SHOW_SECOND
	tick_timer_service_subscribe(SECOND_UNIT, tick_handler);
#else
	tick_timer_service_subscribe(MINUTE_UNIT, tick_handler);
#endif
#ifdef CONFIG_SHOW_TEXT
	battery_state_service_subscribe(&battery_state_handler);
#endif
	bluetooth_connection_service_subscribe(&bluetooth_connection_handler);
    bluetoothConnected = bluetooth_connection_service_peek();
}

/*
 * deinit
 */
static void deinit() {
	bluetooth_connection_service_unsubscribe();
	battery_state_service_unsubscribe();
    tick_timer_service_unsubscribe();
    window_destroy(s_main_window);
}

/*
 * Main - or main as it is known
 */
int main(void) {
    init();
    app_event_loop();
    deinit();
}
