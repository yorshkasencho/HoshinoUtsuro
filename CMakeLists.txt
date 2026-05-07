# SPDX-License-Identifier: MIT

target_sources_ifdef(CONFIG_ZMK_BEHAVIOR_BATTERY_LED app PRIVATE src/behaviors/behavior_battery_led.c)
