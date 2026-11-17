#include "Monitor.h"
#include "JLink.h"
#include <chrono>
#include <cmath>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <limits>
#include <stdexcept>
Config::Config() {
    const char* names[] = {"Voltage (V)","Current (A)","SOC (%)","Cell 1 (V)","Cell 2 (V)","Cell 3 (V)","Cell 4 (V)","Cell 5 (V)","Cell 6 (V)","Cell 7 (V)","Cell 8 (V)","Cell 9 (V)"};
    for (size_t i=0;i<variables.size();++i) strcpy_s(variables[i].name, names[i]);
}
unsigned valueSize(ValueType t) {
    switch(t) {
    case ValueType::Float32: case ValueType::Int32: case ValueType::UInt32: return 4;
    case ValueType::Int16: case ValueType::UInt16: return 2;
    case ValueType::UInt8: return 1;
    default: throw std::runtime_error("Invalid variable type");
    }
}
template<class T> double unpack(const void* p) { T value; std::memcpy(&value,p,sizeof(value)); return static_cast<double>(value); }
double decodeValue(const void* p, ValueType t) {
    switch(t) {
    case ValueType::Float32: return unpack<float>(p);
    case ValueType::Int32: return unpack<int32_t>(p);
    case ValueType::UInt32: return unpack<uint32_t>(p);
    case ValueType::Int16: return unpack<int16_t>(p);
    case ValueType::UInt16: return unpack<uint16_t>(p);
    case ValueType::UInt8: return unpack<uint8_t>(p);
    default: throw std::runtime_error("Invalid variable type");
    }
}
void validate(const Config& c, bool simulation) {
    if (c.speed < 5 || c.speed > 50000) throw std::runtime_error("SWD speed must be 5..50000 kHz.");
    bool any = false;
    for (const auto& v : c.variables) {
        auto size = valueSize(v.type);
        if (!std::isfinite(v.scale) || !std::isfinite(v.offset)) throw std::runtime_error("Scale and offset must be finite.");
        if (!v.enabled) continue;
        any = true;
        if (!simulation && (v.address == 0 || v.address > UINT32_MAX-(size-1) || v.address % size))
            throw std::runtime_error(std::string("Set a valid aligned RAM address for ") + v.name);
    }
    if (!any) throw std::runtime_error("Enable at least one variable.");
}
void saveConfig(const Config& c, const std::string& path) {
    validate(c,true);
    std::ofstream f(path); if (!f) throw std::runtime_error("Cannot save configuration.");
    f << "STM32LiveMonitor 1\n" << std::quoted(c.dll) << '\n' << std::quoted(c.device) << '\n' << c.speed << '\n' << std::setprecision(17);
    for (const auto& v:c.variables) f << std::quoted(v.name) << ' ' << v.address << ' ' << static_cast<int>(v.type) << ' ' << v.scale << ' ' << v.offset << ' ' << v.enabled << '\n';
    f.close(); if (!f) throw std::runtime_error("Configuration write failed.");
}
Config loadConfig(const std::string& path) {
    std::ifstream f(path); Config c; std::string magic,dll,device,name; int version;
    if (!(f >> magic >> version) || magic != "STM32LiveMonitor" || version != 1) throw std::runtime_error("Invalid configuration header.");
    if (!(f >> std::quoted(dll) >> std::quoted(device) >> c.speed) || dll.size()>=sizeof(c.dll) || device.size()>=sizeof(c.device)) throw std::runtime_error("Invalid connection settings.");
    strcpy_s(c.dll,dll.c_str()); strcpy_s(c.device,device.c_str());
    for (auto& v:c.variables) {
        int type,enabled; uint64_t address;
        if (!(f >> std::quoted(name) >> address >> type >> v.scale >> v.offset >> enabled) || name.size()>=sizeof(v.name) || address>UINT32_MAX || type<0 || type>5 || enabled<0 || enabled>1) throw std::runtime_error("Invalid variable configuration.");
        strcpy_s(v.name,name.c_str()); v.address=static_cast<uint32_t>(address); v.type=static_cast<ValueType>(type); v.enabled=enabled!=0;
    }
    f >> std::ws; if (!f.eof()) throw std::runtime_error("Unexpected trailing configuration data.");
    validate(c,true); return c;
}
void exportCsv(const Config& c,const std::vector<Sample>& samples,const std::string& path) {
    std::ofstream f(path); if (!f) throw std::runtime_error("Cannot create CSV file.");
    f << "elapsed_seconds";
    for (const auto& v:c.variables) { f << ",\""; for(char ch:std::string(v.name)) { if(ch=='\"') f << '\"'; f << ch; } f << '\"'; }
    f << '\n' << std::setprecision(17);
    for (const auto& s:samples) { f << s.time; for(auto v:s.values) { f << ','; if(std::isfinite(v)) f << v; } f << '\n'; }
    f.close(); if(!f) throw std::runtime_error("CSV write failed.");
}
void Monitor::stop() { stop_=true; wake_.notify_all(); if(worker_.joinable()) worker_.join(); running_=false; }
Snapshot Monitor::snapshot() {
    std::lock_guard<std::mutex> lock(mutex_);
    return {std::vector<Sample>(samples_.begin(),samples_.end()),status_,running_.load(),readMs_,missed_};
}
void Monitor::start(Config c,bool simulation) {
    validate(c,simulation); stop();
    { std::lock_guard<std::mutex> lock(mutex_); samples_.clear(); missed_=0; readMs_=0; status_="Connecting..."; }
    stop_=false; running_=true;
    worker_=std::thread([this,c,simulation] {
        try {
            JLink link;
            if(!simulation) link.connect(c.dll,c.device,static_cast<unsigned>(c.speed));
            using Clock=std::chrono::steady_clock;
            auto origin=Clock::now(), next=origin;
            while(!stop_) {
                auto begin=Clock::now(); Sample sample;
                sample.time=std::chrono::duration<double>(begin-origin).count();
                sample.values.fill(std::numeric_limits<double>::quiet_NaN());
                for(size_t i=0;i<c.variables.size();++i) {
                    const auto& v=c.variables[i]; if(!v.enabled) continue;
                    double raw;
                    if(simulation) raw = i==0 ? 36.9+0.3*std::sin(sample.time/4) : i==1 ? 2.0+std::sin(sample.time/2) : i==2 ? 78+2*std::sin(sample.time/20) : 4.1+0.025*std::sin(sample.time/4+static_cast<double>(i));
                    else { std::array<uint8_t,4> bytes{}; link.read(v.address,bytes.data(),valueSize(v.type)); raw=decodeValue(bytes.data(),v.type); }
                    sample.values[i]=raw*v.scale+v.offset;
                }
                auto end=Clock::now(); next+=std::chrono::milliseconds(100);
                uint64_t skipped=0;
                while(next<end) { next+=std::chrono::milliseconds(100); ++skipped; }
                std::unique_lock<std::mutex> lock(mutex_);
                samples_.push_back(sample); if(samples_.size()>6000) samples_.pop_front();
                readMs_=std::chrono::duration<double,std::milli>(end-begin).count(); missed_+=skipped;
                status_=simulation ? "SIMULATION - generated data" : "LIVE - J-Link SWD";
                wake_.wait_until(lock,next,[this]{return stop_.load();});
            }
            std::lock_guard<std::mutex> lock(mutex_); status_="Stopped - retained history";
        } catch(const std::exception& e) { std::lock_guard<std::mutex> lock(mutex_); status_=std::string("ERROR: ")+e.what(); }
        running_=false;
    });
}
