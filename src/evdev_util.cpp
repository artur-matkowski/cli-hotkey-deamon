#include "evdev_util.hpp"

#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>
#include <cstring>
#include <cstdio>
#include <sys/ioctl.h>
#include <linux/input.h>
#include <algorithm>
#include <map>

namespace evdev {

// ------------------------------------------------------------------
// Key name table — exhaustive, generated from <linux/input-event-codes.h>.
// Aliases that map to the same numeric value as another entry are excluded
// (e.g. KEY_HANGUEL == KEY_HANGEUL, BTN_A == BTN_SOUTH, etc.).
// ------------------------------------------------------------------
#define ENTRY(x) {x, #x}

static const std::map<int, std::string>& codeToName() {
    static const std::map<int, std::string> m = {
        // ---- standard keyboard keys ----
        ENTRY(KEY_RESERVED),
        ENTRY(KEY_ESC),
        ENTRY(KEY_1), ENTRY(KEY_2), ENTRY(KEY_3), ENTRY(KEY_4), ENTRY(KEY_5),
        ENTRY(KEY_6), ENTRY(KEY_7), ENTRY(KEY_8), ENTRY(KEY_9), ENTRY(KEY_0),
        ENTRY(KEY_MINUS), ENTRY(KEY_EQUAL), ENTRY(KEY_BACKSPACE), ENTRY(KEY_TAB),
        ENTRY(KEY_Q), ENTRY(KEY_W), ENTRY(KEY_E), ENTRY(KEY_R), ENTRY(KEY_T),
        ENTRY(KEY_Y), ENTRY(KEY_U), ENTRY(KEY_I), ENTRY(KEY_O), ENTRY(KEY_P),
        ENTRY(KEY_LEFTBRACE), ENTRY(KEY_RIGHTBRACE), ENTRY(KEY_ENTER),
        ENTRY(KEY_LEFTCTRL),
        ENTRY(KEY_A), ENTRY(KEY_S), ENTRY(KEY_D), ENTRY(KEY_F), ENTRY(KEY_G),
        ENTRY(KEY_H), ENTRY(KEY_J), ENTRY(KEY_K), ENTRY(KEY_L),
        ENTRY(KEY_SEMICOLON), ENTRY(KEY_APOSTROPHE), ENTRY(KEY_GRAVE),
        ENTRY(KEY_LEFTSHIFT), ENTRY(KEY_BACKSLASH),
        ENTRY(KEY_Z), ENTRY(KEY_X), ENTRY(KEY_C), ENTRY(KEY_V), ENTRY(KEY_B),
        ENTRY(KEY_N), ENTRY(KEY_M),
        ENTRY(KEY_COMMA), ENTRY(KEY_DOT), ENTRY(KEY_SLASH),
        ENTRY(KEY_RIGHTSHIFT), ENTRY(KEY_KPASTERISK), ENTRY(KEY_LEFTALT),
        ENTRY(KEY_SPACE), ENTRY(KEY_CAPSLOCK),

        // ---- function keys ----
        ENTRY(KEY_F1), ENTRY(KEY_F2), ENTRY(KEY_F3), ENTRY(KEY_F4),
        ENTRY(KEY_F5), ENTRY(KEY_F6), ENTRY(KEY_F7), ENTRY(KEY_F8),
        ENTRY(KEY_F9), ENTRY(KEY_F10),
        ENTRY(KEY_NUMLOCK), ENTRY(KEY_SCROLLLOCK),

        // ---- numpad ----
        ENTRY(KEY_KP7), ENTRY(KEY_KP8), ENTRY(KEY_KP9), ENTRY(KEY_KPMINUS),
        ENTRY(KEY_KP4), ENTRY(KEY_KP5), ENTRY(KEY_KP6), ENTRY(KEY_KPPLUS),
        ENTRY(KEY_KP1), ENTRY(KEY_KP2), ENTRY(KEY_KP3),
        ENTRY(KEY_KP0), ENTRY(KEY_KPDOT),

        // ---- international / extra keyboard keys ----
        ENTRY(KEY_ZENKAKUHANKAKU), ENTRY(KEY_102ND),
        ENTRY(KEY_F11), ENTRY(KEY_F12),
        ENTRY(KEY_RO), ENTRY(KEY_KATAKANA), ENTRY(KEY_HIRAGANA),
        ENTRY(KEY_HENKAN), ENTRY(KEY_KATAKANAHIRAGANA), ENTRY(KEY_MUHENKAN),
        ENTRY(KEY_KPJPCOMMA), ENTRY(KEY_KPENTER), ENTRY(KEY_RIGHTCTRL),
        ENTRY(KEY_KPSLASH), ENTRY(KEY_SYSRQ), ENTRY(KEY_RIGHTALT),
        ENTRY(KEY_LINEFEED),

        // ---- navigation ----
        ENTRY(KEY_HOME), ENTRY(KEY_UP), ENTRY(KEY_PAGEUP),
        ENTRY(KEY_LEFT), ENTRY(KEY_RIGHT),
        ENTRY(KEY_END), ENTRY(KEY_DOWN), ENTRY(KEY_PAGEDOWN),
        ENTRY(KEY_INSERT), ENTRY(KEY_DELETE),

        // ---- system / media / multimedia ----
        ENTRY(KEY_MACRO),
        ENTRY(KEY_MUTE), ENTRY(KEY_VOLUMEDOWN), ENTRY(KEY_VOLUMEUP),
        ENTRY(KEY_POWER),
        ENTRY(KEY_KPEQUAL), ENTRY(KEY_KPPLUSMINUS), ENTRY(KEY_PAUSE),
        ENTRY(KEY_SCALE), ENTRY(KEY_KPCOMMA),
        ENTRY(KEY_HANGEUL), // KEY_HANGUEL is alias
        ENTRY(KEY_HANJA), ENTRY(KEY_YEN),
        ENTRY(KEY_LEFTMETA), ENTRY(KEY_RIGHTMETA), ENTRY(KEY_COMPOSE),

        ENTRY(KEY_STOP), ENTRY(KEY_AGAIN), ENTRY(KEY_PROPS),
        ENTRY(KEY_UNDO), ENTRY(KEY_FRONT), ENTRY(KEY_COPY),
        ENTRY(KEY_OPEN), ENTRY(KEY_PASTE), ENTRY(KEY_FIND),
        ENTRY(KEY_CUT), ENTRY(KEY_HELP), ENTRY(KEY_MENU),
        ENTRY(KEY_CALC), ENTRY(KEY_SETUP),
        ENTRY(KEY_SLEEP), ENTRY(KEY_WAKEUP),
        ENTRY(KEY_FILE), ENTRY(KEY_SENDFILE), ENTRY(KEY_DELETEFILE),
        ENTRY(KEY_XFER), ENTRY(KEY_PROG1), ENTRY(KEY_PROG2),
        ENTRY(KEY_WWW), ENTRY(KEY_MSDOS),
        ENTRY(KEY_COFFEE),       // KEY_SCREENLOCK is alias
        ENTRY(KEY_ROTATE_DISPLAY), // KEY_DIRECTION is alias
        ENTRY(KEY_CYCLEWINDOWS), ENTRY(KEY_MAIL),
        ENTRY(KEY_BOOKMARKS), ENTRY(KEY_COMPUTER),
        ENTRY(KEY_BACK), ENTRY(KEY_FORWARD),
        ENTRY(KEY_CLOSECD), ENTRY(KEY_EJECTCD), ENTRY(KEY_EJECTCLOSECD),

        // ---- media transport ----
        ENTRY(KEY_NEXTSONG), ENTRY(KEY_PLAYPAUSE), ENTRY(KEY_PREVIOUSSONG),
        ENTRY(KEY_STOPCD), ENTRY(KEY_RECORD), ENTRY(KEY_REWIND),
        ENTRY(KEY_PHONE), ENTRY(KEY_ISO), ENTRY(KEY_CONFIG),
        ENTRY(KEY_HOMEPAGE), ENTRY(KEY_REFRESH), ENTRY(KEY_EXIT),
        ENTRY(KEY_MOVE), ENTRY(KEY_EDIT),
        ENTRY(KEY_SCROLLUP), ENTRY(KEY_SCROLLDOWN),
        ENTRY(KEY_KPLEFTPAREN), ENTRY(KEY_KPRIGHTPAREN),
        ENTRY(KEY_NEW), ENTRY(KEY_REDO),

        // ---- F13-F24 ----
        ENTRY(KEY_F13), ENTRY(KEY_F14), ENTRY(KEY_F15), ENTRY(KEY_F16),
        ENTRY(KEY_F17), ENTRY(KEY_F18), ENTRY(KEY_F19), ENTRY(KEY_F20),
        ENTRY(KEY_F21), ENTRY(KEY_F22), ENTRY(KEY_F23), ENTRY(KEY_F24),

        // ---- more media ----
        ENTRY(KEY_PLAYCD), ENTRY(KEY_PAUSECD),
        ENTRY(KEY_PROG3), ENTRY(KEY_PROG4),
        ENTRY(KEY_ALL_APPLICATIONS), // KEY_DASHBOARD is alias
        ENTRY(KEY_SUSPEND), ENTRY(KEY_CLOSE),
        ENTRY(KEY_PLAY), ENTRY(KEY_FASTFORWARD),
        ENTRY(KEY_BASSBOOST), ENTRY(KEY_PRINT),
        ENTRY(KEY_HP), ENTRY(KEY_CAMERA), ENTRY(KEY_SOUND),
        ENTRY(KEY_QUESTION), ENTRY(KEY_EMAIL), ENTRY(KEY_CHAT),
        ENTRY(KEY_SEARCH), ENTRY(KEY_CONNECT),
        ENTRY(KEY_FINANCE), ENTRY(KEY_SPORT), ENTRY(KEY_SHOP),
        ENTRY(KEY_ALTERASE), ENTRY(KEY_CANCEL),

        // ---- brightness / keyboard illumination ----
        ENTRY(KEY_BRIGHTNESSDOWN), ENTRY(KEY_BRIGHTNESSUP),
        ENTRY(KEY_MEDIA),
        ENTRY(KEY_SWITCHVIDEOMODE),
        ENTRY(KEY_KBDILLUMTOGGLE), ENTRY(KEY_KBDILLUMDOWN), ENTRY(KEY_KBDILLUMUP),

        // ---- communication ----
        ENTRY(KEY_SEND), ENTRY(KEY_REPLY), ENTRY(KEY_FORWARDMAIL),
        ENTRY(KEY_SAVE), ENTRY(KEY_DOCUMENTS),

        // ---- hardware ----
        ENTRY(KEY_BATTERY), ENTRY(KEY_BLUETOOTH), ENTRY(KEY_WLAN),
        ENTRY(KEY_UWB), ENTRY(KEY_UNKNOWN),
        ENTRY(KEY_VIDEO_NEXT), ENTRY(KEY_VIDEO_PREV),
        ENTRY(KEY_BRIGHTNESS_CYCLE),
        ENTRY(KEY_BRIGHTNESS_AUTO), // KEY_BRIGHTNESS_ZERO is alias
        ENTRY(KEY_DISPLAY_OFF),
        ENTRY(KEY_WWAN), // KEY_WIMAX is alias
        ENTRY(KEY_RFKILL), ENTRY(KEY_MICMUTE),

        // ---- digital TV / set-top box ----
        ENTRY(KEY_OK), ENTRY(KEY_SELECT), ENTRY(KEY_GOTO), ENTRY(KEY_CLEAR),
        ENTRY(KEY_POWER2), ENTRY(KEY_OPTION), ENTRY(KEY_INFO),
        ENTRY(KEY_TIME), ENTRY(KEY_VENDOR), ENTRY(KEY_ARCHIVE),
        ENTRY(KEY_PROGRAM), ENTRY(KEY_CHANNEL),
        ENTRY(KEY_FAVORITES), ENTRY(KEY_EPG), ENTRY(KEY_PVR), ENTRY(KEY_MHP),
        ENTRY(KEY_LANGUAGE), ENTRY(KEY_TITLE), ENTRY(KEY_SUBTITLE),
        ENTRY(KEY_ANGLE),
        ENTRY(KEY_FULL_SCREEN), // KEY_ZOOM is alias
        ENTRY(KEY_MODE), ENTRY(KEY_KEYBOARD),
        ENTRY(KEY_ASPECT_RATIO), // KEY_SCREEN is alias
        ENTRY(KEY_PC),
        ENTRY(KEY_TV), ENTRY(KEY_TV2), ENTRY(KEY_VCR), ENTRY(KEY_VCR2),
        ENTRY(KEY_SAT), ENTRY(KEY_SAT2),
        ENTRY(KEY_CD), ENTRY(KEY_TAPE), ENTRY(KEY_RADIO),
        ENTRY(KEY_TUNER), ENTRY(KEY_PLAYER), ENTRY(KEY_TEXT),
        ENTRY(KEY_DVD), ENTRY(KEY_AUX), ENTRY(KEY_MP3),
        ENTRY(KEY_AUDIO), ENTRY(KEY_VIDEO), ENTRY(KEY_DIRECTORY),
        ENTRY(KEY_LIST), ENTRY(KEY_MEMO), ENTRY(KEY_CALENDAR),
        ENTRY(KEY_RED), ENTRY(KEY_GREEN), ENTRY(KEY_YELLOW), ENTRY(KEY_BLUE),
        ENTRY(KEY_CHANNELUP), ENTRY(KEY_CHANNELDOWN),
        ENTRY(KEY_FIRST), ENTRY(KEY_LAST), ENTRY(KEY_AB),
        ENTRY(KEY_NEXT), ENTRY(KEY_RESTART), ENTRY(KEY_SLOW),
        ENTRY(KEY_SHUFFLE), ENTRY(KEY_BREAK), ENTRY(KEY_PREVIOUS),
        ENTRY(KEY_DIGITS), ENTRY(KEY_TEEN), ENTRY(KEY_TWEN),

        // ---- more consumer keys ----
        ENTRY(KEY_VIDEOPHONE), ENTRY(KEY_GAMES),
        ENTRY(KEY_ZOOMIN), ENTRY(KEY_ZOOMOUT), ENTRY(KEY_ZOOMRESET),
        ENTRY(KEY_WORDPROCESSOR), ENTRY(KEY_EDITOR),
        ENTRY(KEY_SPREADSHEET), ENTRY(KEY_GRAPHICSEDITOR),
        ENTRY(KEY_PRESENTATION), ENTRY(KEY_DATABASE),
        ENTRY(KEY_NEWS), ENTRY(KEY_VOICEMAIL),
        ENTRY(KEY_ADDRESSBOOK), ENTRY(KEY_MESSENGER),
        ENTRY(KEY_DISPLAYTOGGLE), // KEY_BRIGHTNESS_TOGGLE is alias
        ENTRY(KEY_SPELLCHECK), ENTRY(KEY_LOGOFF),
        ENTRY(KEY_DOLLAR), ENTRY(KEY_EURO),

        // ---- transport controls ----
        ENTRY(KEY_FRAMEBACK), ENTRY(KEY_FRAMEFORWARD),
        ENTRY(KEY_CONTEXT_MENU), ENTRY(KEY_MEDIA_REPEAT),
        ENTRY(KEY_10CHANNELSUP), ENTRY(KEY_10CHANNELSDOWN),
        ENTRY(KEY_IMAGES),
        ENTRY(KEY_NOTIFICATION_CENTER),
        ENTRY(KEY_PICKUP_PHONE), ENTRY(KEY_HANGUP_PHONE),
        ENTRY(KEY_LINK_PHONE),

        // ---- editing ----
        ENTRY(KEY_DEL_EOL), ENTRY(KEY_DEL_EOS),
        ENTRY(KEY_INS_LINE), ENTRY(KEY_DEL_LINE),

        // ---- Fn keys ----
        ENTRY(KEY_FN),
        ENTRY(KEY_FN_ESC),
        ENTRY(KEY_FN_F1), ENTRY(KEY_FN_F2), ENTRY(KEY_FN_F3),
        ENTRY(KEY_FN_F4), ENTRY(KEY_FN_F5), ENTRY(KEY_FN_F6),
        ENTRY(KEY_FN_F7), ENTRY(KEY_FN_F8), ENTRY(KEY_FN_F9),
        ENTRY(KEY_FN_F10), ENTRY(KEY_FN_F11), ENTRY(KEY_FN_F12),
        ENTRY(KEY_FN_1), ENTRY(KEY_FN_2),
        ENTRY(KEY_FN_D), ENTRY(KEY_FN_E), ENTRY(KEY_FN_F),
        ENTRY(KEY_FN_S), ENTRY(KEY_FN_B), ENTRY(KEY_FN_RIGHT_SHIFT),

        // ---- braille ----
        ENTRY(KEY_BRL_DOT1), ENTRY(KEY_BRL_DOT2), ENTRY(KEY_BRL_DOT3),
        ENTRY(KEY_BRL_DOT4), ENTRY(KEY_BRL_DOT5), ENTRY(KEY_BRL_DOT6),
        ENTRY(KEY_BRL_DOT7), ENTRY(KEY_BRL_DOT8), ENTRY(KEY_BRL_DOT9),
        ENTRY(KEY_BRL_DOT10),

        // ---- numeric (phone / remote keypad) ----
        ENTRY(KEY_NUMERIC_0), ENTRY(KEY_NUMERIC_1), ENTRY(KEY_NUMERIC_2),
        ENTRY(KEY_NUMERIC_3), ENTRY(KEY_NUMERIC_4), ENTRY(KEY_NUMERIC_5),
        ENTRY(KEY_NUMERIC_6), ENTRY(KEY_NUMERIC_7), ENTRY(KEY_NUMERIC_8),
        ENTRY(KEY_NUMERIC_9),
        ENTRY(KEY_NUMERIC_STAR), ENTRY(KEY_NUMERIC_POUND),
        ENTRY(KEY_NUMERIC_A), ENTRY(KEY_NUMERIC_B),
        ENTRY(KEY_NUMERIC_C), ENTRY(KEY_NUMERIC_D),

        // ---- camera / misc ----
        ENTRY(KEY_CAMERA_FOCUS), ENTRY(KEY_WPS_BUTTON),
        ENTRY(KEY_TOUCHPAD_TOGGLE), ENTRY(KEY_TOUCHPAD_ON), ENTRY(KEY_TOUCHPAD_OFF),
        ENTRY(KEY_CAMERA_ZOOMIN), ENTRY(KEY_CAMERA_ZOOMOUT),
        ENTRY(KEY_CAMERA_UP), ENTRY(KEY_CAMERA_DOWN),
        ENTRY(KEY_CAMERA_LEFT), ENTRY(KEY_CAMERA_RIGHT),
        ENTRY(KEY_ATTENDANT_ON), ENTRY(KEY_ATTENDANT_OFF),
        ENTRY(KEY_ATTENDANT_TOGGLE), ENTRY(KEY_LIGHTS_TOGGLE),

        // ---- system ----
        ENTRY(KEY_ALS_TOGGLE), ENTRY(KEY_ROTATE_LOCK_TOGGLE),
        ENTRY(KEY_REFRESH_RATE_TOGGLE),
        ENTRY(KEY_BUTTONCONFIG), ENTRY(KEY_TASKMANAGER),
        ENTRY(KEY_JOURNAL), ENTRY(KEY_CONTROLPANEL),
        ENTRY(KEY_APPSELECT), ENTRY(KEY_SCREENSAVER),
        ENTRY(KEY_VOICECOMMAND), ENTRY(KEY_ASSISTANT),
        ENTRY(KEY_KBD_LAYOUT_NEXT), ENTRY(KEY_EMOJI_PICKER),
        ENTRY(KEY_DICTATE),
        ENTRY(KEY_BRIGHTNESS_MIN), ENTRY(KEY_BRIGHTNESS_MAX),

        // ---- input assist ----
        ENTRY(KEY_KBDINPUTASSIST_PREV), ENTRY(KEY_KBDINPUTASSIST_NEXT),
        ENTRY(KEY_KBDINPUTASSIST_PREVGROUP), ENTRY(KEY_KBDINPUTASSIST_NEXTGROUP),
        ENTRY(KEY_KBDINPUTASSIST_ACCEPT), ENTRY(KEY_KBDINPUTASSIST_CANCEL),

        // ---- diagonal navigation ----
        ENTRY(KEY_RIGHT_UP), ENTRY(KEY_RIGHT_DOWN),
        ENTRY(KEY_LEFT_UP), ENTRY(KEY_LEFT_DOWN),

        // ---- media top menu / advanced ----
        ENTRY(KEY_ROOT_MENU), ENTRY(KEY_MEDIA_TOP_MENU),
        ENTRY(KEY_NUMERIC_11), ENTRY(KEY_NUMERIC_12),
        ENTRY(KEY_AUDIO_DESC), ENTRY(KEY_3D_MODE),
        ENTRY(KEY_NEXT_FAVORITE),
        ENTRY(KEY_STOP_RECORD), ENTRY(KEY_PAUSE_RECORD),
        ENTRY(KEY_VOD), ENTRY(KEY_UNMUTE), ENTRY(KEY_FASTREVERSE),
        ENTRY(KEY_SLOWREVERSE), ENTRY(KEY_DATA),
        ENTRY(KEY_ONSCREEN_KEYBOARD),
        ENTRY(KEY_PRIVACY_SCREEN_TOGGLE), ENTRY(KEY_SELECTIVE_SCREENSHOT),
        ENTRY(KEY_NEXT_ELEMENT), ENTRY(KEY_PREVIOUS_ELEMENT),
        ENTRY(KEY_AUTOPILOT_ENGAGE_TOGGLE), ENTRY(KEY_MARK_WAYPOINT),
        ENTRY(KEY_SOS),
        ENTRY(KEY_NAV_CHART), ENTRY(KEY_FISHING_CHART),
        ENTRY(KEY_SINGLE_RANGE_RADAR), ENTRY(KEY_DUAL_RANGE_RADAR),
        ENTRY(KEY_RADAR_OVERLAY),
        ENTRY(KEY_TRADITIONAL_SONAR), ENTRY(KEY_CLEARVU_SONAR),
        ENTRY(KEY_SIDEVU_SONAR),
        ENTRY(KEY_NAV_INFO), ENTRY(KEY_BRIGHTNESS_MENU),

        // ---- macro keys ----
        ENTRY(KEY_MACRO1), ENTRY(KEY_MACRO2), ENTRY(KEY_MACRO3),
        ENTRY(KEY_MACRO4), ENTRY(KEY_MACRO5), ENTRY(KEY_MACRO6),
        ENTRY(KEY_MACRO7), ENTRY(KEY_MACRO8), ENTRY(KEY_MACRO9),
        ENTRY(KEY_MACRO10), ENTRY(KEY_MACRO11), ENTRY(KEY_MACRO12),
        ENTRY(KEY_MACRO13), ENTRY(KEY_MACRO14), ENTRY(KEY_MACRO15),
        ENTRY(KEY_MACRO16), ENTRY(KEY_MACRO17), ENTRY(KEY_MACRO18),
        ENTRY(KEY_MACRO19), ENTRY(KEY_MACRO20), ENTRY(KEY_MACRO21),
        ENTRY(KEY_MACRO22), ENTRY(KEY_MACRO23), ENTRY(KEY_MACRO24),
        ENTRY(KEY_MACRO25), ENTRY(KEY_MACRO26), ENTRY(KEY_MACRO27),
        ENTRY(KEY_MACRO28), ENTRY(KEY_MACRO29), ENTRY(KEY_MACRO30),
        ENTRY(KEY_MACRO_RECORD_START), ENTRY(KEY_MACRO_RECORD_STOP),
        ENTRY(KEY_MACRO_PRESET_CYCLE),
        ENTRY(KEY_MACRO_PRESET1), ENTRY(KEY_MACRO_PRESET2), ENTRY(KEY_MACRO_PRESET3),
        ENTRY(KEY_KBD_LCD_MENU1), ENTRY(KEY_KBD_LCD_MENU2),
        ENTRY(KEY_KBD_LCD_MENU3), ENTRY(KEY_KBD_LCD_MENU4),
        ENTRY(KEY_KBD_LCD_MENU5),

        // ==== BTN_ (button) codes — also delivered as EV_KEY ====

        // ---- misc buttons ----
        ENTRY(BTN_0), // BTN_MISC is alias
        ENTRY(BTN_1), ENTRY(BTN_2), ENTRY(BTN_3), ENTRY(BTN_4),
        ENTRY(BTN_5), ENTRY(BTN_6), ENTRY(BTN_7), ENTRY(BTN_8), ENTRY(BTN_9),

        // ---- mouse buttons ----
        ENTRY(BTN_LEFT), // BTN_MOUSE is alias
        ENTRY(BTN_RIGHT), ENTRY(BTN_MIDDLE),
        ENTRY(BTN_SIDE), ENTRY(BTN_EXTRA),
        ENTRY(BTN_FORWARD), ENTRY(BTN_BACK), ENTRY(BTN_TASK),

        // ---- joystick buttons ----
        ENTRY(BTN_TRIGGER), // BTN_JOYSTICK is alias
        ENTRY(BTN_THUMB), ENTRY(BTN_THUMB2),
        ENTRY(BTN_TOP), ENTRY(BTN_TOP2), ENTRY(BTN_PINKIE),
        ENTRY(BTN_BASE), ENTRY(BTN_BASE2), ENTRY(BTN_BASE3),
        ENTRY(BTN_BASE4), ENTRY(BTN_BASE5), ENTRY(BTN_BASE6),
        ENTRY(BTN_DEAD),

        // ---- gamepad buttons ----
        ENTRY(BTN_SOUTH), // BTN_GAMEPAD, BTN_A are aliases
        ENTRY(BTN_EAST),  // BTN_B is alias
        ENTRY(BTN_C),
        ENTRY(BTN_NORTH), // BTN_X is alias
        ENTRY(BTN_WEST),  // BTN_Y is alias
        ENTRY(BTN_Z),
        ENTRY(BTN_TL), ENTRY(BTN_TR), ENTRY(BTN_TL2), ENTRY(BTN_TR2),
        ENTRY(BTN_SELECT), ENTRY(BTN_START), ENTRY(BTN_MODE),
        ENTRY(BTN_THUMBL), ENTRY(BTN_THUMBR),

        // ---- digitizer / stylus ----
        ENTRY(BTN_TOOL_PEN), // BTN_DIGI is alias
        ENTRY(BTN_TOOL_RUBBER), ENTRY(BTN_TOOL_BRUSH),
        ENTRY(BTN_TOOL_PENCIL), ENTRY(BTN_TOOL_AIRBRUSH),
        ENTRY(BTN_TOOL_FINGER), ENTRY(BTN_TOOL_MOUSE), ENTRY(BTN_TOOL_LENS),
        ENTRY(BTN_TOOL_QUINTTAP), ENTRY(BTN_STYLUS3),
        ENTRY(BTN_TOUCH), ENTRY(BTN_STYLUS), ENTRY(BTN_STYLUS2),
        ENTRY(BTN_TOOL_DOUBLETAP), ENTRY(BTN_TOOL_TRIPLETAP),
        ENTRY(BTN_TOOL_QUADTAP),

        // ---- wheel / gear ----
        ENTRY(BTN_GEAR_DOWN), // BTN_WHEEL is alias
        ENTRY(BTN_GEAR_UP),

        // ---- d-pad ----
        ENTRY(BTN_DPAD_UP), ENTRY(BTN_DPAD_DOWN),
        ENTRY(BTN_DPAD_LEFT), ENTRY(BTN_DPAD_RIGHT),

        // ---- trigger happy (flight sticks, etc.) ----
        ENTRY(BTN_TRIGGER_HAPPY1), // BTN_TRIGGER_HAPPY is alias
        ENTRY(BTN_TRIGGER_HAPPY2), ENTRY(BTN_TRIGGER_HAPPY3),
        ENTRY(BTN_TRIGGER_HAPPY4), ENTRY(BTN_TRIGGER_HAPPY5),
        ENTRY(BTN_TRIGGER_HAPPY6), ENTRY(BTN_TRIGGER_HAPPY7),
        ENTRY(BTN_TRIGGER_HAPPY8), ENTRY(BTN_TRIGGER_HAPPY9),
        ENTRY(BTN_TRIGGER_HAPPY10), ENTRY(BTN_TRIGGER_HAPPY11),
        ENTRY(BTN_TRIGGER_HAPPY12), ENTRY(BTN_TRIGGER_HAPPY13),
        ENTRY(BTN_TRIGGER_HAPPY14), ENTRY(BTN_TRIGGER_HAPPY15),
        ENTRY(BTN_TRIGGER_HAPPY16), ENTRY(BTN_TRIGGER_HAPPY17),
        ENTRY(BTN_TRIGGER_HAPPY18), ENTRY(BTN_TRIGGER_HAPPY19),
        ENTRY(BTN_TRIGGER_HAPPY20), ENTRY(BTN_TRIGGER_HAPPY21),
        ENTRY(BTN_TRIGGER_HAPPY22), ENTRY(BTN_TRIGGER_HAPPY23),
        ENTRY(BTN_TRIGGER_HAPPY24), ENTRY(BTN_TRIGGER_HAPPY25),
        ENTRY(BTN_TRIGGER_HAPPY26), ENTRY(BTN_TRIGGER_HAPPY27),
        ENTRY(BTN_TRIGGER_HAPPY28), ENTRY(BTN_TRIGGER_HAPPY29),
        ENTRY(BTN_TRIGGER_HAPPY30), ENTRY(BTN_TRIGGER_HAPPY31),
        ENTRY(BTN_TRIGGER_HAPPY32), ENTRY(BTN_TRIGGER_HAPPY33),
        ENTRY(BTN_TRIGGER_HAPPY34), ENTRY(BTN_TRIGGER_HAPPY35),
        ENTRY(BTN_TRIGGER_HAPPY36), ENTRY(BTN_TRIGGER_HAPPY37),
        ENTRY(BTN_TRIGGER_HAPPY38), ENTRY(BTN_TRIGGER_HAPPY39),
        ENTRY(BTN_TRIGGER_HAPPY40),
    };
    return m;
}

#undef ENTRY

static const std::map<std::string, int>& nameToCode() {
    static std::map<std::string, int> m;
    static bool init = false;
    if (!init) {
        for (auto& [code, name] : codeToName()) {
            m[name] = code;
        }
        init = true;
    }
    return m;
}

std::string keyName(int code) {
    auto& m = codeToName();
    auto it = m.find(code);
    if (it != m.end()) return it->second;
    char buf[32];
    snprintf(buf, sizeof(buf), "KEY_%d", code);
    return buf;
}

int keyCode(const std::string& name) {
    auto& m = nameToCode();
    auto it = m.find(name);
    if (it != m.end()) return it->second;
    // Try parsing KEY_<number>
    if (name.rfind("KEY_", 0) == 0) {
        try { return std::stoi(name.substr(4)); } catch (...) {}
    }
    return -1;
}

// ------------------------------------------------------------------
// Device detection
// ------------------------------------------------------------------
std::vector<DeviceInfo> detectKeyboards() {
    std::vector<DeviceInfo> result;

    DIR* dir = opendir("/dev/input");
    if (!dir) return result;

    struct dirent* ent;
    while ((ent = readdir(dir)) != nullptr) {
        std::string fname = ent->d_name;
        if (fname.rfind("event", 0) != 0) continue;

        std::string path = "/dev/input/" + fname;
        int fd = open(path.c_str(), O_RDONLY | O_NONBLOCK);
        if (fd < 0) continue;

        // Get device name
        char namebuf[256] = "Unknown";
        ioctl(fd, EVIOCGNAME(sizeof(namebuf)), namebuf);

        // Check if it supports EV_KEY
        unsigned long evbits = 0;
        ioctl(fd, EVIOCGBIT(0, sizeof(evbits)), &evbits);
        bool has_keys = (evbits & (1 << EV_KEY)) != 0;

        // Further check: does it have any actual key/button codes?
        bool is_keyboard = false;
        if (has_keys) {
            unsigned long keybits[(KEY_MAX + 7) / 8 / sizeof(unsigned long) + 1] = {};
            ioctl(fd, EVIOCGBIT(EV_KEY, sizeof(keybits)), keybits);

            // Accept any device that reports at least one key or button code.
            // This covers standard keyboards, media remotes, gamepads, etc.
            for (size_t i = 0; i < sizeof(keybits) / sizeof(keybits[0]); ++i) {
                if (keybits[i] != 0) {
                    is_keyboard = true;
                    break;
                }
            }
        }

        close(fd);

        if (is_keyboard) {
            result.push_back({path, namebuf, true});
        }
    }
    closedir(dir);

    std::sort(result.begin(), result.end(),
              [](const DeviceInfo& a, const DeviceInfo& b) {
                  return a.path < b.path;
              });
    return result;
}

std::string getDeviceName(const std::string& path) {
    int fd = open(path.c_str(), O_RDONLY | O_NONBLOCK);
    if (fd < 0) return "";
    char namebuf[256] = "";
    ioctl(fd, EVIOCGNAME(sizeof(namebuf)), namebuf);
    close(fd);
    return namebuf;
}

std::string resolvePathByName(const std::string& name) {
    DIR* dir = opendir("/dev/input");
    if (!dir) return "";

    struct dirent* ent;
    while ((ent = readdir(dir)) != nullptr) {
        std::string fname = ent->d_name;
        if (fname.rfind("event", 0) != 0) continue;

        std::string path = "/dev/input/" + fname;
        int fd = open(path.c_str(), O_RDONLY | O_NONBLOCK);
        if (fd < 0) continue;

        char namebuf[256] = "";
        ioctl(fd, EVIOCGNAME(sizeof(namebuf)), namebuf);
        close(fd);

        if (name == namebuf) {
            closedir(dir);
            return path;
        }
    }
    closedir(dir);
    return "";
}

int openDevice(const std::string& path) {
    int fd = open(path.c_str(), O_RDONLY);
    return fd;
}

bool grabDevice(int fd) {
    return ioctl(fd, EVIOCGRAB, 1) == 0;
}

void ungrabDevice(int fd) {
    ioctl(fd, EVIOCGRAB, 0);
}

bool readEvent(int fd, struct input_event& ev) {
    ssize_t n = read(fd, &ev, sizeof(ev));
    return n == static_cast<ssize_t>(sizeof(ev));
}

} // namespace evdev
