#include "Utils.h"

namespace Utils
{
	RE::ObjectRefHandle& getPlayerCharacterHandle()
	{
		static REL::Relocation<RE::ObjectRefHandle*> handle{ REL::VariantID(517013, 403520, 0x2FEB9EC) };
		return *handle;
	}

	/*RE::ObjectRefHandle& getPlayerMarkerHandle()
	{
		static REL::Relocation<RE::ObjectRefHandle*> handle{ REL::VariantID(520103, 406633, 0x0) };
		return *handle;
	}*/

	RE::ObjectRefHandle& getMapMarkerTrackingRef(RE::ObjectRefHandle& out, RE::ObjectRefHandle& targetRefHandle, const RE::TeleportPath* target, std::uint32_t scope, bool validatePath)
	{
		using func_t = decltype(&getMapMarkerTrackingRef);
		static REL::Relocation<func_t> func{ RELOCATION_ID(52183, 53075) };
		return func(out, targetRefHandle, target, scope, validatePath);
	}

	// Get the file that defines the record through the formID index
	// The problem is that the internal file array is wrong in an edge case with REFR forms:
	// It returns wrong file for REFR records defined in esm files when another esm file adds the persistent flag in an override
	// It's not a bug, just bullshit caused by the plugin system since it obviously can't know if 
	// a record is persistent if the flag is added later in another plugin
	const RE::TESFile* getFormBasePlugin(const RE::TESForm* const form)
	{
		const auto handler = RE::TESDataHandler::GetSingleton();

		if (!form || !handler)
			return nullptr;

		const auto formID = form->formID;
		const auto expected = (formID & 0xFF000000) == 0xFE000000 ?
			handler->LookupLoadedLightModByIndex(static_cast<std::uint16_t>((0x00FFF000 & formID) >> 12)) :
			handler->LookupLoadedModByIndex(static_cast<std::uint8_t>((0xFF000000 & formID) >> 24));

		return expected;
	}

	std::string getModName(const RE::TESForm* const form)
	{
		if (!form)
			return {};

		const auto file = getFormBasePlugin(form);
		if (!file)
			return {};

		return file ? std::string(file->GetFilename()) : "";
	}

	RE::FormID getTrimmedFormID(const RE::TESForm* const form)
	{
		if (!form)
			return 0;

		const auto file = getFormBasePlugin(form);
		if (!file)
			return 0;

		RE::FormID formID = form->GetFormID() & 0xFFFFFF; // remove file index -> 0x00XXXXXX
		if (file->IsLight())
		{
			formID &= 0xFFF; // remove ESL index -> 0x00000XXX
		}

		return formID;
	}

	const char* strcasestr(const char* haystack, const char* needle) noexcept
	{
		if (!*needle)
		{
			return haystack;
		}

		const char first = static_cast<char>(std::tolower(static_cast<unsigned char>(*needle)));

		for (; *haystack; ++haystack)
		{
			if (std::tolower(static_cast<unsigned char>(*haystack)) == first)
			{
				const char* h = haystack + 1;
				const char* n = needle + 1;
				while (*n && *h &&
					std::tolower(static_cast<unsigned char>(*h)) ==
					std::tolower(static_cast<unsigned char>(*n)))
				{
					++h;
					++n;
				}

				if (!*n)
				{
					return haystack;
				}
			}
		}

		return nullptr;
	}
}