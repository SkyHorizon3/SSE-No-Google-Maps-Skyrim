#include "Manager.h"

void Manager::parseINI()
{
	m_mapMarkers = enumerateMapMarkers();

	const auto path = std::format("Data/SKSE/Plugins/{}.ini", Plugin::NAME);

	CSimpleIniA ini{};
	ini.SetUnicode();
	ini.LoadFile(path.c_str());

	constexpr const char* section = "General";
	m_isPlayerMarkerHidden = ini.GetBoolValue(section, "bHidePlayerMapMarker");
	m_hideCompassMapMarkers = ini.GetBoolValue(section, "bHideCompassMapMarkers");
	m_hideCompassQuestTargetMarker = ini.GetBoolValue(section, "bHideCompassQuestMarker");
	m_QuestTargetDistance = static_cast<float>(ini.GetDoubleValue(section, "fQuestTargetDistance"));
	m_MarkerTargetDistance = static_cast<float>(ini.GetDoubleValue(section, "fMarkerTargetDistance"));

	const auto marker = ini.GetValue(section, "sMapMarkerReference");
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
	ini.SetBoolValue(section, "bHidePlayerMapMarker", m_isPlayerMarkerHidden);
	ini.SetBoolValue(section, "bHideCompassMapMarkers", m_hideCompassMapMarkers);
	ini.SetBoolValue(section, "bHideCompassQuestMarker", m_hideCompassQuestTargetMarker);
	ini.SetDoubleValue(section, "fQuestTargetDistance", m_QuestTargetDistance);
	ini.SetDoubleValue(section, "fMarkerTargetDistance", m_MarkerTargetDistance);

	std::string markerString = "None";
	if (m_marker)
	{
		const auto file = m_marker->GetFile(0);
		std::string_view filename = file ? file->GetFilename() : ""sv;
		markerString = std::format("{:08X}|{}", Utils::getTrimmedFormID(m_marker), filename);
	}

	ini.SetValue(section, "sMapMarkerReference", markerString.c_str());
	ini.SaveFile(path.c_str());
}

void Manager::draw()
{
	static std::string selected{};
	bool valueChanged = false;

	valueChanged |= ImGui::Checkbox("Hide Player Marker", &m_isPlayerMarkerHidden);
	valueChanged |= ImGui::Checkbox("Hide Compass Map Markers", &m_hideCompassMapMarkers);
	valueChanged |= ImGui::Checkbox("Hide Compass Quest Target Marker", &m_hideCompassQuestTargetMarker);
	valueChanged |= ImGui::InputFloat("Quest Target Marker Unlock Distance", &m_QuestTargetDistance);
	valueChanged |= ImGui::InputFloat("Map Marker Unlock Distance", &m_MarkerTargetDistance);

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

bool Manager::isPlayerNear(const RE::PlayerCharacter* const player, RE::TESObjectREFR* target, const RE::TeleportPath* const teleportPath, const float requiredDistance, const bool sameInteriorCell)
{
	if (sameInteriorCell || !target) // player is in same cell or in an cell near the target
		return true;

	if (!player)
		return false;

	if (isParentInteriorCell(player) && teleportPath) // player is not in same interior and the target is maybe still far away
	{
		const auto& pathRefs = teleportPath->pathRefs;

		for (std::uint32_t i = 0; i < pathRefs.size(); i++)
		{
			const auto& teleportRef = pathRefs[i].ref;
			if (!teleportRef)
				continue;

			const auto teleportLinkedDoor = teleportRef->extraList.GetTeleportLinkedDoor().get().get(); // get door in the linked cell
			if (!teleportLinkedDoor)
				continue;

			if (isParentInteriorCell(teleportLinkedDoor)) // skip if it's another interior
				continue;

			auto nextRef = (i + 1 < pathRefs.size()) ? pathRefs[i + 1].ref : target;
			if (!nextRef || nextRef->GetWorldspace() != target->GetWorldspace())
				continue;

			return teleportLinkedDoor->GetDistance(nextRef) <= requiredDistance;
		}

		return false;
	}

	return player->GetDistance(target) <= requiredDistance;
}

void Manager::handleQuestTarget(RE::TESQuestTarget* questTarget, const RE::TESQuest* quest)
{
	RE::ObjectRefHandle finalTarget{};
	Utils::getTargetRef(questTarget, finalTarget, true, quest); // gets the final target ref of the quest

	const auto target = finalTarget.get();

	const auto player = RE::PlayerCharacter::GetSingleton();
	const bool sameInteriorCell = target && player && isParentInteriorCell(player) && target->parentCell == player->parentCell;

	std::uint32_t scope = sameInteriorCell ? 1 : 0; // 1 = local, 0 = world
	const auto teleportPath = &questTarget->teleportPath;

	RE::ObjectRefHandle mapTarget{};
	Utils::getMapMarkerTrackingRef(mapTarget, finalTarget, teleportPath, scope, true); // gets the ref that is shown for the quest in the map menu atm

	m_isPlayerNearQuestTarget = isPlayerNear(player, mapTarget.get().get(), teleportPath, m_QuestTargetDistance, sameInteriorCell);
}

bool Manager::handleCompassMarker(const RE::RefHandle& handle)
{
	RE::TESObjectREFRPtr refPtr{};
	if (!RE::LookupReferenceByHandle(handle, refPtr))
		return false;

	return isPlayerNear(RE::PlayerCharacter::GetSingleton(), refPtr.get(), nullptr, m_MarkerTargetDistance, false);
}

bool Manager::isShowingQuestTarget(RE::IUIMessageData* data) const
{
	if (!data)
		return false;

	const auto handleData = static_cast<RE::RefHandleUIData*>(data);
	if (!handleData)
		return false;

	return handleData->refHandle != Utils::getPlayerCharacterHandle();
}

void Manager::setCameraCenter(RE::MapMenu* a_menu, RE::UIMessage& a_message)
{
	const auto player = RE::PlayerCharacter::GetSingleton();

	if (!isParentInteriorCell(player) && !isShowingQuestTarget(a_message.data))
	{
		const auto handle = getMarkerRefHandle(player);

		//if (REL::Module::IsVR())
		//	a_menu->GetVRRuntimeData2()->cameraOpeningCenter = handle;
		//else
		a_menu->GetRuntimeData2()->cameraOpeningCenter = handle;
	}
}

const RE::TESWorldSpace* Manager::getRootWorldSpace(const RE::TESWorldSpace* ws)
{
	while (ws && ws->parentWorld)
		ws = ws->parentWorld;
	return ws;
}

// 1402ADB80 - 1.5.97
bool Manager::isParentInteriorCell(const RE::TESObjectREFR* const ref) const
{
	const auto parentCell = ref->parentCell;
	if (parentCell)
		return parentCell->IsInteriorCell();

	const auto saveParent = ref->GetSaveParentCell();
	return !saveParent || saveParent->IsInteriorCell() || saveParent->GetRuntimeData().worldSpace == nullptr;
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
		if (!ref || !ref->IsPersistent())
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
