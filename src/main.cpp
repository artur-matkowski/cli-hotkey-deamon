#include "evdev_util.hpp"
#include "config.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <csignal>
#include <string>
#include <vector>
#include <set>
#include <map>
#include <algorithm>
#include <unistd.h>
#include <sys/wait.h>
#include <linux/input.h>
#include <poll.h>

// ----------------------------------------------------------------
// Usage
// ----------------------------------------------------------------
static void usage(const char* argv0) {
    fprintf(stderr,
        "Usage:\n"
        "  %s --detect                        List keyboard devices\n"
        "  %s --record  -d <device> -n <name> -c <command>\n"
        "                                      Record a hotkey and bind it\n"
        "  %s --daemon  [-d <device>]          Run as hotkey daemon\n"
        "  %s --list                           List current bindings\n"
        "  %s --remove  -n <name>              Remove a binding by name\n"
        "\n"
        "Options:\n"
        "  -d <device>   Input device path (e.g. /dev/input/event3) or device name.\n"
        "                Use --detect to list available devices and their names.\n"
        "                For --record: required, specifies which device to record from.\n"
        "                              The device name is stored in config (not the path).\n"
        "                For --daemon: optional. If omitted, auto-resolves devices from bindings\n"
        "                              or detects all keyboards.\n"
        "  -n <name>     Binding label/name\n"
        "  -c <command>  Shell command to execute\n"
        "  -f <file>     Config file (default: ~/.config/hotkeyd/bindings.conf)\n"
        "\n"
        "Examples:\n"
        "  %s --detect\n"
        "  %s --record -d /dev/input/event3 -n my_script -c '/home/user/run.sh'\n"
        "  %s --daemon                         # auto-detect devices from bindings\n"
        "  %s --daemon -d /dev/input/event3    # monitor a specific device\n"
        "  %s --list\n"
        "  %s --remove -n my_script\n",
        argv0, argv0, argv0, argv0, argv0,
        argv0, argv0, argv0, argv0, argv0, argv0);
}

// ----------------------------------------------------------------
// --detect: list keyboards
// ----------------------------------------------------------------
static int cmdDetect() {
    auto kbds = evdev::detectKeyboards();
    if (kbds.empty()) {
        fprintf(stderr, "No keyboards found. Are you root or in the 'input' group?\n");
        return 1;
    }
    printf("Detected keyboards:\n");
    for (auto& kb : kbds) {
        printf("  %-24s  %s\n", kb.path.c_str(), kb.name.c_str());
    }
    return 0;
}

