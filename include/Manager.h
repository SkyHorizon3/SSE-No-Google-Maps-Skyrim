#pragma once

class Manager : public REX::Singleton<Manager>
{
public:
	void onDataLoaded();
	bool isPlayerMarkerHidden() const noexcept { return m_isPlayerMarkerHidden; }
	bool areCompassMarkersHidden() const noexcept { return m_hideCompassMapMarkers; }
	bool isCompassQuestTargetHidden() const noexcept { return m_hideCompassQuestTargetMarker; }
	bool isPlayerNearQuestTarget() const noexcept { return m_isPlayerNearQuestTarget; }

	RE::NiPoint3 getMarkerPosition(RE::TESWorldSpace* const worldspace, RE::MapMenu::RUNTIME_DATA2* runtimeData);
	bool isParentInteriorCell(const RE::TESObjectREFR* const ref) const;
	void handleQuestTarget(RE::TESQuestTarget* questTarget, const RE::TESQuest* quest);
	bool handleCompassMarker(const RE::RefHandle& handle);

	void draw();

private:
	std::string constructKey(RE::TESObjectREFR* const ref) const;
	bool isPlayerNear(const RE::PlayerCharacter* const player, RE::TESObjectREFR* target, const RE::TeleportPath* const teleportPath, const float requiredDistance, const bool sameInteriorCell);
	const RE::TESWorldSpace* getRootWorldSpace(const RE::TESWorldSpace* ws);
	RE::NiPoint3 getMiddleOfMap(RE::TESWorldSpace* const ws);

	std::vector<std::string> enumerateMapMarkers() const;

	bool createCombo(const char* label, std::string& currentItem, std::vector<std::string>& items, ImGuiComboFlags_ flags);
	RE::TESObjectREFR* lookupRef(const RE::FormID formID, const std::string_view plugin) const;

	void parseINI();
	void serializeINI();

	// INI
	RE::TESObjectREFR* m_marker{ nullptr };
	bool m_isPlayerMarkerHidden{ true };
	bool m_hideCompassMapMarkers{ true };
	bool m_hideCompassQuestTargetMarker{ true };
	bool m_fwmfFound{ false };
	float m_QuestTargetDistance{ 25000.f };
	float m_MarkerTargetDistance{ 25000.f };
	std::string m_selectedMarkerStr;

	std::vector<std::string> m_mapMarkers{};
	bool m_isPlayerNearQuestTarget{ false };
};