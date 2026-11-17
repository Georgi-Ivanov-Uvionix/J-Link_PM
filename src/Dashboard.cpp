#include "Dashboard.h"
#include "Monitor.h"
#include "imgui.h"
#include "implot.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <windows.h>
namespace {
struct App {
    Config config, recorded;
    Monitor monitor;
    std::string message;
    std::filesystem::path folder;
    bool simulation = true, follow = true;
    App() {
        wchar_t path[32768]{};
        GetModuleFileNameW(nullptr,path,32768);
        folder=std::filesystem::path(path).parent_path();
        try { if(std::filesystem::exists(folder/"monitor.cfg")) config=loadConfig((folder/"monitor.cfg").string()); }
        catch(const std::exception& e) { message=e.what(); }
        recorded=config;
        monitor.start(config,true);
    }
};
}
void DrawDashboard() {
    static App app;
    auto snap=app.monitor.snapshot();
    auto* viewport=ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);
    ImGui::Begin("STM32 Live Monitor",nullptr,ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_NoMove|ImGuiWindowFlags_NoSavedSettings);
    ImGui::TextColored(ImVec4(0.35f,0.8f,1,1),"STM32 LIVE MONITOR");
    ImGui::TextWrapped("%s",snap.status.c_str());
    ImGui::Text("100 ms target | Read %.2f ms | Missed intervals %llu | Samples %zu",snap.readMs,static_cast<unsigned long long>(snap.missed),snap.samples.size());
    ImGui::BeginDisabled(snap.running);
    ImGui::Checkbox("Simulation",&app.simulation); ImGui::SameLine();
    if(ImGui::Button("Start / Connect")) {
        try { app.monitor.start(app.config,app.simulation); app.recorded=app.config; app.message.clear(); }
        catch(const std::exception& e) { app.message=e.what(); }
    }
    ImGui::EndDisabled(); ImGui::SameLine();
    if(ImGui::Button("Stop / Disconnect")) app.monitor.stop();
    ImGui::SameLine();
    if(ImGui::Button("Export retained history to CSV")) {
        SYSTEMTIME now; GetLocalTime(&now); char name[96];
        sprintf_s(name,"samples-%04u%02u%02u-%02u%02u%02u-%03u.csv",now.wYear,now.wMonth,now.wDay,now.wHour,now.wMinute,now.wSecond,now.wMilliseconds);
        auto file=app.folder/name;
        try { exportCsv(app.recorded,snap.samples,file.string()); app.message="Exported: "+file.string(); }
        catch(const std::exception& e) { app.message=e.what(); }
    }
    if(!app.message.empty()) ImGui::TextWrapped("%s",app.message.c_str());
    ImGui::Separator();
    if(ImGui::BeginTabBar("Views")) {
        if(ImGui::BeginTabItem("Dashboard & charts")) {
            if(!snap.running && !snap.samples.empty()) ImGui::TextColored(ImVec4(1,0.7f,0.3f,1),"HISTORICAL DATA - sampling is stopped");
            if(!snap.samples.empty()) {
                const auto& last=snap.samples.back();
                if(ImGui::BeginTable("Metrics",3,ImGuiTableFlags_Borders|ImGuiTableFlags_SizingStretchSame)) {
                    for(int i=0;i<3;++i) { ImGui::TableNextColumn(); ImGui::TextUnformatted(app.recorded.variables[i].name); if(std::isfinite(last.values[i])) ImGui::Text("%.3f",last.values[i]); else ImGui::TextUnformatted("Unavailable"); }
                    ImGui::EndTable();
                }
                ImGui::TextUnformatted("Nine-cell voltages (bar range 3.0-4.2 V)");
                if(ImGui::BeginTable("Cells",3,ImGuiTableFlags_SizingStretchSame)) {
                    for(int i=3;i<12;++i) { ImGui::TableNextColumn(); ImGui::PushID(i); ImGui::TextUnformatted(app.recorded.variables[i].name);
                        if(std::isfinite(last.values[i])) { char label[48]; sprintf_s(label,"%.3f V",last.values[i]); ImGui::ProgressBar(static_cast<float>(std::clamp((last.values[i]-3.0)/1.2,0.0,1.0)),ImVec2(-1,20),label); }
                        else ImGui::TextUnformatted("Unavailable"); ImGui::PopID(); }
                    ImGui::EndTable();
                }
                ImGui::Checkbox("Follow latest 60 seconds",&app.follow);
                std::vector<double> times; times.reserve(snap.samples.size());
                for(const auto& s:snap.samples) times.push_back(s.time);
                for(int group=0;group<4;++group) {
                    const char* titles[]={"Voltage history","Current history","SOC history","Cell voltage history"};
                    if(ImPlot::BeginPlot(titles[group],ImVec2(-1,200))) {
                        ImPlot::SetupAxes("Elapsed seconds",group==1?"A":group==2?"%":"V");
                        if(app.follow) ImPlot::SetupAxisLimits(ImAxis_X1,std::max(0.0,last.time-60),std::max(60.0,last.time),ImGuiCond_Always);
                        ImPlot::SetupAxis(ImAxis_Y1,nullptr,ImPlotAxisFlags_AutoFit);
                        for(int i=group;i<(group==3?12:group+1);++i) {
                            if(!app.recorded.variables[i].enabled) continue;
                            std::vector<double> values; values.reserve(snap.samples.size());
                            for(const auto& s:snap.samples) values.push_back(s.values[i]);
                            ImPlot::PlotLine(app.recorded.variables[i].name,times.data(),values.data(),static_cast<int>(times.size()));
                        }
                        ImPlot::EndPlot();
                    }
                }
            } else ImGui::TextUnformatted("No samples yet. Configure variables and start a session.");
            ImGui::EndTabItem();
        }
        if(ImGui::BeginTabItem("Connection & variables")) {
            ImGui::TextWrapped("Stop sampling to edit. Addresses default to zero and must be set from your firmware map/ELF before connecting. Values = raw * scale + offset. Disabled variables are not read.");
            ImGui::BeginDisabled(snap.running);
            ImGui::InputText("JLink_x64.dll (absolute path)",app.config.dll,sizeof(app.config.dll));
            ImGui::InputText("Exact STM32 device name",app.config.device,sizeof(app.config.device));
            ImGui::InputInt("SWD speed (kHz)",&app.config.speed);
            if(ImGui::BeginTable("Variables",6,ImGuiTableFlags_Borders|ImGuiTableFlags_RowBg|ImGuiTableFlags_SizingStretchProp)) {
                for(const char* heading:{"Read","Name / role","RAM address (hex)","Type","Scale","Offset"}) ImGui::TableSetupColumn(heading);
                ImGui::TableHeadersRow();
                for(size_t i=0;i<app.config.variables.size();++i) {
                    auto& v=app.config.variables[i]; ImGui::PushID(static_cast<int>(i));
                    ImGui::TableNextRow(); ImGui::TableNextColumn(); ImGui::Checkbox("##enabled",&v.enabled);
                    ImGui::TableNextColumn(); ImGui::SetNextItemWidth(-1); ImGui::InputText("##name",v.name,sizeof(v.name));
                    ImGui::TableNextColumn(); ImGui::SetNextItemWidth(-1); ImGui::InputScalar("##address",ImGuiDataType_U32,&v.address,nullptr,nullptr,"%08X",ImGuiInputTextFlags_CharsHexadecimal);
                    ImGui::TableNextColumn(); ImGui::SetNextItemWidth(-1); int type=static_cast<int>(v.type); if(ImGui::Combo("##type",&type,"float32\0int32\0uint32\0int16\0uint16\0uint8\0")) v.type=static_cast<ValueType>(type);
                    ImGui::TableNextColumn(); ImGui::SetNextItemWidth(-1); ImGui::InputDouble("##scale",&v.scale,0,0,"%.6g");
                    ImGui::TableNextColumn(); ImGui::SetNextItemWidth(-1); ImGui::InputDouble("##offset",&v.offset,0,0,"%.6g"); ImGui::PopID();
                }
                ImGui::EndTable();
            }
            if(ImGui::Button("Save configuration")) { try { saveConfig(app.config,(app.folder/"monitor.cfg").string()); app.message="Configuration saved."; } catch(const std::exception& e) { app.message=e.what(); } }
            ImGui::SameLine();
            if(ImGui::Button("Load configuration")) { try { app.config=loadConfig((app.folder/"monitor.cfg").string()); app.message="Configuration loaded."; } catch(const std::exception& e) { app.message=e.what(); } }
            ImGui::EndDisabled();
            ImGui::TextWrapped("Files are stored beside the executable: %s",app.folder.string().c_str());
            ImGui::TextWrapped("Roles are fixed by row: pack voltage, current, SOC, then cells 1-9. Use little-endian STM32 scalar variables. This application does not explicitly halt, reset, flash or write target memory. Reads of separate variables are not an atomic snapshot.");
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
    ImGui::End();
}
