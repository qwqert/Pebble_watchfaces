#include <pebble.h>

static Window *window;

// Date and time
#define TIME_FONT RESOURCE_ID_FONT_DIGITAL_SEVEN_66
#define DATE_FONT RESOURCE_ID_FONT_DIGITAL_SEVEN_16
#define TIME_24H_FORMAT "%H:%M"
#define TIME_12H_FORMAT "%I:%M"
#define DATE_FORMAT     "%d/%m/%Y"
static GFont time_font;
static GFont date_font;
static TextLayer *date_layer;
static TextLayer *time_layer;

// Battery and bluetooth
#define BATTERY_DURATION_FORMAT "%dD%02dH"
#define BATTERY_CHARGING_FORMAT "%dH%02dM"
static Layer *battery_layer;

static GPoint battery_dot_points[10];
static Layer *bluetooth_layer;
static TextLayer *battery_duration_layer;
static GBitmap *icon_bluetooth_linked;
static GBitmap *icon_bluetooth_unlinked;
static uint8_t battery_level;
static bool battery_plugged;
static bool bluetoothConnected = false;

// persistent storage
#define LAST_CHARGE_PKEY 0xd3943c7a
static time_t last_charge = 0;  // unit: second
static int battery_duration = 0;    // unit: minute

// Calendar
#define CALENDAR_FONT FONT_KEY_GOTHIC_14
#define CALENDAR_LAYER_HEIGHT  48
#define CALENDAR_CELL_WIDTH    20
#define CALENDAR_CELL_HEIGHT   15
#define CALENDAR_CELL_GAP       2
static Layer *calendar_layer;
static const char *strDaysOfWeek[] = {
    "Su", "Mo", "Tu", "We", "Th", "Fr", "Sa"
};

// How many days are/were in the month
int days_in_month(int mon, int year) {
    mon++; // dec = 0|12, lazily optimized
    switch (mon) {
        // April, June, September and November have 30 Days
        case 4:
        case 6:
        case 9:
        case 11:
            return 30;
        // Deal with Feburary & Leap years
        case 2:
            if (year % 400 == 0) {
                return 29;
            } else if (year % 100 == 0) {
                return 28;
            } else if (year % 4 == 0) {
                return 29;
            } else {
                return 28;
            }
        // Most months have 31 days
        default:
            return 31;
    }
}

void get_calendar(int calendar[14], struct tm *current_time)
{
    int mon = current_time->tm_mon;
    int year = current_time->tm_year + 1900;
    int daysThisMonth = days_in_month(mon, year);
    int i, cellNum = 0;   // address for current day table cell: 0-20
    int daysVisPrevMonth = 0;
    int daysVisNextMonth = 0;
    int daysPriorToToday = current_time->tm_wday; // just instantiating, not final value
    int daysAfterToday   = (6 - current_time->tm_wday) % 7 + 7; // just instantiating, not final value

    if (daysPriorToToday >= current_time->tm_mday) {
        // We're showing more days before today than exist this month
        int daysInPrevMonth = days_in_month(mon - 1,year); // year only matters for February, which will be the same 'from' March

        // Number of days we'll show from the previous month
        daysVisPrevMonth = daysPriorToToday - current_time->tm_mday + 1;

        for (i = 0; i < daysVisPrevMonth; i++, cellNum++) {
            calendar[cellNum] = daysInPrevMonth + i - daysVisPrevMonth + 1;
        }
    }

    // optimization: instantiate i to a hot mess, since the first day we show this month may not be the 1st of the month
    int firstDayShownThisMonth = daysVisPrevMonth + current_time->tm_mday - daysPriorToToday;
    for (i = firstDayShownThisMonth; i < current_time->tm_mday; i++, cellNum++) {
        calendar[cellNum] = i;
    }

    // the current day... we'll style this special
    calendar[cellNum] = current_time->tm_mday;
    cellNum++;

    if (current_time->tm_mday + daysAfterToday > daysThisMonth) {
        daysVisNextMonth = current_time->tm_mday + daysAfterToday - daysThisMonth;
    }

    // add the days after today until the end of the month/next week, to our array...
    int daysLeftThisMonth = daysAfterToday - daysVisNextMonth;
    for (i = 0; i < daysLeftThisMonth; i++, cellNum++) {
        calendar[cellNum] = i + current_time->tm_mday + 1;
    }

    // add any days in the next month to our array...
    for (i = 0; i < daysVisNextMonth; i++, cellNum++) {
        calendar[cellNum] = i + 1;
    }
}

