#include "Hooks.h"
#include "Manager.h"

namespace Hooks
{
	RE::TESCamera* g_mapCamera = nullptr;
	struct MapMenuCtor
	{
		static RE::MapMenu* thunk(RE::MapMenu* map)
		{
			g_mapCamera = &map->GetRuntimeData2()->camera;
			return func(map);
		};
		static inline REL::Relocation<decltype(thunk)> func;

	};

	// disable player map marker
	struct PlayerMarkerHook
	{
		static bool thunk(RE::BSTArray<RE::MapMenuMarker>* player, std::int64_t a2)
		{
			if (Manager::GetSingleton()->allowHidingPlayerMarker())
				return false;

			return func(player, a2);
		};
		static inline REL::Relocation<decltype(thunk)> func;
	};

	// block return to current location
	struct CurrentLocationReturnHook
	{
		static bool thunk(RE::MapMenu* mapMenu)
		{
			if (Manager::GetSingleton()->allowHidingPlayerMarker())
				return false;

			return func(mapMenu);
		};
		static inline REL::Relocation<decltype(thunk)> func;
	};

	// still allow showing quest targets
	struct ShowQuestOnMap
	{
		static void thunk(std::int64_t obj) // Ref FormID at (obj + 0x28) + 0x10
		{
			func(obj);

			//RE::FormID formID = (int)*(double*)(*(std::uintptr_t*)(obj + 0x28) + 0x10);
			//SKSE::log::info("FormID: {0:08X}", formID);

			Manager::GetSingleton()->setisShowingQuest(true);

		};
		static inline REL::Relocation<decltype(thunk)> func;
	};

	struct CameraStateHook
	{
		static RE::TESCameraState* thunk(RE::MapCamera* camera)
		{
			auto result = func(camera);
			if (camera != g_mapCamera)
			{
				return result;
			}

			Manager::GetSingleton()->handleCameraState(camera);

			return result;
		};
		static inline REL::Relocation<decltype(thunk)> func;
	};

	void InstallHooks()
	{
		// MapMenuCtor
		REL::Relocation<std::uintptr_t> mapCtorHook{ RELOCATION_ID(52248, 53139), 0x5D };
		stl::write_thunk_call<MapMenuCtor>(mapCtorHook.address());

		// PlayerMarkerHook
		REL::Relocation<std::uintptr_t> markerHook{ RELOCATION_ID(52221, 53108), 0x121 };
		stl::write_thunk_call<PlayerMarkerHook>(markerHook.address());

		// CurrentLocationReturnHook
		REL::Relocation<std::uintptr_t> loc1{ RELOCATION_ID(530215, 53098), 0x7 };
		stl::write_thunk_jump<CurrentLocationReturnHook>(loc1.address());

		REL::Relocation<std::uintptr_t> loc2{ RELOCATION_ID(52215, 53102), REL::Relocate(0x55D, 0x58F) };
		stl::write_thunk_call<CurrentLocationReturnHook>(loc2.address());

		REL::Relocation<std::uintptr_t> loc3{ RELOCATION_ID(52215, 53102), REL::Relocate(0x63B, 0x66D) };
		stl::write_thunk_call<CurrentLocationReturnHook>(loc3.address());

		// CameraStateHook
		REL::Relocation<std::uintptr_t> statePrologueHook{ RELOCATION_ID(32289, 33025) };
		stl::hook_function_prologue<CameraStateHook, 6>(statePrologueHook.address());

		// ShowQuestOnMap
		REL::Relocation<std::uintptr_t> questReturnFuncHook{ RELOCATION_ID(52274, 53169), REL::Relocate(0x110, 0x112) }; // 4C 8D 05 A9 16 00 00   =  lea     r8, sub_1408EB260
		stl::write_thunk_lea<ShowQuestOnMap>(questReturnFuncHook.address());

		SKSE::log::info("Installed Hooks!");
	}
}