// ----------------------------------------------------------------
// --record: capture a key combo interactively
// ----------------------------------------------------------------
static int cmdRecord(const std::string& device, const std::string& name,
                     const std::string& command, const std::string& cfgPath) {
    if (device.empty() || name.empty() || command.empty()) {
        fprintf(stderr, "Error: --record requires -d <device>, -n <name>, -c <command>\n");
        return 1;
    }

    // Resolve device: if it's not a path, treat it as a device name
    std::string devPath = device;
    if (device.rfind("/dev/", 0) != 0) {
        devPath = evdev::resolvePathByName(device);
        if (devPath.empty()) {
            fprintf(stderr, "Error: no device found with name '%s'\n", device.c_str());
            return 1;
        }
        printf("Resolved device '%s' -> %s\n", device.c_str(), devPath.c_str());
    }

    int fd = evdev::openDevice(devPath);
    if (fd < 0) {
        perror("Cannot open device");
        fprintf(stderr, "Hint: run as root or add yourself to the 'input' group.\n");
        return 1;
    }

    printf("=== Recording hotkey for '%s' ===\n", name.c_str());
    printf("Press and HOLD your desired key combination...\n");
    printf("Release all keys when done. (Ctrl-C to cancel)\n\n");

    // Track currently held keys
    std::set<int> held;
    std::set<int> best;  // largest set of simultaneously held keys seen

    // We grab the device during recording so keystrokes don't leak
    evdev::grabDevice(fd);

    struct input_event ev;
    bool done = false;
    bool had_keys = false;

    while (!done && evdev::readEvent(fd, ev)) {
        if (ev.type != EV_KEY) continue;

        if (ev.value == 1) {  // key press
            held.insert(ev.code);
            had_keys = true;

            // Update best if current held set is larger
            if (held.size() > best.size()) {
                best = held;
            }

            // Print current state
            printf("\r  Held: %-60s",
                   config::keysToString(held).c_str());
            fflush(stdout);

        } else if (ev.value == 0) {  // key release
            held.erase(ev.code);

            // If all keys released after we captured something, we're done
            if (had_keys && held.empty()) {
                done = true;
            }
        }
        // ev.value == 2 is repeat — ignore
    }

    evdev::ungrabDevice(fd);
    close(fd);

    if (best.empty()) {
        fprintf(stderr, "\nNo keys captured. Aborted.\n");
        return 1;
    }

    printf("\n\nCaptured combo: %s\n", config::keysToString(best).c_str());
    printf("Command:        %s\n", command.c_str());

    // Load existing bindings, add/replace, save
    auto bindings = config::load(cfgPath);

    // Remove existing binding with same name
    bindings.erase(
        std::remove_if(bindings.begin(), bindings.end(),
                        [&](const Binding& b) { return b.name == name; }),
        bindings.end());

    // Resolve device path to its stable human-readable name
    std::string devName = evdev::getDeviceName(devPath);
    if (devName.empty()) {
        fprintf(stderr, "Warning: could not read device name for %s, storing path as fallback.\n",
                devPath.c_str());
        devName = devPath;
    } else {
        printf("Device name:    %s\n", devName.c_str());
    }

    Binding b;
    b.name = name;
    b.device = devName;
    b.keyCodes = best;
    b.command = command;
    bindings.push_back(std::move(b));

    if (config::save(cfgPath, bindings)) {
        printf("Saved to %s\n", cfgPath.c_str());
    } else {
        fprintf(stderr, "Error: failed to write config file %s\n", cfgPath.c_str());
        return 1;
    }

    return 0;
}

// ----------------------------------------------------------------
// --list: show bindings
// ----------------------------------------------------------------
static int cmdList(const std::string& cfgPath) {
    auto bindings = config::load(cfgPath);
    if (bindings.empty()) {
        printf("No bindings configured. (Config: %s)\n", cfgPath.c_str());
        return 0;
    }
    printf("%-20s  %-24s  %-40s  %s\n", "NAME", "DEVICE", "KEYS", "COMMAND");
    printf("%-20s  %-24s  %-40s  %s\n",
           "--------------------",
           "------------------------",
           "----------------------------------------",
           "-------");
    for (auto& b : bindings) {
        printf("%-20s  %-24s  %-40s  %s\n",
               b.name.c_str(),
               b.device.empty() ? "(any)" : b.device.c_str(),
               config::keysToString(b.keyCodes).c_str(),
               b.command.c_str());
    }
    return 0;
}

// ----------------------------------------------------------------
// --remove: delete a binding
// ----------------------------------------------------------------
static int cmdRemove(const std::string& name, const std::string& cfgPath) {
    if (name.empty()) {
        fprintf(stderr, "Error: --remove requires -n <name>\n");
        return 1;
    }

    auto bindings = config::load(cfgPath);
    size_t before = bindings.size();
    bindings.erase(
        std::remove_if(bindings.begin(), bindings.end(),
                        [&](const Binding& b) { return b.name == name; }),
        bindings.end());

    if (bindings.size() == before) {
        fprintf(stderr, "No binding named '%s' found.\n", name.c_str());
        return 1;
    }

    config::save(cfgPath, bindings);
    printf("Removed binding '%s'.\n", name.c_str());
    return 0;
}

// ----------------------------------------------------------------
// --daemon: listen for hotkeys and execute commands
// ----------------------------------------------------------------
static volatile sig_atomic_t g_running = 1;

