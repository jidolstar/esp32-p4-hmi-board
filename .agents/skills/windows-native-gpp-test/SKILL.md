---
name: windows-native-gpp-test
description: Prepare Windows host g++ and run standalone PlatformIO native diagnostic assertions. Use for diagnostic projects with a platform = native environment.
---

# Windows Native g++ Test

Use [run_native_tests.ps1](scripts/run_native_tests.ps1) for PlatformIO native diagnostics. For the Windows `main` and `sample-suite` profiles, use the CMake/Ninja build and CTest workflow in [change-scoped-verification](../change-scoped-verification/SKILL.md).

## Workflow

1. Run without `-Install` first.
2. Use `-Install` only when the user has authorized installing MSYS2/GCC and updating their user PATH.
3. Run from the standalone diagnostic directory that contains `platformio.ini`. Pass only environments declared there with `platform = native`.
4. Verify both `pio run` and the generated `program.exe`. A successful build alone does not execute assertions.
5. Report the compiler version, environments, build result, assertion result, and PATH restart requirement.

```powershell
Set-Location diagnostics/jc1060p470c_camera_face_probe/coordinate_transform_test
powershell.exe -NoProfile -ExecutionPolicy Bypass -File `
  ../../../.agents/skills/windows-native-gpp-test/scripts/run_native_tests.ps1 `
  -Environment coordinate-transform-test
```

## Guardrails

- Do not install when g++ is already available.
- Do not use `-Install` without approval.
- Never run upload, esptool, or hardware-mutating commands.
- Do not treat ESP cross-compilation as host assertion execution.
- Restrict environment names to letters, digits, and hyphens.
- Prepend `C:\msys64\ucrt64\bin` to the process PATH when DLL lookup fails.
