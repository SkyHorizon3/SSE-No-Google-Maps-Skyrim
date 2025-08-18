#include "Hooks.h"
#include "Manager.h"

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

		/*
		static bool thunkVR(RE::BSTArray<RE::MapMenuMarker>* playerMarker, RE::NiPoint3* playerMarkerPos, RE::NiPoint3* unk)
		{
			if (Manager::GetSingleton()->isPlayerMarkerHidden())
				return false;

			return funcVR(playerMarker, playerMarkerPos, unk);
		};
		*/

		static inline REL::Relocation<decltype(thunk)> func;
		//static inline REL::Relocation<decltype(thunkVR)> funcVR;

		static void Install()
		{
			REL::Relocation<std::uintptr_t> markerHook{ REL::VariantID(52221, 53108, 0x9184C0), REL::Relocate(0x121,0x121,0x124) };

			//if (REL::Module::IsVR())
			//funcVR = SKSE::GetTrampoline().write_call<5>(markerHook.address(), thunkVR);
			//else

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

			if (!a_menu || a_message.type != RE::UI_MESSAGE_TYPE::kShow)
				return result;

			const auto manager = Manager::GetSingleton();
			const auto player = RE::PlayerCharacter::GetSingleton();

			if (!manager->isParentInteriorCell(player) && !isShowingQuestTarget(a_message.data))
			{
				const auto handle = manager->getMarkerRefHandle(player);

				if (REL::Module::IsVR())
					a_menu->GetVRRuntimeData2()->cameraOpeningCenter = handle;
				else
					a_menu->GetRuntimeData2()->cameraOpeningCenter = handle;
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
	struct MapCameraCenterHook
	{
		struct Patch : Xbyak::CodeGenerator
		{
			Patch(std::uintptr_t retn, std::uintptr_t func)
			{
				Xbyak::Label funcLabel;
				Xbyak::Label retnLabel;

				push(rcx);

				mov(rcx, rdi); // map menu ptr
				call(ptr[rip + funcLabel]); //call thunk

				movss(xmm0, ptr[rax]);
				movss(ptr[rbp - 0x49], xmm0); // x

				movss(xmm1, ptr[rax + 0x4]); // y
				movss(ptr[rbp - 0x45], xmm1);

				movss(xmm0, ptr[rax + 0x8]); // z
				movss(ptr[rbp - 0x41], xmm0);

				pop(rcx);

				jmp(ptr[rip + retnLabel]); //jump back to original code

				L(funcLabel);
				dq(func);

				L(retnLabel);
				dq(retn);
			}
		};

		static RE::NiPoint3* thunk(RE::MapMenu* menu)
		{
			static RE::NiPoint3 result{};

			if (menu && menu->GetRuntimeData2()->cameraOpeningCenter == 0)
			{
				const auto camera = &menu->GetRuntimeData2()->camera; // VR is different

				result.x = (camera->worldSpace->minimumCoords.x + camera->worldSpace->maximumCoords.x) / 2;
				result.y = (camera->worldSpace->minimumCoords.y + camera->worldSpace->maximumCoords.y) / 2;
				result.z = 0.f;
			}
			else
			{
				result = REL::Module::IsVR() ? menu->GetVRRuntimeData2()->playerMarkerPosition : menu->GetRuntimeData2()->playerMarkerPosition;
			}

			return &result;
		}


		static void Install()
		{
			// Inlined in VR, need to rewrite parts for support
			REL::Relocation<std::uintptr_t> fillAddress{ REL::VariantID(52214, 53101, 0x91B360), REL::Relocate(0x1A9, 0x1AB) };

			const auto targetAddress = fillAddress.address();

			REL::safe_fill(targetAddress, REL::NOP, 0x27);
			auto code = Patch(targetAddress + 0x27, stl::unrestricted_cast<std::uintptr_t>(thunk));

			auto& trampoline = SKSE::GetTrampoline();
			auto result = trampoline.allocate(code);
			trampoline.write_branch<5>(targetAddress, (std::uintptr_t)result);

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
			if (manager->isCompassQuestTargetHidden() && !manager->isPlayerNear())
				return false;

			return func(unk, someScaleformInformation, pos, currentMarkerTargetHandle, markerGotoFrame);
		}
		static inline REL::Relocation<decltype(thunk)> func;

		static bool CNOthunk()
		{
			const auto manager = Manager::GetSingleton();
			if (manager->isCompassQuestTargetHidden() && !manager->isPlayerNear())
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
				Xbyak::Label doSkip;

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
				jmp(doSkip);

				// execute cno code
				L(doCno);
				jmp(ptr[rip + cnoLabel]);

				// skip over cno code, use our return value (false)
				L(doSkip);
				jmp(ptr[rip + skipLabel]);

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
				auto result = trampoline.allocate(code);
				trampoline.write_branch<5>(targetAddress, (std::uintptr_t)result);

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
		bool cnoFound = GetModuleHandle(L"CompassNavigationOverhaul.dll");
		size_t amount = cnoFound ? 215 : 150;
		SKSE::AllocTrampoline(amount);


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

		CompassHook02::Install(cnoFound);

		REL::Relocation<std::uintptr_t> refhook{ REL::VariantID(50826, 51691, 0x8B2BD0), REL::Relocate(0xFB, 0x167, 0x117) };
		stl::write_thunk_call<GetRefHandleCompassHook>(refhook.address());

		SKSE::log::info("Installed Hooks!");
	}
}