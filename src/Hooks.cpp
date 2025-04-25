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
		static bool thunk(RE::BSTArray<RE::MapMenuMarker>* playerMarker, RE::NiPoint3* playerMarkerPos)
		{
			if (Manager::GetSingleton()->isPlayerMarkerHidden())
				return false;

			return func(playerMarker, playerMarkerPos);
		};

		static bool thunkVR(RE::BSTArray<RE::MapMenuMarker>* playerMarker, RE::NiPoint3* playerMarkerPos, RE::NiPoint3* unk)
		{
			if (Manager::GetSingleton()->isPlayerMarkerHidden())
				return false;

			return funcVR(playerMarker, playerMarkerPos, unk);
		};

		static inline REL::Relocation<decltype(thunk)> func;
		static inline REL::Relocation<decltype(thunkVR)> funcVR;

		static void Install()
		{
			REL::Relocation<std::uintptr_t> markerHook{ REL::VariantID(52221, 53108, 0x9184C0), REL::Relocate(0x121,0x121,0x124) };

			if (REL::Module::IsVR())
				funcVR = SKSE::GetTrampoline().write_call<5>(markerHook.address(), thunkVR);
			else
				func = SKSE::GetTrampoline().write_call<5>(markerHook.address(), thunk);
		}
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

			return handleData->refHandle != Utils::getPlayerCharacterHandle();
		}

		static RE::UI_MESSAGE_RESULTS thunk(RE::MapMenu* a_menu, RE::UIMessage& a_message)
		{
			auto result = func(a_menu, a_message);

			if (a_message.type != RE::UI_MESSAGE_TYPE::kShow)
				return result;

			const auto manager = Manager::GetSingleton();
			const auto player = RE::PlayerCharacter::GetSingleton();

			if (!manager->isParentInteriorCell(player) && !isShowingQuestTarget(a_message.data))
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
			const auto playerHandle = Utils::getPlayerCharacterHandle();

			if (refHandle == 0 && camera->worldSpace)
			{
				//SKSE::log::info("Worldspace: {}", camera->worldSpace->GetFullName());

				out->x = (camera->worldSpace->minimumCoords.x + camera->worldSpace->maximumCoords.x) / 2;
				out->y = (camera->worldSpace->minimumCoords.y + camera->worldSpace->maximumCoords.y) / 2;
				out->z = 0.f;
			}
			else if (!ref || refHandle == playerHandle || Manager::GetSingleton()->isParentInteriorCell(ref)) // replace original logic
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
			constexpr auto address = REL::VariantID(52214, 53101, 0x91B360); // Inlined in VR, need to rewrite parts for support
			REL::Relocation<std::uintptr_t> fillAddress{ address, REL::Relocate(0x1E5, 0x1E7) };

			auto newCode = Patch();
			assert(newCode.getSize() == 35);

			REL::safe_write(fillAddress.address(), newCode.getCode(), newCode.getSize());

			REL::Relocation<std::uintptr_t> centerFunc{ address, REL::Relocate(0x220, 0x222) };
			stl::write_thunk_call<MapCameraCenterHook>(centerFunc.address());

		}
	};

	// hide compass markers
	struct CompassHook01
	{
		// function parameters different in VR
		static bool thunk(void* unk, void* someScaleformInformation, RE::NiPoint3* pos, const RE::RefHandle& handle, std::uint32_t markerGotoFrame)
		{
			if (Manager::GetSingleton()->areCompassMarkersHidden())
				return false;

			return func(unk, someScaleformInformation, pos, handle, markerGotoFrame);
		}
		static inline REL::Relocation<decltype(thunk)> func;
	};

	// Get the target ref for the quest. So we can get the distance to the player.
	// Skyrim normally returns the next (door) reference if there are any. But we want the the marker like it's shown in the map menu.
	// This function is always called right before the one below
	struct GetRefHandleCompassHook
	{
		static RE::RefHandle& thunk(RE::TESQuestTarget* questTarget, RE::RefHandle& refHandle, RE::TESQuest* quest)
		{
			RE::RefHandle& result = func(questTarget, refHandle, quest);

			/*
				Next is some implementation of code I got from the journal menu questTargetID code, 1408EB320 for 1.5.97.
				I don't fully get how this works under consideration of TESQuestTarget, and I don't care actually.

				RE::TESObjectREFR* list = **(RE::TESObjectREFR***)(unk + 0x30); // that's the next door ref in the way to the quest target
				bool doorAvailable = *(bool*)(unk + 0x40); // Seems to check if there is a door left or if the player is in same worldspace or cell

			*/

			Manager::GetSingleton()->handleQuestTarget(questTarget, quest);


			return result;
		}
		static inline REL::Relocation<decltype(thunk)> func;
	};

	// hide quest target marker. Only show when player is in area
	struct CompassHook02
	{
		static bool thunk(void* unk, void* someScaleformInformation, RE::NiPoint3* pos, const RE::RefHandle& currentMarkerTargetHandle, std::uint32_t markerGotoFrame)
		{
			const auto manager = Manager::GetSingleton();
			if (manager->isCompassQuestTargetHidden() && !manager->isPlayerNear())
				return false;

			return func(unk, someScaleformInformation, pos, currentMarkerTargetHandle, markerGotoFrame);
		}
		static inline REL::Relocation<decltype(thunk)> func;
	};

	/*
	struct TargetIDTest
	{
		static RE::RefHandle& thunk(RE::RefHandle& handle01, RE::RefHandle& handle02, RE::PlayerCharacter::TeleportPath* target, bool a4, bool a5)
		{
			RE::RefHandle& test = func(handle01, handle02, target, a4, a5);

			RE::TESObjectREFRPtr questTargetID = nullptr;

			SKSE::log::info("Handle01: {0:08X}", handle01);
			SKSE::log::info("Handle02: {0:08X}", handle02);
			SKSE::log::info("result: {0:08X}", test);

			if (RE::LookupReferenceByHandle(test, questTargetID))
			{
				SKSE::log::info("ID: {0:08X}", questTargetID->formID);
			}

			if (target)
			{
				for (const auto& unk00 : target->unk00)
				{
					if (unk00.interiorCell)
					{
						SKSE::log::info("ID: {0:08X}", unk00.interiorCell->formID);

					}
					else if (unk00.worldspace)
					{
						SKSE::log::info("ID: {0:08X}", unk00.worldspace->formID);

					}
				}
			}

			return test;
		}
		static inline REL::Relocation<decltype(thunk)> func;
	};
	*/

	void InstallHooks()
	{
		PlayerMarkerHook::Install();

		// CurrentLocationReturnHook
		REL::Relocation<std::uintptr_t> loc1{ REL::VariantID(530215, 53098, 0x9167F0), 0x7 };
		stl::write_thunk_jump<CurrentLocationReturnHook>(loc1.address());

		REL::Relocation<std::uintptr_t> loc2{ REL::VariantID(52215, 53102, 0x916D10), REL::Relocate(0x55D, 0x58F, 0xA82) };
		stl::write_thunk_call<CurrentLocationReturnHook>(loc2.address());

		REL::Relocation<std::uintptr_t> loc3{ REL::VariantID(52215, 53102, 0x916D10), REL::Relocate(0x63B, 0x66D, 0xC61) };
		stl::write_thunk_call<CurrentLocationReturnHook>(loc3.address());

		MapMenuProcessMessageHook::Install();

		MapCameraCenterHook::Install();

		REL::Relocation<std::uintptr_t> compass01{ REL::VariantID(50870, 51744, 0x8B4170), REL::Relocate(0x450, 0x473, 0x468) };
		stl::write_thunk_call<CompassHook01>(compass01.address());

		REL::Relocation<std::uintptr_t> compass02{ REL::VariantID(50826, 51691, 0x8B2BD0), REL::Relocate(0x114, 0x180, 0x13A) };
		stl::write_thunk_call<CompassHook02>(compass02.address());

		REL::Relocation<std::uintptr_t> refhook{ REL::VariantID(50826, 51691, 0x8B2BD0), REL::Relocate(0xFB, 0x167, 0x117) };
		stl::write_thunk_call<GetRefHandleCompassHook>(refhook.address());

		//REL::Relocation<std::uintptr_t> ts{ REL::VariantID(52284, 0, 0x0), REL::Relocate(0x121, 0x0, 0x0) };
		//stl::write_thunk_call<TargetIDTest>(ts.address());

		SKSE::log::info("Installed Hooks!");
	}
}