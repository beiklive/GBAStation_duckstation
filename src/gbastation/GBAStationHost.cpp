// SPDX-License-Identifier: GPL-3.0-or-later

#include "GBAStationHost.h"
#include "GBAStationPaths.h"

#include "core/controller.h"
#include "core/settings.h"
#include "core/system.h"
#include "util/input_manager.h"

#include "common/file_system.h"
#include "common/log.h"
#include "common/path.h"
#include "common/settings_interface.h"
#include "common/string_util.h"

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <map>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>

#ifdef __SWITCH__
#include <switch.h>
#include <switch/runtime/env.h>
#endif

Log_SetChannel(GBAStation);

namespace GBAStation
{
namespace
{
bool s_return_to_launcher = false;
std::string s_launcher_path;
std::string s_session_token;

std::string Trim(std::string_view value)
{
  const size_t first = value.find_first_not_of(" \t\r\n");
  if (first == std::string_view::npos)
    return {};
  const size_t last = value.find_last_not_of(" \t\r\n");
  return std::string(value.substr(first, last - first + 1));
}

std::string DecodeConfigValue(std::string_view value)
{
  std::string decoded = Trim(value);
  // The shared config file stores scalar values with a type prefix.  Core
  // settings such as resolutionScale are integers, so accepting s| alone
  // silently made them fall back to defaults.
  if (decoded.starts_with("i|") || decoded.starts_with("f|") || decoded.starts_with("s|"))
    decoded.erase(0, 2);

  std::string unescaped;
  unescaped.reserve(decoded.size());
  bool escaped = false;
  for (const char ch : decoded)
  {
    if (escaped)
    {
      unescaped.push_back(ch);
      escaped = false;
    }
    else if (ch == '\\')
    {
      escaped = true;
    }
    else
    {
      unescaped.push_back(ch);
    }
  }
  if (escaped)
    unescaped.push_back('\\');
  return unescaped;
}

std::map<std::string, std::string> LoadConfig()
{
  static constexpr const char* files[] = {"sdmc:/GBAStation/config/config.cfg", "/GBAStation/config/config.cfg"};
  for (const char* path : files)
  {
    std::ifstream input(path);
    if (!input)
      continue;
    std::map<std::string, std::string> values;
    std::string line;
    while (std::getline(input, line))
    {
      const size_t equals = line.find('=');
      if (equals != std::string::npos)
        values.emplace(Trim(std::string_view(line).substr(0, equals)),
                       DecodeConfigValue(std::string_view(line).substr(equals + 1)));
    }
    return values;
  }
  return {};
}

const std::string* Find(const std::map<std::string, std::string>& values, const char* key)
{
  const auto it = values.find(key);
  return it == values.end() ? nullptr : &it->second;
}

bool Enabled(std::string_view value)
{
  const std::string normalized(value);
  return StringUtil::Strcasecmp(normalized.c_str(), "enabled") == 0 ||
         StringUtil::Strcasecmp(normalized.c_str(), "true") == 0 || value == "1";
}

std::string TranslateBinding(std::string value)
{
  struct BindingName { const char* gba; const char* duck; };
  static constexpr BindingName names[] = {
    {"PAD_A", "P0/A"}, {"PAD_B", "P0/B"}, {"PAD_X", "P0/X"}, {"PAD_Y", "P0/Y"},
    {"PAD_UP", "P0/DPadUp"}, {"PAD_DOWN", "P0/DPadDown"}, {"PAD_LEFT", "P0/DPadLeft"},
    {"PAD_RIGHT", "P0/DPadRight"}, {"PAD_LB", "P0/L"}, {"PAD_RB", "P0/R"},
    {"PAD_LT", "P0/ZL"}, {"PAD_RT", "P0/ZR"}, {"PAD_ZL", "P0/ZL"}, {"PAD_ZR", "P0/ZR"},
    {"PAD_START", "P0/Plus"}, {"PAD_BACK", "P0/Minus"}, {"PAD_LSB", "P0/LStick"},
    {"PAD_RSB", "P0/RStick"}, {"PAD_L3", "P0/LStick"}, {"PAD_R3", "P0/RStick"},
  };
  for (const BindingName& name : names)
  {
    size_t position = 0;
    while ((position = value.find(name.gba, position)) != std::string::npos)
    {
      value.replace(position, std::strlen(name.gba), name.duck);
      position += std::strlen(name.duck);
    }
  }
  std::replace(value.begin(), value.end(), '+', '&');
  return value;
}

bool ReadJsonString(const std::string& text, const char* key, std::string& value)
{
  const std::string needle = std::string("\"") + key + "\"";
  const size_t key_position = text.find(needle);
  if (key_position == std::string::npos)
    return false;

  const size_t colon = text.find(':', key_position + needle.length());
  const size_t quote = colon == std::string::npos ? std::string::npos : text.find('"', colon + 1);
  if (quote == std::string::npos)
    return false;

  value.clear();
  bool escaped = false;
  for (size_t i = quote + 1; i < text.length(); i++)
  {
    const char ch = text[i];
    if (escaped)
    {
      value.push_back(ch);
      escaped = false;
    }
    else if (ch == '\\')
    {
      escaped = true;
    }
    else if (ch == '"')
    {
      return true;
    }
    else
    {
      value.push_back(ch);
    }
  }
  return false;
}

bool LoadLaunchFile(const char* path, std::optional<SystemBootParameters>& autoboot)
{
  std::ifstream input(path);
  if (!input)
    return false;

  std::ostringstream stream;
  stream << input.rdbuf();
  std::string content_path;
  const std::string json = stream.str();
  static constexpr const char* keys[] = {"contentPath", "content_path", "romPath", "rom", "path", "gamePath"};
  for (const char* key : keys)
  {
    if (ReadJsonString(json, key, content_path) && !content_path.empty())
      break;
  }
  if (content_path.empty())
    return false;

  if (!autoboot)
    autoboot.emplace();
  autoboot->filename = std::move(content_path);
  Log_InfoFmt("Loaded GBAStation launch data from '{}'", path);
  return true;
}

GenericInputBindingMapping GetSwitchMapping()
{
  return {
    {GenericInputBinding::Circle, "P0/A"},
    {GenericInputBinding::Cross, "P0/B"},
    {GenericInputBinding::Triangle, "P0/X"},
    {GenericInputBinding::Square, "P0/Y"},
    {GenericInputBinding::L3, "P0/LStick"},
    {GenericInputBinding::R3, "P0/RStick"},
    {GenericInputBinding::L1, "P0/L"},
    {GenericInputBinding::R1, "P0/R"},
    {GenericInputBinding::L2, "P0/ZL"},
    {GenericInputBinding::R2, "P0/ZR"},
    {GenericInputBinding::Start, "P0/Plus"},
    {GenericInputBinding::Select, "P0/Minus"},
    {GenericInputBinding::DPadLeft, "P0/DPadLeft"},
    {GenericInputBinding::DPadUp, "P0/DPadUp"},
    {GenericInputBinding::DPadRight, "P0/DPadRight"},
    {GenericInputBinding::DPadDown, "P0/DPadDown"},
    {GenericInputBinding::LeftStickLeft, "P0/-LeftX"},
    {GenericInputBinding::LeftStickRight, "P0/+LeftX"},
    {GenericInputBinding::LeftStickUp, "P0/-LeftY"},
    {GenericInputBinding::LeftStickDown, "P0/+LeftY"},
    {GenericInputBinding::RightStickLeft, "P0/-RightX"},
    {GenericInputBinding::RightStickRight, "P0/+RightX"},
    {GenericInputBinding::RightStickUp, "P0/-RightY"},
    {GenericInputBinding::RightStickDown, "P0/+RightY"},
  };
}
} // namespace

void ConfigureFolders()
{
  EmuFolders::DataRoot = Paths::Data;
  EmuFolders::Bios = Paths::BIOS;
  EmuFolders::Cache = Paths::Cache;
  EmuFolders::Cheats = Path::Combine(Paths::Data, "cheats");
  EmuFolders::Covers = Path::Combine(Paths::Data, "covers");
  EmuFolders::Dumps = Path::Combine(Paths::Data, "dumps");
  EmuFolders::GameSettings = Path::Combine(Paths::Data, "gamesettings");
  EmuFolders::InputProfiles = Path::Combine(Paths::Data, "inputprofiles");
  EmuFolders::MemoryCards = Paths::Memcards;
  EmuFolders::SaveStates = Paths::States;
  EmuFolders::Screenshots = Path::Combine(Paths::Data, "screenshots");
  EmuFolders::Shaders = Paths::Shaders;
  EmuFolders::Textures = Path::Combine(Paths::Data, "textures");
  EmuFolders::UserResources = Path::Combine(Paths::Data, "resources");
}

void ApplyDefaultSettings(SettingsInterface& settings)
{
  settings.SetStringValue("CPU", "ExecutionMode", "Recompiler");
  settings.SetStringValue("CPU", "FastmemMode", "MMap");
  settings.SetBoolValue("CPU", "RecompilerBlockLinking", true);
  settings.SetStringValue("GPU", "Renderer", "deko3D");
  settings.SetStringValue("MemoryCards", "Card1Type", "Shared");
  settings.SetStringValue("MemoryCards", "Card2Type", "Shared");
  settings.SetStringValue("MemoryCards", "Card1Path", "card1.mcd");
  settings.SetStringValue("MemoryCards", "Card2Path", "card2.mcd");
  settings.SetBoolValue("Main", "SaveStateOnExit", true);
  settings.SetBoolValue("Main", "CompressSaveStates", false);
  settings.SetBoolValue("Display", "ShowOSDMessages", true);
}

void ApplySettings(SettingsInterface& settings)
{
  ConfigureFolders();
  ApplyDefaultSettings(settings);
  settings.SetStringValue("BIOS", "SearchDirectory", Paths::BIOS);
  settings.SetStringValue("Folders", "Cache", Paths::Cache);
  settings.SetStringValue("MemoryCards", "Directory", Paths::Memcards);
  settings.SetStringValue("Folders", "SaveStates", Paths::States);
  settings.SetStringValue("Folders", "Shaders", Paths::Shaders);

  const auto config = LoadConfig();
  if (const std::string* value = Find(config, "ps1.renderer"))
    settings.SetStringValue("GPU", "Renderer", value->c_str());
  if (const std::string* value = Find(config, "ps1.resolutionScale"))
    settings.SetIntValue("GPU", "ResolutionScale", std::max(1, std::atoi(value->c_str())));
  if (const std::string* value = Find(config, "ps1.aspectRatio"))
    settings.SetStringValue("Display", "AspectRatio", value->c_str());
  if (const std::string* value = Find(config, "ps1.fastBoot"))
    settings.SetBoolValue("BIOS", "PatchFastBoot", Enabled(*value));
  if (const std::string* value = Find(config, "fastforward.multiplier"))
    settings.SetFloatValue("Main", "FastForwardSpeed", std::max(0.0f, std::strtof(value->c_str(), nullptr)));
  if (const std::string* value = Find(config, "save.autoSaveOnExit"))
    settings.SetBoolValue("Main", "SaveStateOnExit", Enabled(*value) || std::atoi(value->c_str()) != 0);
}

void ConfigureInput(SettingsInterface& settings)
{
  constexpr u32 controller = 0;
  const std::string section = Controller::GetSettingsSection(controller);
  settings.SetStringValue(section.c_str(), "Type", Settings::GetControllerTypeName(Settings::DEFAULT_CONTROLLER_1_TYPE));
  settings.SetBoolValue("InputSources", "Switch", true);
  InputManager::MapController(settings, controller, GetSwitchMapping());
  settings.SetBoolValue(section.c_str(), "ForceAnalogOnReset", true);
  settings.SetBoolValue(section.c_str(), "AnalogDPadInDigitalMode", true);

  // The launcher reserves its own overlay/menu bindings. DuckStation receives
  // only gameplay and directly usable core actions.
  settings.SetStringValue("Hotkeys", "FastForward", "P0/LStick");
  settings.SetStringValue("Hotkeys", "SaveSelectedSaveState", "P0/ZR");
  settings.SetStringValue("Hotkeys", "LoadSelectedSaveState", "P0/ZL");
  settings.SetStringValue("Hotkeys", "PowerOff", "P0/Plus & P0/Minus");

  const auto config = LoadConfig();
  if (const std::string* value = Find(config, "ps1.handle.fastforward"))
    settings.SetStringValue("Hotkeys", "FastForward", TranslateBinding(*value).c_str());
  if (const std::string* value = Find(config, "ps1.hotkey.quicksave.pad"))
    settings.SetStringValue("Hotkeys", "SaveSelectedSaveState", TranslateBinding(*value).c_str());
  else if (const std::string* value = Find(config, "ps1.hotkey.save"))
    settings.SetStringValue("Hotkeys", "SaveSelectedSaveState", TranslateBinding(*value).c_str());
  if (const std::string* value = Find(config, "ps1.hotkey.quickload.pad"))
    settings.SetStringValue("Hotkeys", "LoadSelectedSaveState", TranslateBinding(*value).c_str());
  else if (const std::string* value = Find(config, "ps1.hotkey.load"))
    settings.SetStringValue("Hotkeys", "LoadSelectedSaveState", TranslateBinding(*value).c_str());
  if (const std::string* value = Find(config, "ps1.hotkey.exit"))
    settings.SetStringValue("Hotkeys", "PowerOff", TranslateBinding(*value).c_str());
}

bool ConsumeLaunchArgument(int& index, int argc, char* argv[], std::optional<SystemBootParameters>& autoboot)
{
  const std::string_view argument(argv[index] ? argv[index] : "");
  if (argument == "--return")
  {
    if (index + 1 < argc && argv[index + 1] && argv[index + 1][0])
    {
      s_return_to_launcher = true;
      s_launcher_path = argv[++index];
    }
    return true;
  }
  if (argument == "--gbastation-session")
  {
    if (index + 1 < argc && argv[index + 1] && argv[index + 1][0])
      s_session_token = argv[++index];
    return true;
  }
  if (argument == "--launch" && index + 1 < argc && argv[index + 1])
    return LoadLaunchFile(argv[++index], autoboot);
  return false;
}

bool LoadDefaultLaunchFile(std::optional<SystemBootParameters>& autoboot)
{
  if (autoboot && !autoboot->filename.empty())
    return true;
  static constexpr const char* files[] = {
    "sdmc:/GBAStation/runtime/ps1_launch.json",
    "sdmc:/GBAStation/runtime/launch.json",
    "sdmc:/GBAStation/launch.json",
  };
  for (const char* path : files)
  {
    if (LoadLaunchFile(path, autoboot))
      return true;
  }
  return false;
}

bool ShouldReturnToLauncher()
{
  return s_return_to_launcher;
}

void ReturnToLauncher()
{
#ifdef __SWITCH__
  if (!s_return_to_launcher || !envHasNextLoad())
    return;
  const char* launcher = !s_launcher_path.empty() && FileSystem::FileExists(s_launcher_path.c_str()) ?
    s_launcher_path.c_str() :
    (FileSystem::FileExists(Paths::Launcher) ? Paths::Launcher : Paths::LauncherFallback);
  if (FileSystem::FileExists(launcher))
  {
    const std::string args = std::string(launcher) +
      (s_session_token.empty() ? " --resume" : " --external-return " + s_session_token);
    envSetNextLoad(launcher, args.c_str());
  }
#endif
}
} // namespace GBAStation
