#pragma once

#include <string>
#include <vector>
#include <set>

struct Binding {
    std::string name;           // user-given label
    std::string device;         // input device name (e.g. "AT Translated Set 2 keyboard")
    std::set<int> keyCodes;     // set of evdev key codes held simultaneously
    std::string command;        // shell command to run
};

namespace config {

// Default config path: ~/.config/hotkeyd/bindings.conf
std::string defaultPath();

// Load bindings from file. Returns empty vector if file doesn't exist.
std::vector<Binding> load(const std::string& path);

// Save bindings to file (overwrites). Creates parent dirs if needed.
bool save(const std::string& path, const std::vector<Binding>& bindings);

// Serialize a key set to a human-readable string, e.g. "KEY_LEFTCTRL+KEY_LEFTALT+KEY_T"
std::string keysToString(const std::set<int>& keys);

// Parse that string back into a key set
std::set<int> stringToKeys(const std::string& s);

} // namespace config
