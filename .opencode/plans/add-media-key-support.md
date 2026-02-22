# Plan: Add Media Key & Universal Device Support

## Problem

hotkeyd cannot bind to media/special keys (play/pause, next, previous, fast forward, etc.) from wireless remotes like a Telink Wireless receiver. Two root causes:

1. **Key name table is incomplete** — `src/evdev_util.cpp:18-59` only has ~80 standard keyboard keys. Media keys are absent, so they display as opaque `KEY_163` numbers and can't be referenced by name in config files.
2. **Device detection is too restrictive** — `src/evdev_util.cpp:121-131` only detects devices that report `KEY_A`, filtering out media-only remotes that don't have letter keys.

## Changes

**Single file modified:** `src/evdev_util.cpp`

### Change 1: Exhaustive key name table (replace lines 18-59)

Replace the small `codeToName()` map with a comprehensive one covering all `KEY_*` and `BTN_*` constants from `<linux/input-event-codes.h>`. Use an `ENTRY(x)` macro expanding to `{x, #x}` for clean generation.

**Skip aliases** to avoid duplicate map keys:
- `KEY_HANGUEL` (= `KEY_HANGEUL`)
- `KEY_SCREENLOCK` (= `KEY_COFFEE`)
- `KEY_DIRECTION` (= `KEY_ROTATE_DISPLAY`)
- `KEY_DASHBOARD` (= `KEY_ALL_APPLICATIONS`)
- `KEY_BRIGHTNESS_ZERO` (= `KEY_BRIGHTNESS_AUTO`)
- `KEY_WIMAX` (= `KEY_WWAN`)
- `KEY_ZOOM` (= `KEY_FULL_SCREEN`)
- `KEY_SCREEN` (= `KEY_ASPECT_RATIO`)
- `KEY_BRIGHTNESS_TOGGLE` (= `KEY_DISPLAYTOGGLE`)
- `KEY_MIN_INTERESTING` (= `KEY_MUTE`)
- `BTN_MISC` (= `BTN_0`)
- `BTN_MOUSE` (= `BTN_LEFT`)
- `BTN_JOYSTICK` (= `BTN_TRIGGER`)
- `BTN_GAMEPAD` (= `BTN_SOUTH`)
- `BTN_A` (= `BTN_SOUTH`)
- `BTN_B` (= `BTN_EAST`)
- `BTN_X` (= `BTN_NORTH`)
- `BTN_Y` (= `BTN_WEST`)
- `BTN_DIGI` (= `BTN_TOOL_PEN`)
- `BTN_WHEEL` (= `BTN_GEAR_DOWN`)
- `BTN_TRIGGER_HAPPY` (= `BTN_TRIGGER_HAPPY1`)

The new table includes:
- All standard keyboard keys (already present)
- **All media keys**: KEY_PLAYPAUSE, KEY_NEXTSONG, KEY_PREVIOUSSONG, KEY_FASTFORWARD, KEY_REWIND, KEY_STOPCD, KEY_RECORD, KEY_PLAYCD, KEY_PAUSECD
- **Volume/audio**: KEY_MUTE, KEY_VOLUMEDOWN, KEY_VOLUMEUP, KEY_BASSBOOST, KEY_MICMUTE
- **Power/system**: KEY_POWER, KEY_SLEEP, KEY_WAKEUP, KEY_SUSPEND
- **Browser/app**: KEY_WWW, KEY_MAIL, KEY_BOOKMARKS, KEY_HOMEPAGE, KEY_BACK, KEY_FORWARD, KEY_REFRESH, KEY_SEARCH, KEY_CALC, KEY_COFFEE
- **Brightness**: KEY_BRIGHTNESSDOWN, KEY_BRIGHTNESSUP, KEY_BRIGHTNESS_CYCLE, KEY_BRIGHTNESS_AUTO, KEY_KBDILLUMTOGGLE/DOWN/UP
- **F13-F24**, Fn keys, numeric keypad (phone-style KEY_NUMERIC_*)
- **TV/media center**: KEY_RED, KEY_GREEN, KEY_YELLOW, KEY_BLUE, KEY_CHANNELUP/DOWN, KEY_TV, KEY_DVD, KEY_RADIO, etc.
- **Mouse/gamepad buttons**: BTN_LEFT, BTN_RIGHT, BTN_MIDDLE, BTN_SIDE, BTN_EXTRA, joystick/gamepad buttons, touchpad buttons, etc.
- **Navigation/marine**: KEY_NAV_CHART, KEY_FISHING_CHART, KEY_SOS, etc.
- **Macro keys**: KEY_MACRO1-30, macro record/preset keys, LCD menu keys
- Everything else up to KEY_MAX (0x2ff) and BTN_TRIGGER_HAPPY40

### Change 2: Relax device detection (replace lines 120-131)

Replace:
```cpp
// Check for KEY_A (30) as a rough keyboard heuristic
int bit = KEY_A;
if (keybits[bit / (sizeof(unsigned long) * 8)] &
    (1UL << (bit % (sizeof(unsigned long) * 8)))) {
    is_keyboard = true;
}
```

With a loop that checks if **any** key bit is set:
```cpp
// Accept any device that has at least one key code
for (size_t i = 0; i < sizeof(keybits) / sizeof(keybits[0]); ++i) {
    if (keybits[i] != 0) {
        is_keyboard = true;
        break;
    }
}
```

This ensures any EV_KEY-capable device (keyboards, remotes, gamepads, mice) is detected.

### What does NOT change

- `main.cpp` — the recording loop and daemon loop already process arbitrary `ev.code` values
- `config.cpp` / `config.hpp` — serialization calls `evdev::keyName()` (benefits automatically), parsing already handles `KEY_<number>` fallback
- `Makefile` — no changes needed

### Verification

- `make clean && make` should compile without errors or warnings
- Existing config files with numeric keys like `KEY_163` continue to parse correctly
- `--detect` now lists the Telink remote
- `--record` shows human-readable names like `KEY_PLAYPAUSE` instead of `KEY_164`
- Config files can now contain `KEY_PLAYPAUSE` etc. and they resolve correctly
