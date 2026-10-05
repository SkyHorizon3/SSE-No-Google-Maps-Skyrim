#pragma once

namespace Utils
{
	RE::ObjectRefHandle& getPlayerCharacterHandle();
	//RE::ObjectRefHandle& getPlayerMarkerHandle();
	RE::ObjectRefHandle& getMapMarkerTrackingRef(RE::ObjectRefHandle& out, RE::ObjectRefHandle& targetRefHandle, const RE::TeleportPath* target, std::uint32_t scope, bool validatePath);

	const RE::TESFile* getFormBasePlugin(const RE::TESForm* const form);
	std::string getModName(const RE::TESForm* const form);
	RE::FormID getTrimmedFormID(const RE::TESForm* const form);
	const char* strcasestr(const char* haystack, const char* needle) noexcept;
}