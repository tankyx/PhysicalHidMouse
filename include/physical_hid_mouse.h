#pragma once
// Physical HID mouse output for Linux.
//
// Writes input reports to a real USB HID device -- an MCU (RP2040 / ESP32-P4)
// presenting itself as a mouse -- over /dev/hidraw. This is NOT /dev/uinput:
// the reports travel the USB bus, so the device is indistinguishable from an
// ordinary mouse to both the game and the OS.
//
// Protocol (RP2040_Aimer / ESP32-P4 bridge firmware):
//   output report id 2, 64-byte payload:
//     [0]     report id (2)          -- prepended for the hidraw write
//     [1..2]  dx  int16 little-endian
//     [3..4]  dy  int16 little-endian
//     [5]     button mask (bit0=Left bit1=Right bit2=Middle bit3=X1 bit4=X2)
//     [6..64] zero
//
// Requires write access to the matching /dev/hidraw* node: install the shipped
// udev rule, or add your user to the 'input' group.

#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include <dirent.h>
#include <fcntl.h>
#include <linux/hidraw.h>
#include <sys/ioctl.h>
#include <unistd.h>

namespace phm {

enum Button : uint8_t {
    Left   = 1u << 0,
    Right  = 1u << 1,
    Middle = 1u << 2,
    X1     = 1u << 3,
    X2     = 1u << 4,
};

class PhysicalHidMouse {
public:
    // Endgame Gear OP1 8K v2 -- the identity the ESP32-P4 bridge clones.
    static constexpr uint16_t kDefaultVendor  = 0x3367;
    static constexpr uint16_t kDefaultProduct = 0x1978;

    PhysicalHidMouse() = default;
    ~PhysicalHidMouse() { close(); }
    PhysicalHidMouse(const PhysicalHidMouse &) = delete;
    PhysicalHidMouse &operator=(const PhysicalHidMouse &) = delete;

    // Find and open the device. An empty `serial` matches on VID/PID alone.
    bool open(uint16_t vendor = kDefaultVendor, uint16_t product = kDefaultProduct,
              const std::string &serial = {}) {
        _vendor = vendor;
        _product = product;
        _serial = serial;
        return openDevice();
    }

    bool ok() const { return _fd >= 0; }
    int fd() const { return _fd; }
    const std::string &path() const { return _path; }

    // Relative movement in mouse counts; `buttons` is an OR of Button bits.
    // Reopens the device once if it has gone away.
    bool move(int dx, int dy, uint8_t buttons = 0) {
        if (_fd < 0 && !openDevice()) return false;
        if (dx < -32767) dx = -32767;
        else if (dx > 32767) dx = 32767;
        if (dy < -32767) dy = -32767;
        else if (dy > 32767) dy = 32767;

        const auto u16 = [](int v) { return static_cast<uint16_t>(static_cast<int16_t>(v)); };
        const uint16_t x = u16(dx), y = u16(dy);
        std::vector<uint8_t> report(65, 0);
        report[0] = 0x02;
        report[1] = x & 0xff;
        report[2] = (x >> 8) & 0xff;
        report[3] = y & 0xff;
        report[4] = (y >> 8) & 0xff;
        report[5] = buttons;
        return writeReport(report);
    }

    bool press(uint8_t buttons) { return move(0, 0, buttons); }
    bool release() { return move(0, 0, 0); }

    void close() {
        if (_fd >= 0) {
            ::close(_fd);
            _fd = -1;
        }
        _path.clear();
    }

private:
    bool openDevice() {
        close();
        DIR *dir = ::opendir("/dev");
        if (!dir) return false;
        for (dirent *e; (e = ::readdir(dir));) {
            if (std::strncmp(e->d_name, "hidraw", 6) != 0) continue;
            const std::string dev = std::string("/dev/") + e->d_name;

            int fd = ::open(dev.c_str(), O_RDWR);
            if (fd < 0) fd = ::open(dev.c_str(), O_RDONLY);
            if (fd < 0) continue;

            hidraw_devinfo info{};
            if (::ioctl(fd, HIDIOCGRAWINFO, &info) < 0 ||
                static_cast<uint16_t>(info.vendor) != _vendor ||
                static_cast<uint16_t>(info.product) != _product) {
                ::close(fd);
                continue;
            }
            if (!serialMatches(e->d_name)) {
                ::close(fd);
                continue;
            }
            ::close(fd);

            fd = ::open(dev.c_str(), O_RDWR);
            if (fd < 0) continue;
            _fd = fd;
            _path = dev;
            ::closedir(dir);
            return true;
        }
        ::closedir(dir);
        return false;
    }

    // Walk sysfs for the USB serial attribute. An empty target accepts any.
    bool serialMatches(const char *hidrawName) const {
        if (_serial.empty()) return true;
        std::string base = std::string("/sys/class/hidraw/") + hidrawName + "/device";
        for (int depth = 0; depth < 10; ++depth) {
            if (FILE *f = std::fopen((base + "/serial").c_str(), "r")) {
                char buf[128] = {0};
                const bool got = std::fgets(buf, sizeof(buf), f) != nullptr;
                std::fclose(f);
                if (got) {
                    std::string s(buf);
                    while (!s.empty() && (s.back() == '\n' || s.back() == '\r' || s.back() == ' '))
                        s.pop_back();
                    if (!s.empty()) return s == _serial;
                }
            }
            const auto slash = base.rfind('/');
            if (slash == std::string::npos || slash == 0) break;
            base.resize(slash);
        }
        return true;  // no serial attribute: accept on VID/PID
    }

    bool writeReport(const std::vector<uint8_t> &report) {
        const ssize_t n = ::write(_fd, report.data(), report.size());
        if (n == static_cast<ssize_t>(report.size())) return true;
        if (errno == ENODEV || errno == EIO) close();
        return false;
    }

    int _fd = -1;
    uint16_t _vendor = kDefaultVendor;
    uint16_t _product = kDefaultProduct;
    std::string _serial;
    std::string _path;
};

} // namespace phm
