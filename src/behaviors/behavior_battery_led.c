#define DT_DRV_COMPAT zmk_behavior_battery_led

#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/util.h>
#include <errno.h>
#include <stdbool.h>

#include <drivers/behavior.h>
#include <zmk/behavior.h>
#include <zmk/battery.h>
#include <zmk/event_manager.h>
#include <zmk/events/hid_indicators_changed.h>

/*
 * HoshinoUtsuro Battery / Status LED behavior
 *
 * XIAO nRF52840 onboard RGB USER LED:
 *   led0 / Red   = P0.26 GPIO_ACTIVE_LOW
 *   led1 / Green = P0.30 GPIO_ACTIVE_LOW
 *   led2 / Blue  = P0.06 GPIO_ACTIVE_LOW
 *
 * gpio_pin_set_dt() respects GPIO_ACTIVE_LOW:
 *   logical 1 = ON
 *   logical 0 = OFF
 */

#define LED_MODE_BATTERY     0
#define LED_MODE_CHECK       1
#define LED_MODE_DRAW        2
#define LED_MODE_DEFAULT     3
#define LED_MODE_KANA        4
#define LED_MODE_EISU        5
#define LED_MODE_BLE_SELECT  6
#define LED_MODE_BLE_CLEAR   7
#define LED_MODE_NUMLOCK_ARM 8

/*
 * HID LED report:
 * bit 0 = NumLock
 */
#define HID_INDICATOR_NUMLOCK_BIT BIT(0)

enum led_color {
    LED_COLOR_OFF,
    LED_COLOR_RED,
    LED_COLOR_GREEN,
    LED_COLOR_BLUE,
    LED_COLOR_YELLOW,
    LED_COLOR_CYAN,
    LED_COLOR_PURPLE,
    LED_COLOR_WHITE,
};

enum led_pattern {
    LED_PATTERN_NONE,
    LED_PATTERN_BATTERY_BLINK,
    LED_PATTERN_CHECK,
    LED_PATTERN_TRIPLE,
    LED_PATTERN_SINGLE,
};

static const struct gpio_dt_spec led_red =
    GPIO_DT_SPEC_GET(DT_NODELABEL(led0), gpios);

static const struct gpio_dt_spec led_green =
    GPIO_DT_SPEC_GET(DT_NODELABEL(led1), gpios);

static const struct gpio_dt_spec led_blue =
    GPIO_DT_SPEC_GET(DT_NODELABEL(led2), gpios);

static struct k_work_delayable led_work;

static enum led_pattern current_pattern = LED_PATTERN_NONE;
static enum led_color current_color = LED_COLOR_OFF;
static int step = 0;


/*
 * GHAでNumLockを押す直前にtrueにする。
 *
 * NumLockを送った時点ではON/OFFを推測せず、
 * WindowsからHID Indicatorが返ってくるのを待つ。
 */
static bool numlock_feedback_pending = false;

static bool numlock_is_on = false;


static void show_color(enum led_color color) {

    gpio_pin_set_dt(&led_red, 0);
    gpio_pin_set_dt(&led_green, 0);
    gpio_pin_set_dt(&led_blue, 0);

    switch (color) {

    case LED_COLOR_RED:
        gpio_pin_set_dt(&led_red, 1);
        break;

    case LED_COLOR_GREEN:
        gpio_pin_set_dt(&led_green, 1);
        break;

    case LED_COLOR_BLUE:
        gpio_pin_set_dt(&led_blue, 1);
        break;

    case LED_COLOR_YELLOW:
        gpio_pin_set_dt(&led_red, 1);
        gpio_pin_set_dt(&led_green, 1);
        break;

    case LED_COLOR_CYAN:
        gpio_pin_set_dt(&led_green, 1);
        gpio_pin_set_dt(&led_blue, 1);
        break;

    case LED_COLOR_PURPLE:
        gpio_pin_set_dt(&led_red, 1);
        gpio_pin_set_dt(&led_blue, 1);
        break;

    case LED_COLOR_WHITE:
        gpio_pin_set_dt(&led_red, 1);
        gpio_pin_set_dt(&led_green, 1);
        gpio_pin_set_dt(&led_blue, 1);
        break;

    case LED_COLOR_OFF:
    default:
        break;
    }
}


static enum led_color battery_color_from_percent(uint8_t percent) {

