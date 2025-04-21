#pragma once

class Manager : public REX::Singleton<Manager>
{
public:
	void parseINI();
	bool isPlayerMarkerHidden() const noexcept { return m_isPlayerMarkerHidden; }
	bool areCompassMarkersHidden() const noexcept { return m_hideCompassMapMarkers; }
	bool isCompassQuestTargetHidden() const noexcept { return m_hideCompassQuestTargetMarker; }

	RE::RefHandle getMarkerRefHandle(const RE::PlayerCharacter* player);
	RE::TESObjectREFR* getMarkerReference() const { return m_marker; }
	bool isParentInteriorCell(const RE::TESObjectREFR* ref);

	void draw();

private:
	std::string constructKey(const RE::TESObjectREFR* ref) const;
	std::vector<std::string> enumerateMapMarkers() const;


	const RE::TESWorldSpace* getRootWorldSpace(const RE::TESWorldSpace* ws);

	bool createCombo(const char* label, std::string& currentItem, std::vector<std::string>& items, ImGuiComboFlags_ flags);

	void serializeINI();

	// INI
	RE::TESObjectREFR* m_marker{ nullptr };
	bool m_isPlayerMarkerHidden{ true };
	bool m_hideCompassMapMarkers{ true };
	bool m_hideCompassQuestTargetMarker{ true };

	std::vector<std::string> m_mapMarkers{};

};

namespace Utils
{
	/**
	* @brief Performs a case-insensitive substring search.
	*
	* This function searches for the first occurrence of the substring needle in the string haystack,
	* ignoring the case of both strings. If the substring is found, a pointer to the beginning of the
	* substring in haystack is returned. If the substring is not found, the function returns nullptr.
	*
	* @param haystack The string to be searched.
	* @param needle The substring to search for.
	* @return A pointer to the beginning of the located substring, or nullptr if the substring is not found.
	*
	* @note This function ensures cross-platform compatibility as strcasestr is not available on all platforms. Shamelessly stolen from Raven.
	*/
	inline const char* strcasestr(const char* haystack, const char* needle) noexcept
	{
		if (!*needle)
		{
			return haystack;
		}

		// Get length of needle and convert it to lowercase
		size_t needle_len = strlen(needle);
		char* lower_needle = new char[needle_len + 1];
		for (size_t i = 0; i < needle_len; ++i)
		{
			lower_needle[i] = (char)tolower(needle[i]);
		}
		lower_needle[needle_len] = '\0';

		const char first_lower_needle = lower_needle[0];

		for (; *haystack; ++haystack)
		{
			if (tolower(*haystack) == first_lower_needle)
			{
				const char* h = haystack;
				const char* n = lower_needle;
				while (*h && *n && tolower(*h) == *n)
				{
					++h;
					++n;
				}

				if (!*n)
				{
					delete[] lower_needle;
					return haystack;
				}
			}
		}

		delete[] lower_needle;
		return nullptr;
	}

	inline RE::FormID getTrimmedFormID(const RE::TESForm* form)
	{
		if (!form)
		{
			return 0;
		}

		const auto array = form->sourceFiles.array;
		if (!array || array->empty())
		{
			return 0;
		}

		RE::FormID formID = form->GetFormID() & 0xFFFFFF; // remove file index -> 0x00XXXXXX
		if (array->front()->IsLight())
		{
			formID = formID & 0xFFF; // remove ESL index -> 0x00000XXX
		}

		return formID;
	}

}