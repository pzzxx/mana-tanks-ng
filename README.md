# Mana Tanks NG (SKSE)

Self-contained SKSE plugin — no ESP, no scripts. It polls the player's chosen
actor value and drives RaceMenu (skee64) body morphs directly.

## Requirements (runtime)

- SKSE64 (matching the game version)
- Address Library for SKSE Plugins
- RaceMenu (`skee64.dll`) — morphs are applied through its BodyMorph interface
- SKSE Menu Framework (v3) — provides the settings UI (Mod Control Panel)

## Build

Prerequisites: Visual Studio 2022+ ("Desktop development with C++"), CMake 3.21+,
vcpkg at `E:\vcpkg` (toolchain path is hardwired in `CMakePresets.json`).

```bat
cmake --preset release
cmake --build --preset release
```

CommonLibSSE-NG lives in `extern/CommonLibSSE-NG` (alandtse/CommonLibVR, `ng`
branch, shallow clone). First build compiles it from source (~10-15 min).

Output `ManaTanks.dll` is copied into `E:\MO2Mods\Mana Tanks NG\SKSE\Plugins\`
by the post-build step (override with `-DMO2_MOD_FOLDER=...`).

## Layout

- `src/plugin.cpp` — plugin entry, logging, SKSE messaging (menu registration, task kick)
- `src/Updater.cpp` — self-rescheduling per-frame task; computes AV percentage,
  applies SKEE morphs; handles enable/disable cleanup
- `src/Settings.cpp` — `Data/SKSE/Plugins/ManaTanks.json` load/save, defaults
- `src/UI.cpp` — SKSE Menu Framework pages ("Settings" + editable "Morphs" lists)
- `include/SKEE.h` — vendored skee64 interface declaration (vtable-compatible)
- `include/SKSEMenuFramework.h` — vendored consumer header (QTR-Modding v3)

## Version compatibility

Declared via `add_commonlibsse_plugin(... USE_ADDRESS_LIBRARY)` →
`SKSE::VersionIndependence::AddressLibrary`. No hardcoded addresses; all engine
calls go through CommonLibSSE-NG `REL::RelocationID`s, resolved at runtime by
Address Library. One binary covers SE 1.5.97 and all AE versions.