void calendar_layer_update(Layer *me, GContext* ctx) {
    time_t now = time(NULL);
    struct tm *current_time = localtime(&now);
    int calendar[14];
    char calendar_text[] = "00.";
    GRect bounds = layer_get_bounds(me);
    GRect current_bounds = GRect(
        bounds.origin.x + CALENDAR_CELL_GAP +
        CALENDAR_CELL_WIDTH * current_time->tm_wday,
        bounds.origin.y, CALENDAR_CELL_WIDTH, bounds.size.h
    );

    graphics_context_set_fill_color(ctx, GColorWhite);
    graphics_context_set_stroke_color(ctx, GColorWhite);
    graphics_fill_rect(ctx, bounds, 0, GCornerNone);

    graphics_context_set_fill_color(ctx, GColorBlack);
    graphics_context_set_stroke_color(ctx, GColorBlack);
    graphics_fill_rect(ctx, current_bounds, 0, GCornerNone);

    get_calendar(calendar, current_time);
    for (int i = 0; i < 7; i++) {
        if (i == current_time->tm_wday) {
            graphics_context_set_stroke_color(ctx, GColorWhite);
            graphics_context_set_fill_color(ctx, GColorWhite);
            graphics_context_set_text_color(ctx, GColorWhite);
        }
        else {
            graphics_context_set_stroke_color(ctx, GColorBlack);
            graphics_context_set_fill_color(ctx, GColorBlack);
            graphics_context_set_text_color(ctx, GColorBlack);
        }

        graphics_draw_text(ctx, strDaysOfWeek[i],
            fonts_get_system_font(FONT_KEY_GOTHIC_14),
            GRect(bounds.origin.x + CALENDAR_CELL_GAP +
                  CALENDAR_CELL_WIDTH * i, bounds.origin.y,
                  CALENDAR_CELL_WIDTH, CALENDAR_CELL_HEIGHT),
            GTextOverflowModeWordWrap,
            GTextAlignmentCenter, NULL
        );

        snprintf(calendar_text, sizeof(calendar_text), "%2d", calendar[i]);
        graphics_draw_text(ctx, calendar_text,
            fonts_get_system_font(FONT_KEY_GOTHIC_14),
            GRect(bounds.origin.x + CALENDAR_CELL_GAP + CALENDAR_CELL_WIDTH * i,
                  bounds.origin.y + CALENDAR_CELL_HEIGHT,
                  CALENDAR_CELL_WIDTH, CALENDAR_CELL_HEIGHT),
            GTextOverflowModeWordWrap,
            GTextAlignmentCenter, NULL
        );

        snprintf(calendar_text, sizeof(calendar_text), "%2d", calendar[i+7]);
        graphics_draw_text(ctx, calendar_text,
            fonts_get_system_font(FONT_KEY_GOTHIC_14),
            GRect(bounds.origin.x + CALENDAR_CELL_GAP + CALENDAR_CELL_WIDTH * i,
                  bounds.origin.y + CALENDAR_CELL_HEIGHT * 2,
                  CALENDAR_CELL_WIDTH, CALENDAR_CELL_HEIGHT),
            GTextOverflowModeWordWrap,
            GTextAlignmentCenter, NULL
        );
    }
}

void draw_date(struct tm *t) {
    static char date_text[] = "1990/10/14";

    strftime(date_text, sizeof(date_text), DATE_FORMAT, t);
    text_layer_set_text(date_layer, date_text);
}

void draw_time(struct tm *t) {
    static char time_text[] = "00:00";

	if (clock_is_24h_style()) {
		strftime(time_text, sizeof(time_text), TIME_24H_FORMAT, t);
	}
	else {
		strftime(time_text, sizeof(time_text), TIME_12H_FORMAT, t);
	}
    text_layer_set_text(time_layer, time_text);
}