static void sigHandler(int) {
    g_running = 0;
}

static void executeCommand(const std::string& cmd) {
    pid_t pid = fork();
    if (pid == 0) {
        // Child — run in background, detached
        setsid();
        execl("/bin/sh", "sh", "-c", cmd.c_str(), nullptr);
        _exit(127);
    }
    // Parent — don't wait (fire and forget), but reap zombies
    if (pid > 0) {
        // Non-blocking reap of any finished children
        while (waitpid(-1, nullptr, WNOHANG) > 0) {}
    }
}

static int cmdDaemon(const std::string& device, const std::string& cfgPath) {
    auto bindings = config::load(cfgPath);
    if (bindings.empty()) {
        fprintf(stderr, "No bindings configured. Use --record first.\n");
        return 1;
    }

    // Resolve device names to current paths.
    // Bindings store device names (e.g. "AT Translated Set 2 keyboard").
    // We resolve them to /dev/input/eventX paths at every daemon start.
    // The -d flag can be either a path or a device name.

    // Map: device name -> resolved path
    std::map<std::string, std::string> nameToPath;

    if (!device.empty()) {
        // -d flag given: could be a path or a name
        if (device.rfind("/dev/", 0) == 0) {
            // It's a path — look up its name
            std::string devName = evdev::getDeviceName(device);
            if (devName.empty()) devName = device;
            nameToPath[devName] = device;
        } else {
            // It's a device name — resolve to path
            std::string path = evdev::resolvePathByName(device);
            if (path.empty()) {
                fprintf(stderr, "Error: no device found with name '%s'\n", device.c_str());
                return 1;
            }
            nameToPath[device] = path;
        }
    } else {
        // Gather unique device names from bindings and resolve each
        std::set<std::string> deviceNames;
        for (auto& b : bindings) {
            if (!b.device.empty()) {
                deviceNames.insert(b.device);
            }
        }

        if (deviceNames.empty()) {
            // No bindings have device info (legacy config) — auto-detect
            auto kbds = evdev::detectKeyboards();
            if (kbds.empty()) {
                fprintf(stderr, "No keyboards found. Are you root or in the 'input' group?\n");
                return 1;
            }
            for (auto& kb : kbds) {
                nameToPath[kb.name] = kb.path;
            }
        } else {
            for (auto& devName : deviceNames) {
                std::string path = evdev::resolvePathByName(devName);
                if (path.empty()) {
                    fprintf(stderr, "Warning: device '%s' not found (skipping)\n", devName.c_str());
                    continue;
                }
                nameToPath[devName] = path;
            }
        }
    }

    // Open all resolved devices
    // Maps: path -> fd, fd -> device name (for binding matching)
    std::map<std::string, int> pathToFd;
    std::map<int, std::string> fdToName;

    for (auto& [devName, path] : nameToPath) {
        int fd = evdev::openDevice(path);
        if (fd < 0) {
            fprintf(stderr, "Warning: cannot open %s (%s) (skipping)\n",
                    path.c_str(), devName.c_str());
            continue;
        }
        pathToFd[path] = fd;
        fdToName[fd] = devName;
    }

    if (pathToFd.empty()) {
        fprintf(stderr, "Error: could not open any input devices.\n");
        return 1;
    }

    signal(SIGINT, sigHandler);
    signal(SIGTERM, sigHandler);

    printf("hotkeyd daemon started (%zu devices, %zu bindings)\n",
           pathToFd.size(), bindings.size());
    printf("Press Ctrl-C to stop.\n");

    printf("\nDevices:\n");
    for (auto& [devName, path] : nameToPath) {
        printf("  %-24s  %s\n", path.c_str(), devName.c_str());
    }

    printf("\nBindings:\n");
    for (auto& b : bindings) {
        printf("  [%s] %s %s -> %s\n",
               b.name.c_str(),
               b.device.c_str(),
               config::keysToString(b.keyCodes).c_str(),
               b.command.c_str());
    }
    printf("\n");

    // Per-device held keys (keyed by device name)
    std::map<std::string, std::set<int>> held;

    // Track which bindings have already fired (to avoid repeat-firing
    // while keys are held down). Reset when the combo is released.
    std::map<std::string, bool> fired;

    // Build pollfd array
    std::vector<struct pollfd> pfds;
    for (auto& [path, fd] : pathToFd) {
        (void)path;
        struct pollfd pfd;
        pfd.fd = fd;
        pfd.events = POLLIN;
        pfds.push_back(pfd);
    }

    while (g_running) {
        int ret = poll(pfds.data(), pfds.size(), 500);  // 500ms timeout for signal checking
        if (ret <= 0) continue;

        for (auto& pfd : pfds) {
            if (!(pfd.revents & POLLIN)) continue;

            struct input_event ev;
            if (!evdev::readEvent(pfd.fd, ev)) continue;
            if (ev.type != EV_KEY) continue;

            const std::string& devName = fdToName[pfd.fd];

            if (ev.value == 1) {        // press
                held[devName].insert(ev.code);
            } else if (ev.value == 0) { // release
                held[devName].erase(ev.code);
            } else {
                continue;  // repeat — ignore
            }

            // Check bindings that belong to this device
            for (auto& b : bindings) {
                // Match binding to device: if binding has a device name set, it must
                // match the event's device name. If binding has no device (legacy),
                // match against any device.
                if (!b.device.empty() && b.device != devName) continue;

                const auto& devHeld = held[devName];
                bool match = std::includes(devHeld.begin(), devHeld.end(),
                                            b.keyCodes.begin(), b.keyCodes.end());
                if (match && !fired[b.name]) {
                    printf("[trigger] %s (%s) -> %s\n",
                           b.name.c_str(), devName.c_str(), b.command.c_str());
                    executeCommand(b.command);
                    fired[b.name] = true;
                } else if (!match && fired[b.name]) {
                    fired[b.name] = false;
                }
            }
        }
    }

    // Close all devices
    for (auto& [path, fd] : pathToFd) {
        (void)path;
        close(fd);
    }
    printf("\nDaemon stopped.\n");
    return 0;
}

