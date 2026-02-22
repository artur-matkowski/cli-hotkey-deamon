CXX      := g++
CXXFLAGS := -std=c++17 -Wall -Wextra -O2
LDFLAGS  :=

SRCDIR := src
OBJDIR := build
TARGET := hotkeyd

SRCS := $(wildcard $(SRCDIR)/*.cpp)
OBJS := $(patsubst $(SRCDIR)/%.cpp,$(OBJDIR)/%.o,$(SRCS))

.PHONY: all clean install uninstall deps install-daemon uninstall-daemon

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

$(OBJDIR)/%.o: $(SRCDIR)/%.cpp | $(OBJDIR)
	$(CXX) $(CXXFLAGS) -c -o $@ $<

$(OBJDIR):
	mkdir -p $(OBJDIR)

clean:
	rm -rf $(OBJDIR) $(TARGET)

install: $(TARGET)
	install -Dm755 $(TARGET) /usr/local/bin/$(TARGET)

uninstall:
	rm -f /usr/local/bin/$(TARGET)

# --- Install build dependencies ---
deps:
	@if command -v apt-get >/dev/null 2>&1; then \
		sudo apt-get update && sudo apt-get install -y g++ make linux-libc-dev; \
	elif command -v dnf >/dev/null 2>&1; then \
		sudo dnf install -y gcc-c++ make kernel-headers; \
	elif command -v pacman >/dev/null 2>&1; then \
		sudo pacman -S --needed --noconfirm gcc make linux-api-headers; \
	elif command -v zypper >/dev/null 2>&1; then \
		sudo zypper install -y gcc-c++ make linux-glibc-devel; \
	else \
		echo "Error: unsupported package manager. Install g++, make, and linux headers manually."; \
		exit 1; \
	fi

# --- Systemd daemon management ---
install-daemon: install
	install -Dm644 hotkeyd.service /etc/systemd/system/hotkeyd.service
	systemctl daemon-reload
	systemctl enable hotkeyd.service
	systemctl start hotkeyd.service
	@echo "hotkeyd daemon installed and started."

uninstall-daemon:
	-systemctl stop hotkeyd.service
	-systemctl disable hotkeyd.service
	rm -f /etc/systemd/system/hotkeyd.service
	systemctl daemon-reload
	@echo "hotkeyd daemon removed."