    if (percent >= 90) {
        return LED_COLOR_BLUE;

    } else if (percent >= 50) {
        return LED_COLOR_GREEN;

    } else if (percent >= 20) {
        return LED_COLOR_YELLOW;

    } else {
        return LED_COLOR_RED;
    }
}


static void led_work_handler(struct k_work *work) {

    switch (current_pattern) {

    case LED_PATTERN_BATTERY_BLINK:

        /*
         * 約3秒
         * 250ms x 12
         */
        if (step >= 12) {
            show_color(LED_COLOR_OFF);
            current_pattern = LED_PATTERN_NONE;
            return;
        }

        if ((step % 2) == 0) {
            show_color(current_color);
        } else {
            show_color(LED_COLOR_OFF);
        }

        step++;
        k_work_schedule(&led_work, K_MSEC(250));
        break;


    case LED_PATTERN_CHECK:

        switch (step) {

        case 0:
            show_color(LED_COLOR_WHITE);
            break;

        case 1:
            show_color(LED_COLOR_RED);
            break;

        case 2:
            show_color(LED_COLOR_GREEN);
            break;

        case 3:
            show_color(LED_COLOR_BLUE);
            break;

        case 4:
            show_color(LED_COLOR_YELLOW);
            break;

        case 5:
            show_color(LED_COLOR_CYAN);
            break;

        case 6:
            show_color(LED_COLOR_PURPLE);
            break;

        default:
            show_color(LED_COLOR_OFF);
            current_pattern = LED_PATTERN_NONE;
            return;
        }

        step++;
        k_work_schedule(&led_work, K_MSEC(500));
        break;


    case LED_PATTERN_TRIPLE:

        /*
         * お絵描きモードと同じ3回点滅
         *
         * ON / OFF x 3
         * 各120ms
         */
        if (step >= 6) {
            show_color(LED_COLOR_OFF);
            current_pattern = LED_PATTERN_NONE;
            return;
        }

        if ((step % 2) == 0) {
            show_color(current_color);
        } else {
            show_color(LED_COLOR_OFF);
        }

        step++;
        k_work_schedule(&led_work, K_MSEC(120));
        break;


    case LED_PATTERN_SINGLE:

        if (step == 0) {

            show_color(current_color);

            step++;

            k_work_schedule(&led_work, K_MSEC(350));

        } else {

            show_color(LED_COLOR_OFF);
            current_pattern = LED_PATTERN_NONE;
        }

        break;


    case LED_PATTERN_NONE:
    default:

        show_color(LED_COLOR_OFF);
        current_pattern = LED_PATTERN_NONE;
        return;
    }
}


static void start_pattern(
    enum led_pattern pattern,
    enum led_color color
) {

    current_pattern = pattern;
    current_color = color;
    step = 0;

    k_work_cancel_delayable(&led_work);

    k_work_schedule(&led_work, K_NO_WAIT);
}


static int on_keymap_binding_pressed(
    struct zmk_behavior_binding *binding,
    struct zmk_behavior_binding_event event
) {

    switch (binding->param1) {

    /*
     * GHV
     */
    case LED_MODE_BATTERY: {

        uint8_t percent =
            zmk_battery_state_of_charge();

        start_pattern(
            LED_PATTERN_BATTERY_BLINK,
            battery_color_from_percent(percent)
        );

        break;
    }


    /*
     * GHC
     */
    case LED_MODE_CHECK:

        start_pattern(
            LED_PATTERN_CHECK,
            LED_COLOR_WHITE
        );

        break;


    /*
     * GHB
     */
    case LED_MODE_DRAW:

        start_pattern(
            LED_PATTERN_TRIPLE,
            LED_COLOR_WHITE
        );

        break;


    /*
     * GHN
     */
    case LED_MODE_DEFAULT:

        start_pattern(
            LED_PATTERN_TRIPLE,
            LED_COLOR_GREEN
        );

        break;


    /*
     * かな
     */
    case LED_MODE_KANA:

        start_pattern(
            LED_PATTERN_SINGLE,
            LED_COLOR_GREEN
        );

        break;


    /*
     * 英数
     */
    case LED_MODE_EISU:

        start_pattern(
            LED_PATTERN_SINGLE,
            LED_COLOR_BLUE
        );

        break;


    /*
     * BLE0～4
     *
     * 青3回
     */
    case LED_MODE_BLE_SELECT:

        start_pattern(
            LED_PATTERN_TRIPLE,
            LED_COLOR_BLUE
        );

        break;


    /*
     * GHQ
     *
     * 赤3回
     */
    case LED_MODE_BLE_CLEAR:

        start_pattern(
            LED_PATTERN_TRIPLE,
            LED_COLOR_RED
        );

        break;


    /*
     * GHA NumLock
     *
     * この時点ではLEDを光らせない。
     *
     * 実際のNumLock状態が
     * Windowsから返ってくるまで待つ。
     */
    case LED_MODE_NUMLOCK_ARM:

        numlock_feedback_pending = true;

        break;


    default:

        start_pattern(
            LED_PATTERN_SINGLE,
            LED_COLOR_RED
        );

        break;
    }

    return ZMK_BEHAVIOR_OPAQUE;
}