// ----------------------------------------------------------------
// main
// ----------------------------------------------------------------
int main(int argc, char* argv[]) {
    if (argc < 2) {
        usage(argv[0]);
        return 1;
    }

    enum Mode { NONE, DETECT, RECORD, DAEMON, LIST, REMOVE };
    Mode mode = NONE;

    std::string device;
    std::string name;
    std::string command;
    std::string cfgPath = config::defaultPath();

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];

        if (arg == "--detect")      { mode = DETECT; }
        else if (arg == "--record") { mode = RECORD; }
        else if (arg == "--daemon") { mode = DAEMON; }
        else if (arg == "--list")   { mode = LIST; }
        else if (arg == "--remove") { mode = REMOVE; }
        else if (arg == "-d" && i + 1 < argc) { device = argv[++i]; }
        else if (arg == "-n" && i + 1 < argc) { name = argv[++i]; }
        else if (arg == "-c" && i + 1 < argc) { command = argv[++i]; }
        else if (arg == "-f" && i + 1 < argc) { cfgPath = argv[++i]; }
        else if (arg == "--help" || arg == "-h") {
            usage(argv[0]);
            return 0;
        } else {
            fprintf(stderr, "Unknown argument: %s\n", arg.c_str());
            usage(argv[0]);
            return 1;
        }
    }

    switch (mode) {
        case DETECT: return cmdDetect();
        case RECORD: return cmdRecord(device, name, command, cfgPath);
        case DAEMON: return cmdDaemon(device, cfgPath);
        case LIST:   return cmdList(cfgPath);
        case REMOVE: return cmdRemove(name, cfgPath);
        default:
            fprintf(stderr, "Error: specify one of --detect, --record, --daemon, --list, --remove\n");
            usage(argv[0]);
            return 1;
    }
}
