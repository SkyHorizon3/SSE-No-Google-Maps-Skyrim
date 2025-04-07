#pragma once

class Manager :
	public REX::Singleton<Manager>,
	public RE::BSTEventSink<RE::MenuOpenCloseEvent>
{
public:
	void parseINI();
	void handleCameraState(RE::MapCamera* const cameraState);
	void setIsShowingQuest(const bool val) noexcept { m_isShowingQuest = val; }
	bool isPlayerMarkerHidden() const noexcept { return m_isPlayerMarkerHidden; }

	void draw();

	RE::BSEventNotifyControl ProcessEvent(const RE::MenuOpenCloseEvent* a_event, RE::BSTEventSource<RE::MenuOpenCloseEvent>* a_source) override;
private:
	RE::MapCameraStates::World* asWorld(RE::TESCameraState* state);
	RE::MapCameraStates::Transition* asTransition(RE::TESCameraState* state);
	//RE::MapCameraStates::Exit* asExit(RE::TESCameraState* state);

	std::string constructKey(const RE::TESObjectREFR* ref) const;
	std::vector<std::string> enumerateMapMarkers() const;

	bool createCombo(const char* label, std::string& currentItem, std::vector<std::string>& items, ImGuiComboFlags_ flags);

	void serializeINI();

	// INI
	RE::TESObjectREFR* m_marker{ nullptr };
	bool m_isPlayerMarkerHidden{ true };

	bool m_mapOpen{ false };
	bool m_isShowingQuest{ false };
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