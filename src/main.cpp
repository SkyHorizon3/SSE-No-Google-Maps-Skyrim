#include "Hooks.h"
#include "Manager.h"

HMODULE g_hModule = nullptr;

static void DrawMenu(reshade::api::effect_runtime*)
{
	Manager::GetSingleton()->draw();
}

bool Load()
{
	if (reshade::register_addon(g_hModule))
	{
		SKSE::log::info("Registered addon for ReShade.");
		reshade::register_overlay(nullptr, &DrawMenu);
		return true;
	}
	else
	{
		SKSE::log::info("ReShade not present.");
		return false;
	}
}

void MessageListener(SKSE::MessagingInterface::Message* message)
{
	switch (message->type)
	{
		// https://github.com/ianpatt/skse64/blob/09f520a2433747f33ae7d7c15b1164ca198932c3/skse64/PluginAPI.h#L193-L212	
	case SKSE::MessagingInterface::kDataLoaded:
	{
		Manager::GetSingleton()->parseINI();
	}
	break;


	default:
		break;

	}
}

#define DLLEXPORT __declspec(dllexport)

extern "C" DLLEXPORT const char* NAME = "No Google Maps Skyrim";
extern "C" DLLEXPORT const char* DESCRIPTION = "No Google Maps Skyrim by SkyHorizon.";

extern "C" DLLEXPORT constinit auto SKSEPlugin_Version = []()
	{
		SKSE::PluginVersionData v;
		v.PluginName(Plugin::NAME);
		v.PluginVersion(Plugin::VERSION);
		v.AuthorName("SkyHorizon"sv);
		v.UsesAddressLibrary();
		v.UsesNoStructs();
		return v;
	}
();

extern "C" DLLEXPORT bool SKSEAPI SKSEPlugin_Query(const SKSE::QueryInterface*, SKSE::PluginInfo* pluginInfo)
{
	pluginInfo->name = SKSEPlugin_Version.pluginName;
	pluginInfo->infoVersion = SKSE::PluginInfo::kVersion;
	pluginInfo->version = SKSEPlugin_Version.pluginVersion;
	return true;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD dwReason, LPVOID)
{
	if (dwReason == DLL_PROCESS_ATTACH)
	{
		g_hModule = hModule;
	}
	else if (dwReason == DLL_PROCESS_DETACH)
	{
		reshade::unregister_overlay(nullptr, &DrawMenu);
		reshade::unregister_addon(hModule);
	}

	return TRUE;
}

SKSEPluginLoad(const SKSE::LoadInterface* skse)
{
	SKSE::Init(skse, true);

	spdlog::set_pattern("[%H:%M:%S:%e] [%l] %v"s);

#ifndef NDEBUG
	spdlog::set_level(spdlog::level::trace);
	spdlog::flush_on(spdlog::level::trace);
#else
	spdlog::set_level(spdlog::level::info);
	spdlog::flush_on(spdlog::level::info);
#endif

	SKSE::log::info("Game version: {}", skse->RuntimeVersion());

	if (REL::Module::IsVR())
	{
		SKSE::stl::report_and_fail("No Google Maps Skyrim does not support VR atm."sv);
	}

	Load();

	SKSE::AllocTrampoline(70);
	Hooks::InstallHooks();

	SKSE::GetMessagingInterface()->RegisterListener(MessageListener);

	return true;
}