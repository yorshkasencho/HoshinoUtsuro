#error "behavior_battery_led.c is compiled"

#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>

#include <zmk/behavior.h>
#include <zmk/keymap.h>

#define DT_DRV_COMPAT zmk_behavior_battery_led

/*
 * XIAO nRF52840 RGB LED test with peripheral power enable
 *
 * USER RGB LED:
 *   Red   = P0.26
 *   Green = P0.30
 *   Blue  = P0.06
 *
 * Possible power gate:
 *   P1.08 = IMU_PWR / peripheral power enable
 */

static const struct device *gpio0_dev = DEVICE_DT_GET(DT_NODELABEL(gpio0));
static const struct device *gpio1_dev = DEVICE_DT_GET(DT_NODELABEL(gpio1));

static struct k_work_delayable led_work;
static int step = 0;

static void all_off(void) {
    gpio_pin_set_raw(gpio0_dev, 26, 1); /* Red off */
    gpio_pin_set_raw(gpio0_dev, 30, 1); /* Green off */
    gpio_pin_set_raw(gpio0_dev, 6, 1);  /* Blue off */
}

static void show_red(void) {
    all_off();
    gpio_pin_set_raw(gpio0_dev, 26, 0);
}

static void show_green(void) {
    all_off();
    gpio_pin_set_raw(gpio0_dev, 30, 0);
}

static void show_blue(void) {
    all_off();
    gpio_pin_set_raw(gpio0_dev, 6, 0);
}

static void show_yellow(void) {
    all_off();
    gpio_pin_set_raw(gpio0_dev, 26, 0);
    gpio_pin_set_raw(gpio0_dev, 30, 0);
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

        /*
         * テスト後に省電力へ戻す。
         * もしこれを入れると次回光らない場合は、ここをコメントアウト。
         */
        gpio_pin_set_raw(gpio1_dev, 8, 0);
        return;
    }

    step++;
    k_work_schedule(&led_work, K_MSEC(300));
}

static int battery_led_start_test(void) {
    /*
     * P1.08 peripheral power enable test.
     * HIGH/LOWどちらが有効か環境差がある可能性はあるが、まずHIGHで試す。
     */
    gpio_pin_set_raw(gpio1_dev, 8, 1);
    k_msleep(10);

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
    if (!device_is_ready(gpio0_dev) || !device_is_ready(gpio1_dev)) {
        return -ENODEV;
    }

    gpio_pin_configure(gpio1_dev, 8, GPIO_OUTPUT_LOW);

    gpio_pin_configure(gpio0_dev, 26, GPIO_OUTPUT_HIGH);
    gpio_pin_configure(gpio0_dev, 30, GPIO_OUTPUT_HIGH);
    gpio_pin_configure(gpio0_dev, 6, GPIO_OUTPUT_HIGH);

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
