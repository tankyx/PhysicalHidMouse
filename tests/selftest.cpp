// Runnable self-check: open the physical HID mouse and, with --move, emit a
// two-count wiggle. Returns 77 (skip) when no matching device is present.
#include <physical_hid_mouse.h>

#include <cstdio>
#include <cstring>

int main(int argc, char **argv) {
    phm::PhysicalHidMouse mouse;
    if (!mouse.open()) {
        std::fprintf(stderr, "selftest: no physical HID mouse (VID 0x%04x PID 0x%04x)\n",
                     phm::PhysicalHidMouse::kDefaultVendor, phm::PhysicalHidMouse::kDefaultProduct);
        return 77;
    }
    std::printf("selftest: opened %s\n", mouse.path().c_str());

    if (argc > 1 && std::strcmp(argv[1], "--move") == 0) {
        const bool moved = mouse.move(2, 0) && mouse.move(-2, 0);
        std::printf("selftest: move %s\n", moved ? "ok" : "FAILED");
        return moved ? 0 : 1;
    }
    return 0;
}
