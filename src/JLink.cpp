#include "JLink.h"
#include <filesystem>
#include <stdexcept>
namespace {
template<class T> T symbol(HMODULE m, const char* name) {
    auto p = GetProcAddress(m, name);
    if (!p) throw std::runtime_error(std::string("Missing J-Link export: ") + name);
    return reinterpret_cast<T>(p);
}
}
void JLink::connect(const std::string& dll, const std::string& device, unsigned speed) {
    close();
    if (device.empty() || device.find_first_of("\r\n") != std::string::npos)
        throw std::runtime_error("Enter an exact SEGGER STM32 device name.");
    auto path = std::filesystem::u8path(dll);
    if (!path.is_absolute()) throw std::runtime_error("Select an absolute path to JLink_x64.dll.");
    module_ = LoadLibraryExW(path.c_str(), nullptr, LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
    if (!module_) throw std::runtime_error("Cannot load 64-bit J-Link DLL. Windows error " + std::to_string(GetLastError()));
    try {
        open_ = symbol<decltype(open_)>(module_, "JLINKARM_Open");
        close_ = symbol<decltype(close_)>(module_, "JLINKARM_Close");
        exec_ = symbol<decltype(exec_)>(module_, "JLINKARM_ExecCommand");
        tif_ = symbol<decltype(tif_)>(module_, "JLINKARM_TIF_Select");
        speed_ = symbol<decltype(speed_)>(module_, "JLINKARM_SetSpeed");
        connect_ = symbol<decltype(connect_)>(module_, "JLINKARM_Connect");
        read_ = symbol<decltype(read_)>(module_, "JLINKARM_ReadMemEx");
        const char* error = open_();
        if (error) throw std::runtime_error(error);
        opened_ = true;
        char reply[1024]{};
        if (exec_(("Device = " + device).c_str(), reply, sizeof(reply)) < 0 || reply[0] != '\0')
            throw std::runtime_error(std::string("Device selection failed: ") + reply);
        if (tif_(1) < 0) throw std::runtime_error("Cannot select SWD interface.");
        speed_(speed);
        if (connect_() < 0) throw std::runtime_error("Target connection failed. Check power, SWD and device name.");
    } catch (...) { close(); throw; }
}
void JLink::read(uint32_t address, void* data, unsigned bytes) {
    if (!opened_ || read_(address, bytes, data, 0) != static_cast<int>(bytes))
        throw std::runtime_error("Target memory read failed at " + std::to_string(address));
}
void JLink::close() {
    if (opened_ && close_) close_();
    opened_ = false;
    if (module_) FreeLibrary(module_);
    module_ = nullptr;
}
