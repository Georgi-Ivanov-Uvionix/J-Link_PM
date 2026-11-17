#include "Monitor.h"
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
void require(bool condition,const char* message) { if(!condition) throw std::runtime_error(message); }
int main() {
    try {
        int16_t negative=-1234; require(decodeValue(&negative,ValueType::Int16)==-1234,"signed decoding");
        float f=3.25f; require(decodeValue(&f,ValueType::Float32)==3.25,"float decoding");
        uint32_t u=0xFFFFFFFFu; require(decodeValue(&u,ValueType::UInt32)==4294967295.0,"unsigned decoding");
        Config c; bool rejected=false; try { validate(c,false); } catch(...) { rejected=true; } require(rejected,"zero hardware addresses rejected");
        auto path=(std::filesystem::temp_directory_path()/"STM32LiveMonitor-test.cfg").string();
        c.variables[0].address=0x20000000; c.variables[0].scale=0.001;
        saveConfig(c,path); auto loaded=loadConfig(path); require(loaded.variables[0].address==0x20000000 && loaded.variables[0].scale==0.001,"configuration roundtrip");
        {std::ofstream bad(path); bad << "broken";} rejected=false; try { loadConfig(path); } catch(...) {rejected=true;} require(rejected,"invalid configuration rejected");
        std::filesystem::remove(path);
        c.variables[1].enabled=false;
        Monitor monitor; monitor.start(c,true); std::this_thread::sleep_for(std::chrono::milliseconds(550)); monitor.stop();
        auto s=monitor.snapshot(); require(s.samples.size()>=3 && s.samples.size()<=9,"100 ms simulation cadence");
        require(std::isnan(s.samples.front().values[1]),"disabled variable missing");
        require(s.samples.back().time>s.samples.front().time,"monotonic timestamps");
        monitor.start(c,true); std::this_thread::sleep_for(std::chrono::milliseconds(150)); monitor.stop();
        require(monitor.snapshot().samples.size()<s.samples.size(),"restart clears history");
        std::cout << "Monitor tests passed\n"; return 0;
    } catch(const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
