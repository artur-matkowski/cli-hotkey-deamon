#pragma once

#include <string>
#include <vector>
#include <linux/input.h>

namespace evdev {

// Info about an input device
struct DeviceInfo {
    std::string path;   // e.g. /dev/input/event3
    std::string name;   // e.g. "AT Translated Set 2 keyboard"
    bool has_keys;      // supports EV_KEY
};

// Scan /dev/input/event* and return devices that look like keyboards
std::vector<DeviceInfo> detectKeyboards();

// Get the human-readable name of a device given its path (via EVIOCGNAME).
// Returns empty string on failure.
std::string getDeviceName(const std::string& path);

// Resolve a device name (e.g. "AT Translated Set 2 keyboard") to its current
// /dev/input/eventX path. Scans all event devices. Returns empty string if not found.
std::string resolvePathByName(const std::string& name);

// Open a device for reading. Returns fd or -1 on error.
int openDevice(const std::string& path);

// Grab exclusive access (other programs won't see the keys while grabbed)
bool grabDevice(int fd);
void ungrabDevice(int fd);

// Read one input_event (blocking). Returns false on error/EOF.
bool readEvent(int fd, struct input_event& ev);

// Human-readable name for a key code (e.g. KEY_A -> "KEY_A")
std::string keyName(int code);

// Reverse: "KEY_A" -> 30.  Returns -1 if unknown.
int keyCode(const std::string& name);

} // namespace evdev
