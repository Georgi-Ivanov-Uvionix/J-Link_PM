#pragma once
#include <windows.h>
#include <cstdint>
#include <string>
class JLink {
public:
    JLink() = default;
    ~JLink() { close(); }
    JLink(const JLink&) = delete;
    JLink& operator=(const JLink&) = delete;
    void connect(const std::string& dll, const std::string& device, unsigned speed);
    void read(uint32_t address, void* data, unsigned bytes);
    void close();
private:
    HMODULE module_ = nullptr;
    bool opened_ = false;
    const char* (__cdecl *open_)() = nullptr;
    void (__cdecl *close_)() = nullptr;
    int (__cdecl *exec_)(const char*, char*, int) = nullptr;
    int (__cdecl *tif_)(int) = nullptr;
    void (__cdecl *speed_)(unsigned) = nullptr;
    int (__cdecl *connect_)() = nullptr;
    int (__cdecl *read_)(uint32_t, uint32_t, void*, uint32_t) = nullptr;
};
