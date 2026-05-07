#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>

#include <zmk/battery.h>
#include <zmk/behavior.h>
#include <zmk/behavior_queue.h>
#include <zmk/keymap.h>

#define DT_DRV_COMPAT zmk_behavior_battery_led

/*
 * XIAO nRF52840 onboard RGB USER LED
 *
 * These LEDs are active-low:
 *   gpio_pin_set_dt(..., 0) = ON
 *   gpio_pin_set_dt(..., 1) = OFF
 */
static const struct gpio_dt_spec led_red = GPIO_DT_SPEC_GET(DT_NODELABEL(led0), gpios);
static const struct gpio_dt_spec led_green = GPIO_DT_SPEC_GET(DT_NODELABEL(led1), gpios);
static const struct gpio_dt_spec led_blue = GPIO_DT_SPEC_GET(DT_NODELABEL(led2), gpios);

static struct k_work_delayable battery_led_work;
static int blink_count;
static bool blink_on;

enum battery_led_color {
    BATTERY_LED_RED,
    BATTERY_LED_GREEN,
    BATTERY_LED_BLUE,
    BATTERY_LED_YELLOW,
};

static enum battery_led_color current_color = BATTERY_LED_RED;

static void battery_led_all_off(void) {
    gpio_pin_set_dt(&led_red, 1);
    gpio_pin_set_dt(&led_green, 1);
    gpio_pin_set_dt(&led_blue, 1);
}

static void battery_led_show_color(enum battery_led_color color) {
    battery_led_all_off();

    switch (color) {
    case BATTERY_LED_BLUE:
        gpio_pin_set_dt(&led_blue, 0);
        break;
    case BATTERY_LED_GREEN:
        gpio_pin_set_dt(&led_green, 0);
        break;
    case BATTERY_LED_YELLOW:
        gpio_pin_set_dt(&led_red, 0);
        gpio_pin_set_dt(&led_green, 0);
        break;
    case BATTERY_LED_RED:
    default:
        gpio_pin_set_dt(&led_red, 0);
        break;
    }
}

static enum battery_led_color battery_led_color_from_percent(uint8_t percent) {
    if (percent >= 90) {
        return BATTERY_LED_BLUE;
    } else if (percent >= 50) {
        return BATTERY_LED_GREEN;
    } else if (percent >= 20) {
        return BATTERY_LED_YELLOW;
    } else {
        return BATTERY_LED_RED;
    }
}

static void battery_led_blink_work_handler(struct k_work *work) {
    if (blink_count <= 0) {
        battery_led_all_off();
        return;
    }

    if (blink_on) {
        battery_led_all_off();
    } else {
        battery_led_show_color(current_color);
    }

    blink_on = !blink_on;
    blink_count--;

    k_work_schedule(&battery_led_work, K_MSEC(250));
}

static int battery_led_start_blink(void) {
    uint8_t percent = zmk_battery_state_of_charge();

    current_color = battery_led_color_from_percent(percent);

    blink_count = 12; /* 250ms x 12 = about 3 seconds */
    blink_on = false;

    k_work_cancel_delayable(&battery_led_work);
    k_work_schedule(&battery_led_work, K_NO_WAIT);

    return ZMK_BEHAVIOR_OPAQUE;
}

static int on_keymap_binding_pressed(struct zmk_behavior_binding *binding,
                                     struct zmk_behavior_binding_event event) {
    return battery_led_start_blink();
}

static int on_keymap_binding_released(struct zmk_behavior_binding *binding,
                                      struct zmk_behavior_binding_event event) {
    return ZMK_BEHAVIOR_OPAQUE;
}

static int behavior_battery_led_init(const struct device *dev) {
    if (!device_is_ready(led_red.port) ||
        !device_is_ready(led_green.port) ||
        !device_is_ready(led_blue.port)) {
        return -ENODEV;
    }

    gpio_pin_configure_dt(&led_red, GPIO_OUTPUT_INACTIVE);
    gpio_pin_configure_dt(&led_green, GPIO_OUTPUT_INACTIVE);
    gpio_pin_configure_dt(&led_blue, GPIO_OUTPUT_INACTIVE);

    battery_led_all_off();

    k_work_init_delayable(&battery_led_work, battery_led_blink_work_handler);

    return 0;
}

static const struct behavior_driver_api behavior_battery_led_driver_api = {
    .binding_pressed = on_keymap_binding_pressed,
    .binding_released = on_keymap_binding_released,
};

#define BATTERY_LED_INST(n)                                                                    \
    BEHAVIOR_DT_INST_DEFINE(n, behavior_battery_led_init, NULL, NULL, NULL, POST_KERNEL,        \
                            CONFIG_KERNEL_INIT_PRIORITY_DEFAULT, &behavior_battery_led_driver_api);

DT_INST_FOREACH_STATUS_OKAY(BATTERY_LED_INST)
