// SPDX-License-Identifier: GPL-3.0-or-later
// GBAStation-owned Switch frontend integration. Keep DuckStation core code unaware
// of the launcher, SD layout, and GBAStation configuration format.
#pragma once

#include "core/system.h"

#include <optional>
#include <string>

class SettingsInterface;

namespace GBAStation
{
void ConfigureFolders();
void ApplySettings(SettingsInterface& settings);
void ApplyDefaultSettings(SettingsInterface& settings);
void ConfigureInput(SettingsInterface& settings);

// Consumes GBAStation-only arguments and returns the content path stored in a
// launch JSON file when no positional content path was supplied.
bool ConsumeLaunchArgument(int& index, int argc, char* argv[], std::optional<SystemBootParameters>& autoboot);
bool LoadDefaultLaunchFile(std::optional<SystemBootParameters>& autoboot);

bool ShouldReturnToLauncher();
void ReturnToLauncher();
} // namespace GBAStation
