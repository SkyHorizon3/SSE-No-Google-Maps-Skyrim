#include "Hooks.h"
#include "Manager.h"
#include "Utils.h"

namespace Hooks
{
	// disable player map marker
	struct PlayerMarkerHook
	{
		static bool thunk(RE::BSTArray<RE::MapMenuMarker>& playerMarker, RE::NiPoint3* playerMarkerPos)
		{
			if (Manager::GetSingleton()->isPlayerMarkerHidden())
				return false;

			return func(playerMarker, playerMarkerPos);
		};
		static inline REL::Relocation<decltype(thunk)> func;

		static void Install()
		{
			REL::Relocation<std::uintptr_t> markerHook{ REL::VariantID(52221, 53108, 0x9184C0), REL::Relocate(0x121,0x121,0x124) };
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

	// Allow the map camera center to be the middle of the map
	struct SetMapCameraRootHook
	{
		static void thunk(RE::MapCamera* camera, RE::NiNode* root, const RE::NiPoint3& mapPos)
		{
			const auto ui = RE::UI::GetSingleton();
			const auto menu = ui ? ui->GetMenu<RE::MapMenu>() : nullptr;
			const auto runtimeData = menu ? menu->GetRuntimeData2() : nullptr;

			const auto ws = camera ? camera->worldSpace : nullptr;
			if (!menu || !runtimeData || !ws || runtimeData->cameraRootRef != Utils::getPlayerCharacterHandle().native_handle())
			{
				func(camera, root, mapPos);
				return;
			}

			const auto pos = Manager::GetSingleton()->getMarkerPosition(ws, runtimeData);
			func(camera, root, pos);
		}
		static inline REL::Relocation<decltype(thunk)> func;

		static void Install()
		{
			REL::Relocation<std::uintptr_t> Vtbl{ RE::VTABLE_MapCamera[0] }; // VR wrong
			func = Vtbl.write_vfunc(0x3, &thunk);
		}
	};

	// hide compass markers
	struct CompassHook01
	{
		// function parameters different in VR
		static bool thunk(void* unk, void* someScaleformInformation, RE::NiPoint3* pos, const RE::RefHandle& handle, std::uint32_t markerGotoFrame)
		{
			const auto manager = Manager::GetSingleton();
			if (manager->areCompassMarkersHidden() && !manager->handleCompassMarker(handle))
				return false;

			return func(unk, someScaleformInformation, pos, handle, markerGotoFrame);
		}
		static inline REL::Relocation<decltype(thunk)> func;
	};

	// Get the target ref for the quest. So we can get the distance to the player.
	// Skyrim normally returns the next (door) reference if there are any. But we want the the marker like it's shown in the map menu.
	// This function is always called right before the one below
	struct GetTrackingRefHook
	{
		static RE::ObjectRefHandle& thunk(RE::TESQuestTarget* questTarget, RE::ObjectRefHandle& out, const RE::TESQuest* quest)
		{
			auto& result = func(questTarget, out, quest);

			//Next is some implementation of code I got from the journal menu questTargetID code, 1408EB320 for 1.5.97.
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
			if (manager->isCompassQuestTargetHidden() && !manager->isPlayerNearQuestTarget())
				return false;

			return func(unk, someScaleformInformation, pos, currentMarkerTargetHandle, markerGotoFrame);
		}
		static inline REL::Relocation<decltype(thunk)> func;

		static bool CNOthunk()
		{
			const auto manager = Manager::GetSingleton();
			if (manager->isCompassQuestTargetHidden() && !manager->isPlayerNearQuestTarget())
				return false;

			return true;
		}

		// CompassNavigationOverhaul write_branches the original call. That our logic can work we chain on top of it
		struct CNOPatch : Xbyak::CodeGenerator
		{
			CNOPatch(std::uintptr_t cno, std::uintptr_t skip, std::uintptr_t cnoFunc)
			{
				Xbyak::Label funcLabel;
				Xbyak::Label cnoLabel;
				Xbyak::Label skipLabel;
				Xbyak::Label doCno;

				push(rcx);
				push(rdx);
				push(r8);
				push(r9);

				sub(rsp, 0x20);
				call(ptr[rip + funcLabel]); // call thunk
				add(rsp, 0x20);

				test(al, al);

				pop(r9);
				pop(r8);
				pop(rdx);
				pop(rcx);

				jnz(doCno);
				jmp(ptr[rip + skipLabel]);

				// execute cno code
				L(doCno);
				jmp(ptr[rip + cnoLabel]);

				L(funcLabel);
				dq(cnoFunc);

				L(cnoLabel);
				dq(cno);

				L(skipLabel);
				dq(skip);
			}
		};

		static void Install(const bool cnoFound)
		{
			REL::Relocation<std::uintptr_t> compass02{ REL::VariantID(50826, 51691, 0x8B2BD0), REL::Relocate(0x114, 0x180, 0x13A) };
			auto targetAddress = compass02.address();

			if (cnoFound)
			{
				std::int32_t rel = *reinterpret_cast<std::int32_t*>(targetAddress + 1);
				std::uintptr_t instr_end = targetAddress + 5;
				std::uintptr_t absolute = instr_end + rel;

				auto code = CNOPatch(absolute, targetAddress + 0x5, stl::unrestricted_cast<std::uintptr_t>(CNOthunk));

				auto& trampoline = SKSE::GetTrampoline();
				trampoline.write_branch<5>(targetAddress, trampoline.allocate(code));

				SKSE::log::info("Installed compatibility for Compass Navigation Overhaul!");
			}
			else
			{
				stl::write_thunk_call<CompassHook02>(targetAddress);
			}
		}
	};

	void InstallHooks()
	{
		bool cnoFound = REX::W32::GetModuleHandleA("CompassNavigationOverhaul.dll");
		const bool ae2 = REL::Module::get().version() >= SKSE::RUNTIME_SSE_1_7_99;

		const size_t amount = cnoFound ? 140 : 70;
		SKSE::AllocTrampoline(amount);

		PlayerMarkerHook::Install();

		// CurrentLocationReturnHook
		REL::Relocation<std::uintptr_t> loc1{ REL::VariantID(530215, 53098, 0x9167F0), 0x7 };
		stl::write_thunk_jump<CurrentLocationReturnHook>(loc1.address());

		REL::Relocation<std::uintptr_t> loc2{ REL::VariantID(52215, 53102, 0x916D10), REL::Relocate(0x55D, 0x58F, 0xA82) };
		stl::write_thunk_call<CurrentLocationReturnHook>(loc2.address());

		REL::Relocation<std::uintptr_t> loc3{ REL::VariantID(52215, 53102, 0x916D10), REL::Relocate(0x63B, ae2 ? 0x67E : 0x66D, 0xC61) };
		stl::write_thunk_call<CurrentLocationReturnHook>(loc3.address());

		SetMapCameraRootHook::Install();

		REL::Relocation<std::uintptr_t> compass01{ REL::VariantID(50870, 51744, 0x8B4170), REL::Relocate(0x450, 0x473, 0x468) };
		stl::write_thunk_call<CompassHook01>(compass01.address());

		CompassHook02::Install(cnoFound);

		REL::Relocation<std::uintptr_t> refhook{ REL::VariantID(50826, 51691, 0x8B2BD0), REL::Relocate(0xFB, 0x167, 0x117) };
		stl::write_thunk_call<GetTrackingRefHook>(refhook.address());

		SKSE::log::info("Installed Hooks!");
	}
}