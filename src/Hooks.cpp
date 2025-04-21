#include "Hooks.h"
#include "Manager.h"
#include <windows.h>
#include <dbghelp.h>
#include <iostream>
#include <psapi.h>

namespace Hooks
{
	// disable player map marker
	struct PlayerMarkerHook
	{
		static bool thunk(RE::BSTArray<RE::MapMenuMarker>* playerMarker, std::int64_t a2)
		{
			if (Manager::GetSingleton()->isPlayerMarkerHidden())
				return false;

			return func(playerMarker, a2);
		};
		static inline REL::Relocation<decltype(thunk)> func;
	};

	// block return to current location
	struct CurrentLocationReturnHook
	{
		static bool thunk(RE::MapMenu* mapMenu)
		{
			if (Manager::GetSingleton()->isPlayerMarkerHidden())
				return false;

			return func(mapMenu);
		};
		static inline REL::Relocation<decltype(thunk)> func;
	};

	class RefHandleUIData : public RE::IUIMessageData {
	public:
		uint32_t refHandle;  // 10
		uint32_t pad14;      // 14
	};

	struct MapMenuProcessMessageHook
	{
		static bool isShowingQuestTarget(RE::IUIMessageData* data)
		{
			if (!data)
				return false;

			const auto handleData = static_cast<RefHandleUIData*>(data);
			if (!handleData)
				return false;

			return handleData->refHandle != 0;
		}

		static RE::UI_MESSAGE_RESULTS thunk(RE::MapMenu* a_menu, RE::UIMessage& a_message)
		{
			auto result = func(a_menu, a_message);

			if (a_message.type != RE::UI_MESSAGE_TYPE::kShow)
				return result;

			const auto manager = Manager::GetSingleton();
			const auto player = RE::PlayerCharacter::GetSingleton();

			if (!isShowingQuestTarget(a_message.data) && !manager->isParentInteriorCell(player))
			{
				if (REL::Module::IsVR())
					a_menu->GetVRRuntimeData2()->unk30530 = manager->getMarkerRefHandle(player);
				else
					a_menu->GetRuntimeData2()->unk30460 = manager->getMarkerRefHandle(player);
			}

			return result;
		}
		static inline REL::Relocation<decltype(thunk)> func;

		static void Install()
		{
			REL::Relocation<std::uintptr_t> Vtbl{ RE::VTABLE_MapMenu[0] };
			func = Vtbl.write_vfunc(0x4, &thunk);
		}
	};

	// Remove the default to Player reference when opening the map menu.
	// This function is only exectued if the camera opening handle in the MapMenu class is not the player and not in an interior cell!
	// Thats why we remove the if in the original code and replace it with our logic. In terms of compatibility this is still better than overwriting the call or whatever
	struct MapCameraCenterHook
	{
		static RE::NiPoint3* thunk(RE::MapCamera* camera, RE::NiPoint3* out, RE::RefHandle& refHandle, RE::TESObjectREFR* ref, RE::MapMenu* menu)
		{
			const auto playerHandle = *reinterpret_cast<RE::RefHandle*>(RELOCATION_ID(517013, 0).address());

			//SKSE::log::info("Handle: {0:08X}", refHandle);
			if (refHandle == 0 && camera->worldSpace)
			{
				//SKSE::log::info("Worldspace: {}", camera->worldSpace->GetFullName());

				out->x = (camera->worldSpace->minimumCoords.x + camera->worldSpace->maximumCoords.x) / 2;
				out->y = (camera->worldSpace->minimumCoords.y + camera->worldSpace->maximumCoords.y) / 2;
				out->z = 0.f;
			}
			else if (!ref || Manager::GetSingleton()->isParentInteriorCell(ref) || refHandle == playerHandle) // replace original logic
			{
				const auto pos = REL::Module::IsVR() ? menu->GetVRRuntimeData2()->playerMarkerPosition : menu->GetRuntimeData2()->playerMarkerPosition;
				*out = pos;
			}
			else
			{
				out = func(camera, out, refHandle); // call original function. Since we replaced the other logics no need to run useless funcs
			}

			return out;
		}
		static inline REL::Relocation<RE::NiPoint3* (*)(RE::MapCamera* camera, RE::NiPoint3* out, RE::RefHandle& refHandle)> func;

		struct Patch : Xbyak::CodeGenerator
		{
			Patch()
			{
				nop(26);
				mov(ptr[rsp + 0x20], rdi); // RE::MapMenu*
				mov(r9, ptr[rbp - 0x31]); // RE::TESObjectREFR*
			}
		};

		static void Install()
		{
			constexpr auto address = RELOCATION_ID(52214, 0);
			REL::Relocation<std::uintptr_t> fillAddress{ address, REL::Relocate(0x1E5, 0x0) };

			auto newCode = Patch();
			assert(newCode.getSize() == 35);

			REL::safe_write(fillAddress.address(), newCode.getCode(), newCode.getSize());

			REL::Relocation<std::uintptr_t> centerFunc{ address, REL::Relocate(0x220, 0x0) };
			stl::write_thunk_call<MapCameraCenterHook>(centerFunc.address());

		}
	};

	// hide compass markers
	struct CompassHook01
	{
		static bool thunk(void*, RE::NiPoint3*, const RE::RefHandle&, std::uint32_t)
		{
			return false;
		}
		static inline REL::Relocation<decltype(thunk)> func;
	};

	// hide quest target marker. Only show when player is in area
	struct CompassHook02
	{
		static bool thunk(void*, RE::NiPoint3*, const RE::RefHandle&, std::uint32_t)
		{
			return false;
		}
		static inline REL::Relocation<decltype(thunk)> func;
	};

	void InstallHooks()
	{
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

		MapMenuProcessMessageHook::Install();

		MapCameraCenterHook::Install();

		REL::Relocation<std::uintptr_t> compass01{ RELOCATION_ID(50870, 51744), REL::Relocate(0x450, 0x473) };
		stl::write_thunk_call<CompassHook01>(compass01.address());

		REL::Relocation<std::uintptr_t> compass02{ RELOCATION_ID(50826, 51691), REL::Relocate(0x114, 0x180) };
		stl::write_thunk_call<CompassHook02>(compass02.address());

		SKSE::log::info("Installed Hooks!");
	}
}