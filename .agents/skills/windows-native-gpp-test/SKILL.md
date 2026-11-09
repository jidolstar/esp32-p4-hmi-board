---
name: windows-native-gpp-test
description: Prepare and verify Windows host g++ for PlatformIO native C/C++ tests, install MSYS2 UCRT64 GCC when explicitly authorized, repair PATH, and execute generated assertion programs. Use when native PlatformIO tests fail because g++ is missing or when verifying embedded logic on Windows without hardware upload.
---

# Windows Native g++ Test

Use `scripts/run_native_tests.ps1` for compiler discovery, approved installation, PATH handling, native build, and assertion execution.

## Workflow

1. Run without `-Install` first.
2. If g++ is missing, request approval before using `-Install`; it downloads MSYS2/GCC, uses about 500 MB, and updates the user PATH.
3. Pass only `platform = native` environments from `platformio.ini`.
4. Verify both `pio run` and the generated `program.exe`. A successful build alone does not execute assertions.
5. Report the compiler version, environments, build result, assertion result, and PATH restart requirement.

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File `
  .agents\skills\windows-native-gpp-test\scripts\run_native_tests.ps1
powershell.exe -NoProfile -ExecutionPolicy Bypass -File `
  .agents\skills\windows-native-gpp-test\scripts\run_native_tests.ps1 -Install
powershell.exe -NoProfile -ExecutionPolicy Bypass -File `
  .agents\skills\windows-native-gpp-test\scripts\run_native_tests.ps1 `
  -Environment cooking-device-operation-coordinator-test,manual-cooking-controller-test
```

## Guardrails

- Do not install when g++ is already available.
- Do not use `-Install` without approval.
- Never run upload, esptool, or hardware-mutating commands.
- Do not treat ESP cross-compilation as host assertion execution.
- Restrict environment names to letters, digits, and hyphens.
- Prepend `C:\msys64\ucrt64\bin` to the process PATH when DLL lookup fails.
