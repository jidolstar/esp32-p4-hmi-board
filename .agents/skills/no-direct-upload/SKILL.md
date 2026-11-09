---
name: no-direct-upload
description: Prevent direct firmware upload, flashing, or device-mutating commands in this repository. Use whenever Codex is about to run or suggest running upload actions such as `pio run -t upload`, `platformio run -t upload`, `esptool`, serial flashing, firmware deployment to attached boards, or any command that writes to hardware.
---

# No Direct Upload

Do not directly upload, flash, or deploy firmware to attached hardware from Codex in this repository.

## Rule

- Do not run `pio run -t upload`, `platformio run -t upload`, `esptool`, serial flashing commands, or any equivalent command that writes firmware to a device.
- Building is allowed. Prefer `pio run` or the local PlatformIO executable path for compile verification.
- If hardware verification is needed, tell the user the build succeeded and ask them to upload or flash manually.
- If the user explicitly asks Codex to upload, pause and remind them that this repository's workflow requires the user to perform uploads directly.
- If an upload command was accidentally started and is still running, stop only the upload-related process tree that Codex launched, then report what happened.

## Safe Alternatives

Use these instead of uploading:

```powershell
& "$env:USERPROFILE\.platformio\penv\Scripts\pio.exe" run
```

For manual user-side upload, provide the command as text only:

```powershell
& "$env:USERPROFILE\.platformio\penv\Scripts\pio.exe" run -t upload
```

Do not execute the upload command yourself.