void draw_battery_duration(int duration_min)
{
	static char battdur_text[] = "12D12H"; //battery duration

	if (!battery_plugged) { 
		int duration_hour = duration_min / 60;
		snprintf(battdur_text, sizeof(battdur_text),
			BATTERY_DURATION_FORMAT, duration_hour / 24, duration_hour % 24);
	} else { //charging
		snprintf(battdur_text, sizeof(battdur_text),
			BATTERY_CHARGING_FORMAT, duration_min / 60, duration_min % 60);
	}
	text_layer_set_text(battery_duration_layer, battdur_text);
}

/*
 * Battery icon callback handler
 */
void battery_layer_update_callback(Layer *layer, GContext *ctx) {
    graphics_context_set_fill_color(ctx, GColorWhite);
    graphics_context_set_stroke_color(ctx, GColorWhite);

    for (unsigned int i = 0; i < battery_level / 10; i++) {
        graphics_fill_circle(ctx, battery_dot_points[i], 3);
    }

    for (unsigned int i = battery_level / 10; i < 10; i++) {
        graphics_draw_circle(ctx, battery_dot_points[i], 3);
    }
}

void battery_state_handler(BatteryChargeState charge) {
    static bool last_battery_plugged;
    battery_level = charge.charge_percent;
    last_battery_plugged = battery_plugged;
    battery_plugged = charge.is_plugged;

    layer_mark_dirty(battery_layer);

	// start or stop charging
    if (last_battery_plugged ^ battery_plugged) {
        time_t now = time(NULL);
        last_charge = now;
        persist_write_int(LAST_CHARGE_PKEY, last_charge);
        battery_duration = 0;
        draw_battery_duration(battery_duration);
    }
}

/*
 * Bluetooth icon callback handler
 */
void bluetooth_layer_update_callback(Layer *layer, GContext *ctx) {
	graphics_context_set_compositing_mode(ctx, GCompOpAssign);
	if (bluetoothConnected) {
		graphics_draw_bitmap_in_rect(ctx, icon_bluetooth_linked, GRect(0, 0, 14, 12));
	}
	else {
		graphics_draw_bitmap_in_rect(ctx, icon_bluetooth_unlinked, GRect(0, 0, 14, 12));
	}
}

void bluetooth_connection_handler(bool connected) {
	static char vibrate = false;
	bluetoothConnected = connected;
	if (bluetoothConnected) {
		vibrate = false;
	}
	else if (!vibrate) {
		vibes_cancel();
		vibes_short_pulse();
		vibrate = true;
	}
	layer_mark_dirty(bluetooth_layer);
}

void tick_handler(struct tm *tick_time, TimeUnits units_changed) {
	if (units_changed & DAY_UNIT) {
		draw_date(tick_time);
		layer_mark_dirty(calendar_layer);
	}

	if (units_changed & MINUTE_UNIT) {
		time_t now = time(NULL);
		battery_duration = (int)(now - last_charge) / 60;
		draw_battery_duration(battery_duration);
		draw_time(tick_time);
	}
}

