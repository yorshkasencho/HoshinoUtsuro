/*
 * HoshinoUtsuro BLE profile -> layer restore
 *
 * BLE0 / Windows:
 *   Default layer
 *
 * BLE1 / iPad:
 *   Apple layer + Draw layer
 *
 * BLE2 / iPhone:
 *   Apple layer
 *
 * BLE3 / MacBook:
 *   Apple layer
 *
 * BLE4 / Default:
 *   Default layer
 *
 * 起動時だけでなく、
 * BLEプロファイル変更・再接続時にも
 * 現在のBLEプロファイルに合わせてレイヤーを再設定する。
 */

#include <zephyr/settings/settings.h>

#include <zmk/ble.h>
#include <zmk/keymap.h>

#include <zmk/event_manager.h>
#include <zmk/events/ble_active_profile_changed.h>


#define HOSHINO_LAYER_DEFAULT 0
#define HOSHINO_LAYER_APPLE   1
#define HOSHINO_LAYER_DRAW    5


/*
 * ------------------------------------------------------------
 * 現在のBLEプロファイルに合わせてレイヤーを設定
 * ------------------------------------------------------------
 */
static int hoshino_apply_profile_layers(int profile) {

    switch (profile) {

    case 1:
        /*
         * BLE1 / iPad
         *
         * Apple Base
         * +
         * Draw mode
         */
        zmk_keymap_layer_to(HOSHINO_LAYER_APPLE);
        zmk_keymap_layer_activate(HOSHINO_LAYER_DRAW);
        break;


    case 2:
    case 3:
        /*
         * BLE2 / iPhone
         * BLE3 / MacBook
         *
         * Apple Baseのみ
         *
         * layer_to() により
         * Draw layerもOFFになる。
         */
        zmk_keymap_layer_to(HOSHINO_LAYER_APPLE);
        break;


    case 0:
    case 4:
    default:
        /*
         * BLE0 / Windows
         * BLE4 / Default
         *
         * Windows Baseのみ
         *
         * layer_to() により
         * Draw layerを含む他レイヤーをOFFにする。
         */
        zmk_keymap_layer_to(HOSHINO_LAYER_DEFAULT);
        break;
    }

    return 0;
}


/*
 * ------------------------------------------------------------
 * Deep Sleep / 電源投入からの起動時
 *
 * SettingsからBLEのactive profileが読み込まれた後、
 * 保存されているBLEプロファイルに合わせてレイヤーを設定。
 * ------------------------------------------------------------
 */
static int hoshino_restore_profile_layers(void) {

    const int profile = zmk_ble_active_profile_index();

    return hoshino_apply_profile_layers(profile);
}


SETTINGS_STATIC_HANDLER_DEFINE(
    hoshino_profile_layer_restore,
    "hoshino_profile_layer_restore",
    NULL,
    NULL,
    hoshino_restore_profile_layers,
    NULL
);


/*
 * ------------------------------------------------------------
 * BLEプロファイル変更 / 再接続イベント
 *
 * Deep Sleep復帰後にWindowsへ再接続した場合も
 * BLE0を検出してDefault layerへ戻す。
 *
 * iPadへ再接続した場合は
 * Apple + Drawへ戻す。
 * ------------------------------------------------------------
 */
static int hoshino_profile_changed_listener(const zmk_event_t *eh) {

    const struct zmk_ble_active_profile_changed *event =
        as_zmk_ble_active_profile_changed(eh);

    if (event == NULL) {
        return ZMK_EV_EVENT_BUBBLE;
    }

    hoshino_apply_profile_layers(event->index);

    return ZMK_EV_EVENT_BUBBLE;
}


/*
 * Listener登録
 */
ZMK_LISTENER(
    hoshino_profile_layer_listener,
    hoshino_profile_changed_listener
);


/*
 * BLE active profileイベントを監視
 */
ZMK_SUBSCRIPTION(
    hoshino_profile_layer_listener,
    zmk_ble_active_profile_changed
);
