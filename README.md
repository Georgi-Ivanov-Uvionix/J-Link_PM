# STM32 Live Monitor

Windows x64 C++17 application for Visual Studio 2022, Dear ImGui, ImPlot, DirectX 11 and the SEGGER J-Link DLL. Recreated from the requirements in the previous chat: the previous sandbox downloads were not available as local source files.

## Open and build

1. Install/complete Visual Studio 2022 **Desktop development with C++**, including MSVC v143, a Windows 10/11 SDK, and C++ CMake tools for Windows.
2. Open `STM32LiveMonitor.sln`. Select **Release | x64**, then **Build Solution** and **Start Without Debugging**.
3. The executable is `bin/x64/Release/STM32LiveMonitor.exe`. It starts sampling generated data in simulation mode. No J-Link hardware is opened until you choose live mode and connect.

The dependency source files are included; no network download or package manager is required to build. A console window accompanies the GUI for diagnostics.

Alternatively open this folder in Visual Studio, select the `vs2022-x64` CMake preset, and build. From a VS 2022 Developer PowerShell:

```powershell
cmake --preset vs2022-x64
cmake --build --preset release
ctest --preset release
```

CMake builds the application and `MonitorTests`. Tests exercise decoding, validation, configuration roundtrip/corruption, simulated cadence, disabled variables and restart. Test timing is intentionally tolerant of ordinary Windows scheduling.

## Connect your STM32

1. Click **Stop / Disconnect** and open **Connection & variables**.
2. Select the absolute path to the installed **64-bit** `JLink_x64.dll`. The initial path matches the J-Link V8.44a installation found on this computer. Do not use the 32-bit `JLinkARM.dll` with this x64 application.
3. Enter the exact SEGGER STM32 device identifier for your chip. It is deliberately blank initially. Set SWD speed in kHz; 1000 is the default.
4. Obtain each variable's current RAM address from the matching firmware ELF or linker map. Enter hexadecimal addresses without `0x`. Zero addresses are placeholders and blocked in hardware mode. Disable unused rows.
5. Choose `float32`, `int32`, `uint32`, `int16`, `uint16`, or `uint8`. Values are little-endian; display value = raw value × scale + offset. For millivolts stored as `uint16`, select that type and scale `0.001`.
6. Rows have fixed dashboard roles: pack voltage, current, SOC, then cells 1–9. Keep units V, A, %, V respectively. Labels are editable; use unique labels for separate chart legends.
7. Save configuration, clear **Simulation**, and click **Start / Connect**. Use one USB J-Link probe and close other applications using it.

Configuration (`monitor.cfg`) and exported CSV files are stored beside the executable. The path is shown in the GUI. The next launch loads that configuration but still starts in simulation mode. Keep the executable in a writable folder.

The sampler owns the DLL on one worker thread and targets a monotonic 100 ms cadence. Windows and the probe do not provide a hard real-time guarantee. Slow reads skip elapsed deadlines; the dashboard shows missed intervals and read duration. A memory-read failure stops sampling and reports the error; retained values are explicitly marked historical. Stop may wait for an in-flight vendor DLL call.

Each chart holds at most 6000 samples (nominally ten minutes). **Export retained history to CSV** writes that window, with elapsed seconds and escaped variable names; missing/non-finite values are blank. This is snapshot export, not continuous logging. Disable “Follow latest 60 seconds” to pan/zoom retained history.

## Hardware limits

No explicit halt, reset, flash, or target-memory-write calls are used. Device-specific J-Link connection behavior can still affect a target; validate on your board. RAM reads while running depend on the MCU, debug protection, low-power state and cache coherency. Separate reads are not an atomic multi-variable snapshot. Volatile firmware variables, a coherent RAM telemetry block, and firmware-side synchronization may be needed. Automatic ELF symbol extraction is not implemented.

SEGGER's proprietary DLL is not redistributed. Use your installed J-Link software and comply with SEGGER's applicable SDK and probe license terms. The adapter dynamically resolves the required DLL exports and checks short reads and errors.

## Dependency sources

- Dear ImGui v1.91.9b: commit `f5befd2d29e66809cd1110a152e375a7f1981f06`, https://github.com/ocornut/imgui. MIT license included. `src/main.cpp` adapts its Win32/DirectX 11 example, preserving upstream window/render handling.
- ImPlot v0.17: commit `4707b245fbcd69075b1a8a74fa8d2435561b3134`, https://github.com/epezent/implot. MIT license included.
- J-Link information: https://www.segger.com/products/debug-probes/j-link/technology/j-link-sdk/ and https://kb.segger.com/Update_the_J-Link_DLL.
- DLL call signatures/read semantics cross-checked against the PyLink implementation: https://github.com/square/pylink/blob/master/pylink/jlink.py.

## Verification on this computer

See `VERIFICATION.md`. Compilation, GUI operation and physical J-Link communication must not be assumed successful until the local toolchain is repaired and those checks are run.
