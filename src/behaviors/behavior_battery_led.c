#define DT_DRV_COMPAT zmk_behavior_battery_led

#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#include <errno.h>

#include <drivers/behavior.h>
#include <zmk/behavior.h>

/*
 * XIAO nRF52840 onboard RGB USER LED test
 *
 * Zephyr board definition:
 *   led0 / Red   = P0.26
 *   led1 / Green = P0.30
 *   led2 / Blue  = P0.06
 *
 * XIAO onboard RGB LED is active-low:
 *   0 = ON
 *   1 = OFF
 */

static const struct gpio_dt_spec led_red = GPIO_DT_SPEC_GET(DT_NODELABEL(led0), gpios);
static const struct gpio_dt_spec led_green = GPIO_DT_SPEC_GET(DT_NODELABEL(led1), gpios);
static const struct gpio_dt_spec led_blue = GPIO_DT_SPEC_GET(DT_NODELABEL(led2), gpios);

static struct k_work_delayable led_work;
static int step = 0;

static void all_off(void) {
    gpio_pin_set_dt(&led_red, 1);
    gpio_pin_set_dt(&led_green, 1);
    gpio_pin_set_dt(&led_blue, 1);
}

static void show_red(void) {
    all_off();
    gpio_pin_set_dt(&led_red, 0);
}

static void show_green(void) {
    all_off();
    gpio_pin_set_dt(&led_green, 0);
}

static void show_blue(void) {
    all_off();
    gpio_pin_set_dt(&led_blue, 0);
}

static void show_yellow(void) {
    all_off();
    gpio_pin_set_dt(&led_red, 0);
    gpio_pin_set_dt(&led_green, 0);
}

static void led_work_handler(struct k_work *work) {
    switch (step) {
    case 0:
        show_red();
        break;
    case 1:
        all_off();
        break;
    case 2:
        show_green();
        break;
    case 3:
        all_off();
        break;
    case 4:
        show_blue();
        break;
    case 5:
        all_off();
        break;
    case 6:
        show_yellow();
        break;
    case 7:
        all_off();
        break;
    default:
        all_off();
        return;
    }

    step++;
    k_work_schedule(&led_work, K_MSEC(300));
}

static int battery_led_start_test(void) {
    step = 0;

    k_work_cancel_delayable(&led_work);
    k_work_schedule(&led_work, K_NO_WAIT);

    return ZMK_BEHAVIOR_OPAQUE;
}

static int on_keymap_binding_pressed(struct zmk_behavior_binding *binding,
                                     struct zmk_behavior_binding_event event) {
    return battery_led_start_test();
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

    all_off();

    k_work_init_delayable(&led_work, led_work_handler);

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
