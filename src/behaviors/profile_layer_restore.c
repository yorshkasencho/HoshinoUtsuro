/*
 * HoshinoUtsuro BLE profile -> layer restore
 *
 * ZMK stores the active BLE profile in persistent settings.
 * Deep sleep clears volatile layer state, so restore the intended base layer
 * after settings_load() has finished:
 *
 *   BLE0 -> default_layer (C hold = CTRL)
 *   BLE1 -> apple_layer + draw_layer (iPad default)
 *   BLE2 -> apple_layer (iPhone)
 *   BLE3 -> apple_layer (MacBook)
 *   BLE4 -> default_layer (C hold = CTRL)
 */

#include <zephyr/settings/settings.h>

#include <zmk/ble.h>
#include <zmk/keymap.h>

#define HOSHINO_LAYER_DEFAULT 0
#define HOSHINO_LAYER_APPLE   1
#define HOSHINO_LAYER_DRAW    5

static int hoshino_restore_profile_layers(void) {
    const int profile = zmk_ble_active_profile_index();

    switch (profile) {
    case 1:
        /* iPad: Apple base + Draw as the default working mode. */
        zmk_keymap_layer_to(HOSHINO_LAYER_APPLE);
        zmk_keymap_layer_activate(HOSHINO_LAYER_DRAW);
        break;

    case 2:
    case 3:
        /* iPhone / MacBook: Apple base, normal layout. */
        zmk_keymap_layer_to(HOSHINO_LAYER_APPLE);
        break;

    case 0:
    case 4:
    default:
        /* Windows / default: CTRL on C hold. */
        zmk_keymap_layer_to(HOSHINO_LAYER_DEFAULT);
        break;
    }

    return 0;
}

/*
 * settings_load() first restores all saved values (including BLE active_profile),
 * then calls every registered h_commit handler. That makes this a deterministic
 * point to rebuild the volatile layer state after boot/deep-sleep wake.
 */
SETTINGS_STATIC_HANDLER_DEFINE(hoshino_profile_layer_restore,
                               "hoshino_profile_layer_restore",
                               NULL,
                               NULL,
                               hoshino_restore_profile_layers,
                               NULL);
