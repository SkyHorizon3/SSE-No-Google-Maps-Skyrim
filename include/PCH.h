#pragma once

#include "RE/Skyrim.h"
#include "SKSE/SKSE.h"
#include "REX/REX/Singleton.h"

using namespace std::literals;

#include "Plugin.h"

#include <xbyak/xbyak.h>
#include <SimpleIni.h>

namespace stl
{
	using namespace SKSE::stl;

	template <class T, std::uint8_t size = 5>
	void write_thunk_call(std::uintptr_t a_src)
	{
		T::func = SKSE::GetTrampoline().write_call<size>(a_src, T::thunk);
	}

	template <class T, std::uint8_t size = 5>
	void write_thunk_jump(std::uintptr_t a_src)
	{
		T::func = SKSE::GetTrampoline().write_branch<size>(a_src, T::thunk);
	}

	template <class T>
	void write_thunk_lea(std::uintptr_t a_src) //only use on x64, e.g. rexw (0x48), rexrw (0x4C)
	{
		auto bytes = a_src;
		const auto opCode = *reinterpret_cast<std::uint8_t*>(++bytes);

		if (opCode == 0x8D) // check if it's lea
		{
			const auto operand1 = *reinterpret_cast<std::uint8_t*>(++bytes); // mostly 0x05 in case of lea
			const auto writeAddress = bytes;

			// get original displacement
			std::int32_t disp = 0;
			for (std::uint8_t i = 0; i < 4; ++i)
			{
				disp |= *reinterpret_cast<std::uint8_t*>(++bytes) << (i * 8);
			}

			SKSE::GetTrampoline().write_call<5>(writeAddress, T::thunk); // overwrite last 5 bytes of lea instruction

			REL::safe_write(writeAddress, operand1); // write back the operand which got modified by write_call

			T::func = a_src + 7 + disp; // address + lea size + displacement
		}
	}

	// Thanks Nukem and po3 for this!
	template <class T, std::size_t BYTES>
	void hook_function_prologue(std::uintptr_t a_src)
	{
		struct Patch : Xbyak::CodeGenerator
		{
			Patch(std::uintptr_t a_originalFuncAddr, std::size_t a_originalByteLength)
			{
				// Hook returns here. Execute the restored bytes and jump back to the original function.
				for (size_t i = 0; i < a_originalByteLength; i++)
					db(*reinterpret_cast<uint8_t*>(a_originalFuncAddr + i));

				jmp(qword[rip]);
				dq(a_originalFuncAddr + a_originalByteLength);
			}
		};

		Patch p(a_src, BYTES);
		p.ready();

		auto& trampoline = SKSE::GetTrampoline();
		trampoline.write_branch<5>(a_src, T::thunk);

		auto alloc = trampoline.allocate(p.getSize());
		std::memcpy(alloc, p.getCode(), p.getSize());

		T::func = reinterpret_cast<std::uintptr_t>(alloc);
	}
}

#define IMGUI_DISABLE_INCLUDE_IMCONFIG_H

#include <ImGui/imgui.h>
#include <ReShade/reshade.hpp>