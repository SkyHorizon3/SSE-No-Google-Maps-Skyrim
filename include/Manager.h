#pragma once

class Manager : public REX::Singleton<Manager>
{
public:
	void parseINI();
	bool isPlayerMarkerHidden() const noexcept { return m_isPlayerMarkerHidden; }
	bool areCompassMarkersHidden() const noexcept { return m_hideCompassMapMarkers; }
	bool isCompassQuestTargetHidden() const noexcept { return m_hideCompassQuestTargetMarker; }
	bool isPlayerNearQuestTarget() const noexcept { return m_isPlayerNearQuestTarget; }

	RE::RefHandle getMarkerRefHandle(const RE::PlayerCharacter* player);
	RE::TESObjectREFR* getMarkerReference() const { return m_marker; }
	bool isParentInteriorCell(const RE::TESObjectREFR* const ref) const;
	void handleQuestTarget(RE::TESQuestTarget* questTarget, const RE::TESQuest* quest);
	bool handleCompassMarker(const RE::RefHandle& handle);
	void setCameraCenter(RE::MapMenu* a_menu, RE::UIMessage& a_message);

	void draw();

private:
	bool isShowingQuestTarget(RE::IUIMessageData* data) const;
	std::string constructKey(const RE::TESObjectREFR* ref) const;
	bool isPlayerNear(const RE::PlayerCharacter* const player, RE::TESObjectREFR* target, const RE::TeleportPath* const teleportPath, const float requiredDistance, const bool sameInteriorCell);
	const RE::TESWorldSpace* getRootWorldSpace(const RE::TESWorldSpace* ws);

	std::vector<std::string> enumerateMapMarkers() const;

	bool createCombo(const char* label, std::string& currentItem, std::vector<std::string>& items, ImGuiComboFlags_ flags);

	void serializeINI();

	// INI
	RE::TESObjectREFR* m_marker{ nullptr };
	bool m_isPlayerMarkerHidden{ true };
	bool m_hideCompassMapMarkers{ true };
	bool m_hideCompassQuestTargetMarker{ true };
	float m_QuestTargetDistance{ 25000.f };
	float m_MarkerTargetDistance{ 25000.f };

	std::vector<std::string> m_mapMarkers{};
	bool m_isPlayerNearQuestTarget{ false };
};

namespace RE
{
	class RefHandleUIData : public RE::IUIMessageData
	{
	public:
		ObjectRefHandle refHandle;  // 10
		uint32_t pad14;      // 14
	};

}

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

	inline RE::FormID getTrimmedFormID(const RE::TESForm* form)
	{
		if (!form)
			return 0;

		const auto file = form->GetFile(0);
		if (!file)
			return 0;

		RE::FormID formID = form->GetFormID() & 0xFFFFFF; // remove file index -> 0x00XXXXXX
		if (file->IsLight())
		{
			formID &= 0xFFF; // remove ESL index -> 0x00000XXX
		}

		return formID;
	}

	// static REL::Relocation just for performance reasons...
	inline RE::ObjectRefHandle& getPlayerCharacterHandle()
	{
		static REL::Relocation<RE::ObjectRefHandle*> handle{ REL::VariantID(517013, 403520, 0x2FEB9EC) };
		return *handle;
	}

	/*
	inline RE::RefHandle getPlayerMarkerHandle()
	{
		static REL::Relocation<RE::RefHandle*> handle{ REL::VariantID(520103, 406633, 0x0) };
		return *handle;
	}
	*/

	inline RE::ObjectRefHandle& getMapMarkerTrackingRef(RE::ObjectRefHandle& out, RE::ObjectRefHandle& targetRefHandle, const RE::TeleportPath* target, std::uint32_t scope, bool validatePath)
	{
		using func_t = decltype(&getMapMarkerTrackingRef);
		static REL::Relocation<func_t> func{ RELOCATION_ID(52183, 53075) };
		return func(out, targetRefHandle, target, scope, validatePath);
	}

	inline bool getMaxHeightAt(RE::TESWorldSpace* worldSpace, const RE::NiPoint3& point, float& outHeight)
	{
		using func_t = decltype(&getMaxHeightAt);
		static REL::Relocation<func_t> func{ RELOCATION_ID(20103, 20551) };
		return func(worldSpace, point, outHeight);
	}

	inline RE::ObjectRefHandle& getTargetRef(RE::TESQuestTarget* target, RE::ObjectRefHandle& out, bool allowPickUpActor, const RE::TESQuest* quest)
	{
		using func_t = decltype(&getTargetRef);
		static REL::Relocation<func_t> func{ RELOCATION_ID(24815, 25284) };
		return func(target, out, allowPickUpActor, quest);
	}
}