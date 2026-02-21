#include "config.hpp"
#include "evdev_util.hpp"

#include <fstream>
#include <sstream>
#include <cstdlib>
#include <sys/stat.h>
#include <algorithm>

namespace config {

std::string defaultPath() {
    const char* xdg = std::getenv("XDG_CONFIG_HOME");
    std::string base;
    if (xdg && xdg[0]) {
        base = xdg;
    } else {
        const char* home = std::getenv("HOME");
        base = home ? std::string(home) + "/.config" : "/tmp";
    }
    return base + "/hotkeyd/bindings.conf";
}

// Config format (one binding per line):
//   name<TAB>device_name<TAB>KEY_LEFTCTRL+KEY_A<TAB>/path/to/script.sh
// The device field stores the human-readable device name (e.g. "AT Translated Set 2 keyboard")
// which is resolved to a /dev/input/eventX path at daemon start.
// Lines starting with # are comments. Blank lines are skipped.
// Legacy 3-field format (name<TAB>keys<TAB>cmd) is also accepted (device = "").

std::vector<Binding> load(const std::string& path) {
    std::vector<Binding> result;
    std::ifstream ifs(path);
    if (!ifs.is_open()) return result;

    std::string line;
    while (std::getline(ifs, line)) {
        if (line.empty() || line[0] == '#') continue;

        // Split by tab into fields
        std::vector<std::string> fields;
        std::istringstream ss(line);
        std::string field;
        while (std::getline(ss, field, '\t')) {
            fields.push_back(field);
        }

        Binding b;
        if (fields.size() == 4) {
            // New format: name, device, keys, command
            b.name = fields[0];
            b.device = fields[1];
            b.keyCodes = stringToKeys(fields[2]);
            b.command = fields[3];
        } else if (fields.size() == 3) {
            // Legacy format: name, keys, command (no device)
            b.name = fields[0];
            b.keyCodes = stringToKeys(fields[1]);
            b.command = fields[2];
        } else {
            continue;
        }

        if (!b.keyCodes.empty() && !b.command.empty()) {
            result.push_back(std::move(b));
        }
    }
    return result;
}

static void mkdirs(const std::string& path) {
    // Create parent directories recursively
    std::string dir;
    for (size_t i = 0; i < path.size(); ++i) {
        dir += path[i];
        if (path[i] == '/' && i > 0) {
            mkdir(dir.c_str(), 0755);
        }
    }
}

bool save(const std::string& path, const std::vector<Binding>& bindings) {
    // Ensure parent directory exists
    auto slash = path.rfind('/');
    if (slash != std::string::npos) {
        mkdirs(path.substr(0, slash + 1));
    }

    std::ofstream ofs(path, std::ios::trunc);
    if (!ofs.is_open()) return false;

    ofs << "# hotkeyd bindings — do not edit while daemon is running\n";
    ofs << "# Format: name<TAB>device_name<TAB>keys<TAB>command\n";

    for (auto& b : bindings) {
        ofs << b.name << '\t'
            << b.device << '\t'
            << keysToString(b.keyCodes) << '\t'
            << b.command << '\n';
    }
    return ofs.good();
}

std::string keysToString(const std::set<int>& keys) {
    std::string result;
    for (auto it = keys.begin(); it != keys.end(); ++it) {
        if (it != keys.begin()) result += '+';
        result += evdev::keyName(*it);
    }
    return result;
}

std::set<int> stringToKeys(const std::string& s) {
    std::set<int> result;
    std::istringstream ss(s);
    std::string token;
    while (std::getline(ss, token, '+')) {
        int code = evdev::keyCode(token);
        if (code >= 0) result.insert(code);
    }
    return result;
}

} // namespace config
