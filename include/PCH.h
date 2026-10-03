#pragma once

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX

#include "RE/Skyrim.h"
#include "SKSE/SKSE.h"
#include "REX/REX/Singleton.h"

#include "Plugin.h"

#include <ClibUtil/string.hpp>
#include <xbyak/xbyak.h>
#include <SimpleIni.h>

namespace string = clib_util::string;

using namespace std::literals;
using namespace clib_util::string::literals;

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
}

#define IMGUI_DISABLE_INCLUDE_IMCONFIG_H

#include <ImGui/imgui.h>
#include <ReShade/reshade.hpp>