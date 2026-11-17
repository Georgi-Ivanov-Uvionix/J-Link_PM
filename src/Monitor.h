#pragma once
#include <array>
#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <mutex>
#include <string>
#include <thread>
#include <vector>
enum class ValueType { Float32, Int32, UInt32, Int16, UInt16, UInt8 };
struct Variable {
    char name[64]{};
    uint32_t address = 0;
    ValueType type = ValueType::Float32;
    double scale = 1, offset = 0;
    bool enabled = true;
};
struct Config {
    char dll[512] = "C:\\Program Files\\SEGGER\\JLink_V844a\\JLink_x64.dll";
    char device[128] = "";
    int speed = 1000;
    std::array<Variable,12> variables;
    Config();
};
struct Sample { double time = 0; std::array<double,12> values{}; };
struct Snapshot { std::vector<Sample> samples; std::string status; bool running; double readMs; uint64_t missed; };
unsigned valueSize(ValueType type);
double decodeValue(const void* bytes, ValueType type);
void validate(const Config& config, bool simulation);
void saveConfig(const Config&, const std::string& path);
Config loadConfig(const std::string& path);
void exportCsv(const Config&, const std::vector<Sample>&, const std::string& path);
class Monitor {
public:
    ~Monitor() { stop(); }
    void start(Config config, bool simulation);
    void stop();
    Snapshot snapshot();
private:
    std::thread worker_;
    std::atomic<bool> stop_{false}, running_{false};
    std::mutex mutex_;
    std::condition_variable wake_;
    std::deque<Sample> samples_;
    std::string status_ = "Stopped";
    double readMs_ = 0;
    uint64_t missed_ = 0;
};