static int on_keymap_binding_released(
    struct zmk_behavior_binding *binding,
    struct zmk_behavior_binding_event event
) {

    return ZMK_BEHAVIOR_OPAQUE;
}


/*
 * ------------------------------------------------------------
 * NumLock HID Indicator
 * ------------------------------------------------------------
 *
 * Windowsなどのホストから返ってきた
 * 実際のNumLock状態を受信する。
 *
 * NumLock ON
 *   → 赤3回
 *
 * NumLock OFF
 *   → 緑3回
 *
 * GHAからNumLockを送った時だけLEDを表示する。
 *
 * BLEプロファイル変更でも
 * HID Indicatorイベントは発生するため、
 * そのイベントで青BLE表示を潰さないため。
 */
static int hoshino_hid_indicators_listener(
    const zmk_event_t *eh
) {

    const struct zmk_hid_indicators_changed *event =
        as_zmk_hid_indicators_changed(eh);

    if (event == NULL) {
        return ZMK_EV_EVENT_BUBBLE;
    }


    /*
     * bit0 = NumLock
     */
    numlock_is_on =
        (event->indicators &
         HID_INDICATOR_NUMLOCK_BIT) != 0;


    /*
     * GHAでNumLockを押した直後だけ
     * 結果をLED表示する。
     */
    if (numlock_feedback_pending) {

        numlock_feedback_pending = false;


        if (numlock_is_on) {

            /*
             * NumLock ON
             *
             * 赤3回
             */
            start_pattern(
                LED_PATTERN_TRIPLE,
                LED_COLOR_RED
            );

        } else {

            /*
             * NumLock OFF
             *
             * 緑3回
             */
            start_pattern(
                LED_PATTERN_TRIPLE,
                LED_COLOR_GREEN
            );
        }
    }


    return ZMK_EV_EVENT_BUBBLE;
}


ZMK_LISTENER(
    hoshino_hid_indicators,
    hoshino_hid_indicators_listener
);


ZMK_SUBSCRIPTION(
    hoshino_hid_indicators,
    zmk_hid_indicators_changed
);


/*
 * ------------------------------------------------------------
 * Init
 * ------------------------------------------------------------
 */
static int behavior_battery_led_init(
    const struct device *dev
) {

    if (
        !device_is_ready(led_red.port) ||
        !device_is_ready(led_green.port) ||
        !device_is_ready(led_blue.port)
    ) {

        return -ENODEV;
    }


    gpio_pin_configure_dt(
        &led_red,
        GPIO_OUTPUT_INACTIVE
    );

    gpio_pin_configure_dt(
        &led_green,
        GPIO_OUTPUT_INACTIVE
    );

    gpio_pin_configure_dt(
        &led_blue,
        GPIO_OUTPUT_INACTIVE
    );


    show_color(
        LED_COLOR_OFF
    );


    k_work_init_delayable(
        &led_work,
        led_work_handler
    );


    return 0;
}


static const struct behavior_driver_api
behavior_battery_led_driver_api = {

    .binding_pressed =
        on_keymap_binding_pressed,

    .binding_released =
        on_keymap_binding_released,
};


#define BATTERY_LED_INST(n)                                           \
    BEHAVIOR_DT_INST_DEFINE(                                          \
        n,                                                            \
        behavior_battery_led_init,                                    \
        NULL,                                                         \
        NULL,                                                         \
        NULL,                                                         \
        POST_KERNEL,                                                  \
        CONFIG_KERNEL_INIT_PRIORITY_DEFAULT,                          \
        &behavior_battery_led_driver_api                              \
    );


DT_INST_FOREACH_STATUS_OKAY(
    BATTERY_LED_INST
)
