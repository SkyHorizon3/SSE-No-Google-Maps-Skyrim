#include "Manager.h"

void Manager::parseINI()
{
	m_mapMarkers = enumerateMapMarkers();

	const auto path = std::format("Data/SKSE/Plugins/{}.ini", Plugin::NAME);

	CSimpleIniA ini{};
	ini.SetUnicode();
	ini.LoadFile(path.c_str());

	constexpr const char* section = "General";

	m_isPlayerMarkerHidden = ini.GetBoolValue(section, "HidePlayerMapMarker");
	m_hideCompassMapMarkers = ini.GetBoolValue(section, "HideCompassMapMarkers");
	m_hideCompassQuestTargetMarker = ini.GetBoolValue(section, "HideCompassQuestTargetMarker");

	const auto marker = ini.GetValue(section, "MapMarkerReference");
	if (!marker || *marker == '\0')
		return;

	const std::string markerString = marker;
	const size_t separatorPos = markerString.find('|');
	if (separatorPos == std::string::npos)
	{
		// if | is not found
		return;
	}

	const RE::FormID formID = std::stoul(markerString.substr(0, separatorPos), nullptr, 16);
	const std::string plugin = markerString.substr(separatorPos + 1);

	m_marker = RE::TESDataHandler::GetSingleton()->LookupForm<RE::TESObjectREFR>(formID, plugin);
}

void Manager::serializeINI()
{
	const auto path = std::format("Data/SKSE/Plugins/{}.ini", Plugin::NAME);

	CSimpleIniA ini;
	ini.SetUnicode();

	constexpr const char* section = "General";

	ini.SetBoolValue(section, "HidePlayerMapMarker", m_isPlayerMarkerHidden);
	ini.SetBoolValue(section, "HideCompassMapMarkers", m_hideCompassMapMarkers);
	ini.SetBoolValue(section, "HideCompassQuestTargetMarker", m_hideCompassQuestTargetMarker);

	std::string markerString = "None";
	if (m_marker)
	{
		const auto file = m_marker->GetFile(0);
		std::string_view filename = file ? file->GetFilename() : ""sv;
		markerString = std::format("{:08X}|{}", Utils::getTrimmedFormID(m_marker), filename);
	}

	ini.SetValue(section, "MapMarkerReference", markerString.c_str());
	ini.SaveFile(path.c_str());
}

void Manager::draw()
{
	static std::string selected{};
	bool valueChanged = false;

	if (ImGui::Checkbox("Hide Player Marker", &m_isPlayerMarkerHidden))
	{
		valueChanged = true;
	}
	if (ImGui::Checkbox("Hide Compass Map Markers", &m_hideCompassMapMarkers))
	{
		valueChanged = true;
	}
	if (ImGui::Checkbox("Hide Compass Quest Target Marker", &m_hideCompassQuestTargetMarker))
	{
		valueChanged = true;
	}

	if (!m_marker)
		selected = "None";
	else
		selected = constructKey(m_marker);

	if (createCombo("Select Marker", selected, m_mapMarkers, ImGuiComboFlags_None))
	{
		valueChanged = true;

		const size_t firstSep = selected.find('|');
		if (firstSep == std::string::npos)
		{
			m_marker = nullptr;
		}
		else
		{
			const auto first = selected.substr(0, firstSep);

			const size_t secondSep = selected.find('|', firstSep + 1);
			const auto second = selected.substr(firstSep + 1, secondSep - firstSep - 1);

			m_marker = RE::TESDataHandler::GetSingleton()->LookupForm<RE::TESObjectREFR>(std::stoul(first, nullptr, 16), second);
		}
	}

	if (valueChanged)
	{
		serializeINI();
	}
}

std::string Manager::constructKey(const RE::TESObjectREFR* ref) const
{
	if (!ref)
		return {};

	const auto marker = ref ? ref->extraList.GetByType<RE::ExtraMapMarker>() : nullptr;
	if (marker && marker->mapData)
	{
		const auto markerName = marker->mapData->locationName.GetFullName();
		const auto file = ref->GetFile(0);
		std::string_view filename = file ? file->GetFilename() : ""sv;

		return std::format("{:08X}|{}|{}", Utils::getTrimmedFormID(ref), filename, markerName);
	}

	return {};
}

const RE::TESWorldSpace* Manager::getRootWorldSpace(const RE::TESWorldSpace* ws)
{
	while (ws && ws->parentWorld)
		ws = ws->parentWorld;
	return ws;
}

bool Manager::isParentInteriorCell(const RE::TESObjectREFR* ref)
{
	if (!ref)
		return false;

	auto* parentCell = ref->GetParentCell();
	if (!parentCell)
	{
		parentCell = ref->GetSaveParentCell();
		if (!parentCell)
		{
			return false;
		}
	}

	return parentCell->IsInteriorCell();
}

RE::RefHandle Manager::getMarkerRefHandle(const RE::PlayerCharacter* player)
{
	if (!m_marker || !player)
		return 0;

	const auto playerWS = player->GetWorldspace();
	if (!playerWS)
		return 0;

	const auto markerWs = m_marker->GetWorldspace();
	if (!markerWs)
		return 0;

	if (getRootWorldSpace(markerWs) != getRootWorldSpace(playerWS))
		return 0;

	RE::RefHandle handle{};
	RE::CreateRefHandle(handle, m_marker);

	return handle;
}

std::vector<std::string> Manager::enumerateMapMarkers() const
{
	const auto& [map, lock] = RE::TESForm::GetAllForms();
	[[maybe_unused]] const RE::BSReadLockGuard l{ lock };

	std::vector<std::string> markerVec{};

	if (!map)
		return markerVec;

	markerVec.emplace_back("None");

	for (const auto& [formID, form] : *map)
	{
		if (!form || form->IsNot(RE::FormType::Reference))
			continue;

		const auto ref = form->As<RE::TESObjectREFR>();
		if (!ref || ref->IsDisabled() || !ref->IsPersistent())
			continue;

		const std::string val = constructKey(ref);
		if (val.empty())
			continue;

		markerVec.emplace_back(val);
	}

	return markerVec;
}

bool Manager::createCombo(const char* label, std::string& currentItem, std::vector<std::string>& items, ImGuiComboFlags_ flags)
{
	ImGuiStyle& style = ImGui::GetStyle();
	float w = 500.0f;
	float spacing = style.ItemInnerSpacing.x;
	float button_sz = ImGui::GetFrameHeight();
	ImGui::PushItemWidth(w - spacing - button_sz * 2.0f);

	bool itemChanged = false;
	static char searchBuffer[256] = "";

	if (ImGui::BeginCombo(label, currentItem.c_str(), flags))
	{
		ImGui::InputTextWithHint("##Search", "Search...", searchBuffer, sizeof(searchBuffer));

		// Filter items based on search buffer
		for (const auto& item : items)
		{
			if (Utils::strcasestr(item.c_str(), searchBuffer))
			{
				bool isSelected = (currentItem == item);
				if (ImGui::Selectable(item.c_str(), isSelected))
				{
					currentItem = item;
					itemChanged = true;
					searchBuffer[0] = '\0'; // Clear search buffer on selection
				}
				if (isSelected) { ImGui::SetItemDefaultFocus(); }
			}
		}
		ImGui::EndCombo();
	}

	ImGui::PopItemWidth();

	return itemChanged;
}
