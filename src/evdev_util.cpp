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
// Key name table (covering common keys; extend as needed)
// ------------------------------------------------------------------
static const std::map<int, std::string>& codeToName() {
    static const std::map<int, std::string> m = {
        {KEY_ESC,"KEY_ESC"},{KEY_1,"KEY_1"},{KEY_2,"KEY_2"},{KEY_3,"KEY_3"},
        {KEY_4,"KEY_4"},{KEY_5,"KEY_5"},{KEY_6,"KEY_6"},{KEY_7,"KEY_7"},
        {KEY_8,"KEY_8"},{KEY_9,"KEY_9"},{KEY_0,"KEY_0"},{KEY_MINUS,"KEY_MINUS"},
        {KEY_EQUAL,"KEY_EQUAL"},{KEY_BACKSPACE,"KEY_BACKSPACE"},{KEY_TAB,"KEY_TAB"},
        {KEY_Q,"KEY_Q"},{KEY_W,"KEY_W"},{KEY_E,"KEY_E"},{KEY_R,"KEY_R"},
        {KEY_T,"KEY_T"},{KEY_Y,"KEY_Y"},{KEY_U,"KEY_U"},{KEY_I,"KEY_I"},
        {KEY_O,"KEY_O"},{KEY_P,"KEY_P"},{KEY_LEFTBRACE,"KEY_LEFTBRACE"},
        {KEY_RIGHTBRACE,"KEY_RIGHTBRACE"},{KEY_ENTER,"KEY_ENTER"},
        {KEY_LEFTCTRL,"KEY_LEFTCTRL"},{KEY_A,"KEY_A"},{KEY_S,"KEY_S"},
        {KEY_D,"KEY_D"},{KEY_F,"KEY_F"},{KEY_G,"KEY_G"},{KEY_H,"KEY_H"},
        {KEY_J,"KEY_J"},{KEY_K,"KEY_K"},{KEY_L,"KEY_L"},
        {KEY_SEMICOLON,"KEY_SEMICOLON"},{KEY_APOSTROPHE,"KEY_APOSTROPHE"},
        {KEY_GRAVE,"KEY_GRAVE"},{KEY_LEFTSHIFT,"KEY_LEFTSHIFT"},
        {KEY_BACKSLASH,"KEY_BACKSLASH"},{KEY_Z,"KEY_Z"},{KEY_X,"KEY_X"},
        {KEY_C,"KEY_C"},{KEY_V,"KEY_V"},{KEY_B,"KEY_B"},{KEY_N,"KEY_N"},
        {KEY_M,"KEY_M"},{KEY_COMMA,"KEY_COMMA"},{KEY_DOT,"KEY_DOT"},
        {KEY_SLASH,"KEY_SLASH"},{KEY_RIGHTSHIFT,"KEY_RIGHTSHIFT"},
        {KEY_KPASTERISK,"KEY_KPASTERISK"},{KEY_LEFTALT,"KEY_LEFTALT"},
        {KEY_SPACE,"KEY_SPACE"},{KEY_CAPSLOCK,"KEY_CAPSLOCK"},
        {KEY_F1,"KEY_F1"},{KEY_F2,"KEY_F2"},{KEY_F3,"KEY_F3"},
        {KEY_F4,"KEY_F4"},{KEY_F5,"KEY_F5"},{KEY_F6,"KEY_F6"},
        {KEY_F7,"KEY_F7"},{KEY_F8,"KEY_F8"},{KEY_F9,"KEY_F9"},
        {KEY_F10,"KEY_F10"},{KEY_NUMLOCK,"KEY_NUMLOCK"},
        {KEY_SCROLLLOCK,"KEY_SCROLLLOCK"},
        {KEY_F11,"KEY_F11"},{KEY_F12,"KEY_F12"},
        {KEY_RIGHTCTRL,"KEY_RIGHTCTRL"},{KEY_KPSLASH,"KEY_KPSLASH"},
        {KEY_RIGHTALT,"KEY_RIGHTALT"},{KEY_HOME,"KEY_HOME"},{KEY_UP,"KEY_UP"},
        {KEY_PAGEUP,"KEY_PAGEUP"},{KEY_LEFT,"KEY_LEFT"},{KEY_RIGHT,"KEY_RIGHT"},
        {KEY_END,"KEY_END"},{KEY_DOWN,"KEY_DOWN"},{KEY_PAGEDOWN,"KEY_PAGEDOWN"},
        {KEY_INSERT,"KEY_INSERT"},{KEY_DELETE,"KEY_DELETE"},
        {KEY_LEFTMETA,"KEY_LEFTMETA"},{KEY_RIGHTMETA,"KEY_RIGHTMETA"},
        {KEY_KPENTER,"KEY_KPENTER"},{KEY_KP0,"KEY_KP0"},{KEY_KP1,"KEY_KP1"},
        {KEY_KP2,"KEY_KP2"},{KEY_KP3,"KEY_KP3"},{KEY_KP4,"KEY_KP4"},
        {KEY_KP5,"KEY_KP5"},{KEY_KP6,"KEY_KP6"},{KEY_KP7,"KEY_KP7"},
        {KEY_KP8,"KEY_KP8"},{KEY_KP9,"KEY_KP9"},{KEY_KPMINUS,"KEY_KPMINUS"},
        {KEY_KPPLUS,"KEY_KPPLUS"},{KEY_KPDOT,"KEY_KPDOT"},
        {KEY_SYSRQ,"KEY_SYSRQ"},{KEY_PAUSE,"KEY_PAUSE"},
    };
    return m;
}

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

        // Further check: does it have actual letter keys? (filter out mice etc.)
        bool is_keyboard = false;
        if (has_keys) {
            unsigned long keybits[(KEY_MAX + 7) / 8 / sizeof(unsigned long) + 1] = {};
            ioctl(fd, EVIOCGBIT(EV_KEY, sizeof(keybits)), keybits);

            // Check for KEY_A (30) as a rough keyboard heuristic
            int bit = KEY_A;
            if (keybits[bit / (sizeof(unsigned long) * 8)] &
                (1UL << (bit % (sizeof(unsigned long) * 8)))) {
                is_keyboard = true;
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
