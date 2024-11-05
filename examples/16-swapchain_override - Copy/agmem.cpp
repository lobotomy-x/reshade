/*
 * Copyright (C) 2024 Patrick Mours
 * SPDX-License-Identifier: BSD-3-Clause OR MIT
 */

#include <reshade.hpp>
#include <cstdlib>
#include <hook_manager.hpp>
#include <hook.hpp>
#include <hook_manager.cpp>
bool isGenshin = false;
reshade::hook setFovPtr;
std::filesystem::path get_module_path(HMODULE module) {
  WCHAR buf[4096];
  return GetModuleFileNameW(module, buf, ARRAYSIZE(buf))
             ? buf
             : std::filesystem::path();
}

static bool on_init_runtime(reshade::api::effect_runtime *runtime, void *) {

  std::filesystem::path  g_target_executable_path = get_module_path(nullptr);
  if (g_target_executable_path.stem().u8string() == "GenshinImpact.exe")
    isGenshin = true;



bool on_set_fullscreen_state(reshade::api::swapchain *, bool fullscreen, void *)
{
    if (isGenshin) {
      const auto module = reinterpret_cast<uintptr_t>(GetModuleHandle(nullptr));
      //    const auto offset_fog = 0xa4f4e0;
      // const auto offset_fps = 0x1339720;
      const auto offset = 0x1136f30;
      

      // reshade::hooks->install_internal("setFov ", reshade::hook & hook_method
      // export_hook)
      reshade::hooks->install(
          "setFov", reinterpret_cast<hook::address>(module + offset),
          reinterpret_cast<hook::address>(&setFovPtr), true);
      setFovPtr->apply_queued_actions();
      setFovPtr->call();
    }
  }

extern "C" __declspec(dllexport) const char *NAME = "Swap chain override";
extern "C" __declspec(dllexport) const char *DESCRIPTION = "Adds options to force the application into windowed or fullscreen mode, or force a specific resolution or the default refresh rate.\n\n"
	"These are controlled via ReShade.ini:\n"
	"[APP]\n"
	"ForceWindowed=<0/1>\n"
	"ForceFullscreen=<0/1>\n"
	"Force10BitFormat=<0/1>\n"
	"ForceDefaultRefreshRate=<0/1>\n"
	"ForceResolution=<width>,<height>";

BOOL APIENTRY DllMain(HMODULE hModule, DWORD fdwReason, LPVOID)
{
	switch (fdwReason)
	{
	case DLL_PROCESS_ATTACH:
		if (!reshade::register_addon(hModule))
			return FALSE;
		reshade::register_event<reshade::addon_event::init_effect_runtime>(on_init_runtime());
		reshade::register_event<reshade::addon_event::set_fullscreen_state>(on_set_fullscreen_state);
		break;
	case DLL_PROCESS_DETACH:
		reshade::unregister_addon(hModule);
		break;
	}

	return TRUE;
}
