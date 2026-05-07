# SPDX-License-Identifier: MIT

zephyr_library()

zephyr_library_sources_ifdef(CONFIG_ZMK_BEHAVIOR_BATTERY_LED src/behaviors/behavior_battery_led.c)
