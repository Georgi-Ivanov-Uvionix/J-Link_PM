# Verification — 17 November 2026

- Prior chat inspected; it had sandbox download links but no available attachments or source text. No existing STM32LiveMonitor project was found under the requested Projects directory.
- Vendored dependency tags were downloaded from their upstream GitHub repositories and their commit IDs recorded in README.md.
- Both Visual Studio solution/project files and CMake project/presets are supplied.
- The available Visual Studio installation identifies as **Visual Studio Community 2026 (18.12)**, incomplete and not launchable. Visual Studio 2022 was not found. No usable MSVC `cl.exe`, Windows SDK include directory or CMake executable was found in the checked standard locations.
- Attempting to build the solution with the installed MSBuild failed before project compilation: `System.IO.FileLoadException`, assembly `System.Memory, Version=4.0.2.0`, manifest mismatch (`0x80131040`). This is an installed-toolchain failure, not a successful source compilation check.
- Automated C++ tests are provided but could not be executed with this installation. GUI and physical hardware validation have not been performed.

To finish validation: complete/repair the C++ installation, build Release x64, run the CMake tests, run the simulation and verify charts/CSV/settings, then configure the exact MCU and known RAM symbols and compare readings against firmware/debugger values. No firmware or target-memory changes were attempted.
