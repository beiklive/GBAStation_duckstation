// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

namespace GBAStation::Paths
{
inline constexpr const char* Root = "sdmc:/GBAStation";
inline constexpr const char* Config = "sdmc:/GBAStation/config";
inline constexpr const char* Data = "sdmc:/GBAStation/ps1";
inline constexpr const char* BIOS = "sdmc:/GBAStation/bios/ps1";
inline constexpr const char* Memcards = "sdmc:/GBAStation/ps1/memcards";
inline constexpr const char* States = "sdmc:/GBAStation/ps1/states";
inline constexpr const char* Cache = "sdmc:/GBAStation/ps1/cache";
inline constexpr const char* Shaders = "sdmc:/GBAStation/ps1/cache/shaders";
inline constexpr const char* Pipelines = "sdmc:/GBAStation/ps1/cache/pipelines";
inline constexpr const char* Settings = "sdmc:/GBAStation/config/duckstation.ini";
inline constexpr const char* Launcher = "sdmc:/switch/GBAStation.nro";
inline constexpr const char* LauncherFallback = "sdmc:/GBAStation/GBAStation.nro";
} // namespace GBAStation::Paths