static void main_window_load(Window *window)
{
    time_t now = time(NULL);
    struct tm *current_time = localtime(&now);
    Layer *window_layer = window_get_root_layer(window);
    GRect window_bounds = layer_get_bounds(window_layer);

    // Fonts
    time_font = fonts_load_custom_font(resource_get_handle(TIME_FONT));
    date_font = fonts_load_custom_font(resource_get_handle(DATE_FONT));

    // Digital time
    time_layer = text_layer_create(GRect(0, 28, window_bounds.size.w, 70));
    text_layer_set_text_color(time_layer, GColorWhite);
    text_layer_set_text_alignment(time_layer, GTextAlignmentCenter);
    text_layer_set_background_color(time_layer, GColorClear);
    text_layer_set_font(time_layer, time_font);
    layer_add_child(window_layer, text_layer_get_layer(time_layer));

    // Date
    date_layer = text_layer_create(GRect(8,
        window_bounds.size.h - CALENDAR_LAYER_HEIGHT - 25,
        window_bounds.size.w * 2 / 3, 25));
    text_layer_set_text_color(date_layer, GColorWhite);
    text_layer_set_text_alignment(date_layer, GTextAlignmentLeft);
    text_layer_set_background_color(date_layer, GColorClear);
    text_layer_set_font(date_layer, date_font);
    layer_add_child(window_layer, text_layer_get_layer(date_layer));

    // Battery 
	BatteryChargeState initial = battery_state_service_peek();
	battery_level = initial.charge_percent;
	battery_plugged = initial.is_plugged;
	battery_layer = layer_create(GRect(0, 0, window_bounds.size.w - 28, 20)); //24*12
    for (unsigned int i = 0; i < 10; i++) {
        battery_dot_points[i] = GPoint (
			(i + 1) * layer_get_bounds(battery_layer).size.w / 11,
			layer_get_bounds(battery_layer).size.h / 2
        );
    }
	layer_set_update_proc(battery_layer, &battery_layer_update_callback);
	layer_add_child(window_layer, battery_layer);

    // Battery duration
	if (persist_exists(LAST_CHARGE_PKEY)) {
		last_charge = persist_read_int(LAST_CHARGE_PKEY);
	} else {
		last_charge = now;
		persist_write_int(LAST_CHARGE_PKEY, last_charge);
	}
	battery_duration = (int)(now - last_charge) / 60;
	battery_duration_layer = text_layer_create(GRect(window_bounds.size.w * 2 / 3,
		window_bounds.size.h - CALENDAR_LAYER_HEIGHT - 25, window_bounds.size.w / 3, 25));
	text_layer_set_text_color(battery_duration_layer, GColorWhite);
	text_layer_set_text_alignment(battery_duration_layer, GTextAlignmentLeft);
	text_layer_set_background_color(battery_duration_layer, GColorClear);
	text_layer_set_font(battery_duration_layer, date_font);
	layer_add_child(window_layer, text_layer_get_layer(battery_duration_layer));
	draw_battery_duration(battery_duration);

    // Bluetooth
	icon_bluetooth_linked = gbitmap_create_with_resource(RESOURCE_ID_BLUETOOTH_LINKED);
	icon_bluetooth_unlinked = gbitmap_create_with_resource(RESOURCE_ID_BLUETOOTH_UNLINKED);
	bluetoothConnected = bluetooth_connection_service_peek();
	bluetooth_layer = layer_create(GRect(window_bounds.size.w - 24, 4, 14, 12)); //14*12
	layer_set_update_proc(bluetooth_layer, &bluetooth_layer_update_callback);
	layer_add_child(window_layer, bluetooth_layer);

    // Calendar
    calendar_layer = layer_create(
        GRect(0, window_bounds.size.h - CALENDAR_LAYER_HEIGHT,
              window_bounds.size.w, CALENDAR_LAYER_HEIGHT)
    );
    layer_set_update_proc(calendar_layer, &calendar_layer_update);
    layer_add_child(window_layer, calendar_layer);

    draw_time(current_time);
    draw_date(current_time);
}

static void main_window_unload(Window *window) {
	gbitmap_destroy(icon_bluetooth_linked);
	gbitmap_destroy(icon_bluetooth_unlinked);
    text_layer_destroy(date_layer);
    text_layer_destroy(time_layer);
	text_layer_destroy(battery_duration_layer);
    layer_destroy(calendar_layer);
	layer_destroy(battery_layer);
	layer_destroy(bluetooth_layer);
    fonts_unload_custom_font(time_font);
    fonts_unload_custom_font(date_font);
}

static void init() {

    // Window
    window = window_create();
    window_set_window_handlers(window, (WindowHandlers) {
        .load = main_window_load,
        .unload = main_window_unload
    });
    window_stack_push(window, true /* Animated */);
    window_set_background_color(window, GColorBlack);

    tick_timer_service_subscribe(MINUTE_UNIT | HOUR_UNIT | DAY_UNIT, &tick_handler);
    battery_state_service_subscribe(&battery_state_handler);
    bluetooth_connection_service_subscribe(&bluetooth_connection_handler);
}

/*
 * deinit
 */
static void deinit() {
    bluetooth_connection_service_unsubscribe();
    battery_state_service_unsubscribe();
    tick_timer_service_unsubscribe();
    window_destroy(window);
}

/*
 * Main - or main as it is known
 */
int main(void) {
    init();
    app_event_loop();
    deinit();
}
