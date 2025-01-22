/*
 * Copyright (C) 2021 Patrick Mours
 * SPDX-License-Identifier: BSD-3-Clause OR MIT
 */

#include <reshade.hpp>
#include "config.hpp"
#include "crc32_hash.hpp"
#include <cstring>
#include <fstream>
#include <filesystem>

using namespace reshade::api;
std::filesystem::path dump_path;
constexpr uint32_t SPIRV_MAGIC = 0x07230203;

static void save_shader_code(device_api device_type, const shader_desc &desc)
{
	if (desc.code_size == 0)
		return;

	uint32_t shader_hash = compute_crc32(static_cast<const uint8_t *>(desc.code), desc.code_size);

	const wchar_t *extension = L".cso";
	if (device_type == device_api::vulkan || (device_type == device_api::opengl && desc.code_size > sizeof(uint32_t) && *static_cast<const uint32_t *>(desc.code) == SPIRV_MAGIC))
		extension = L".spv"; // Vulkan uses SPIR-V (and sometimes OpenGL does too)
	else if (device_type == device_api::opengl)
		extension = desc.code_size > 5 && std::strncmp(static_cast<const char *>(desc.code), "!!ARB", 5) == 0 ? L".txt" : L".glsl"; // OpenGL otherwise uses plain text ARB assembly language or GLSL

	if (std::filesystem::exists(dump_path) == false)
		std::filesystem::create_directory(dump_path);

	wchar_t hash_string[11];
	swprintf_s(hash_string, L"0x%08X", shader_hash);

	dump_path /= hash_string;
	dump_path += extension;

	std::ofstream file(dump_path, std::ios::binary);
	file.write(static_cast<const char *>(desc.code), desc.code_size);
}

static bool on_create_pipeline(device *device, pipeline_layout, uint32_t subobject_count, const pipeline_subobject *subobjects)
{
	const device_api device_type = device->get_api();

	// Go through all shader stages that are in this pipeline and dump the associated shader code
	for (uint32_t i = 0; i < subobject_count; ++i)
	{
		switch (subobjects[i].type)
		{
		case pipeline_subobject_type::vertex_shader:
		case pipeline_subobject_type::hull_shader:
		case pipeline_subobject_type::domain_shader:
		case pipeline_subobject_type::geometry_shader:
		case pipeline_subobject_type::pixel_shader:
		case pipeline_subobject_type::compute_shader:
		case pipeline_subobject_type::amplification_shader:
		case pipeline_subobject_type::mesh_shader:
		case pipeline_subobject_type::raygen_shader:
		case pipeline_subobject_type::any_hit_shader:
		case pipeline_subobject_type::closest_hit_shader:
		case pipeline_subobject_type::miss_shader:
		case pipeline_subobject_type::intersection_shader:
		case pipeline_subobject_type::callable_shader:
			save_shader_code(device_type, *static_cast<const shader_desc *>(subobjects[i].data));
			break;
		}
	}

	return false;
}
/*
 need to change all the dump/repl addons to use a location relative to addon location or dll path to fix injection support
 most of these games will fail to startup with dumped shaders/textures in their game dir
 would make sense to use getconfigvalue addonPath but its possible to load addons without having the config value set
 could use reshade base path which would fix cases of external injection games failing to launch,
 but its possible to have that issue even in games that can load reshade as a dxgi wrapper
 my soln is to just set a global var and handle this during addon init. At first I just saved the module handle and ran this when needed
 but while I didnt have any issues doing so, ms docs say this can be a problem since module handle can change,
 I think maybe since we start dumping right away the path is not recalculated and the issue is avoided but it could've just been luck
 probably best soln is to just ensure addonpath gets set in the config when addons have been loaded and either write that as a fullpath
 or add specific handling to the api to resolve from basepath if getconfigvalue returns a blank path (tried this a while back could be misremembering my issue)
 but since these are barebones example addons and most people would prefer renodx which allows changing dump path already
 I think what I've done here is fine and it works as expected. If I missed an easier route or committed some cardinal sin of C++ do let me know
 Making this change to all of the default shader/texture addons so only leaving this notice once
*/
void set_dump_path(HMODULE mod) {
    wchar_t file_prefix[MAX_PATH] = L"";
    GetModuleFileNameW(mod, file_prefix, ARRAYSIZE(file_prefix));
	dump_path = file_prefix;
	dump_path = dump_path.parent_path();
   dump_path /= RESHADE_ADDON_SHADER_SAVE_DIR;
}

 extern "C" __declspec(dllexport) const char *NAME = "Shader Dump";
extern "C" __declspec(dllexport) const char *DESCRIPTION = "Example add-on that dumps all shader binaries used by the application to disk (\"" RESHADE_ADDON_SHADER_SAVE_DIR "\" directory).";

BOOL APIENTRY DllMain(HMODULE hModule, DWORD fdwReason, LPVOID)
{
	switch (fdwReason)
	{
	case DLL_PROCESS_ATTACH:
		if (!reshade::register_addon(hModule))
			return FALSE;
		reshade::register_event<reshade::addon_event::create_pipeline>(on_create_pipeline);
                set_dump_path(hModule);
		break;
	case DLL_PROCESS_DETACH:
		reshade::unregister_addon(hModule);
		break;
	}

	return TRUE;
}
