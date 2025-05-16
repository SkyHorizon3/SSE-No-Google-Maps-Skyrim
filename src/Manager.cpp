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
	m_requiredQuestTargetDistance = static_cast<float>(ini.GetDoubleValue(section, "RequiredQuestTargetDistance"));

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
	ini.SetDoubleValue(section, "RequiredQuestTargetDistance", m_requiredQuestTargetDistance);

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
	if (ImGui::InputFloat("Quest Target Marker Unlock Distance", &m_requiredQuestTargetDistance))
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

void Manager::setPlayerNear(const RE::RefHandle& mapTarget, const bool sameInteriorCell, const RE::PlayerCharacter::TeleportPath* const teleportPath)
{
	if (sameInteriorCell || mapTarget == 0) // player is in same interior cell or in an interior cell near the target
	{
		m_isPlayerNear = true;
		return;
	}

	const auto player = RE::PlayerCharacter::GetSingleton();
	if (!player || !sameInteriorCell && isParentInteriorCell(player) && teleportPath) // player is not in same interior and the target is maybe still far away
	{
		const auto pathRefs = teleportPath->unk18;

		for (std::uint32_t i = 0; i < pathRefs.size(); i++) // pathRefs.size() mostly <= 5
		{
			const auto& teleportRef = pathRefs[i].unk00;
			if (!teleportRef)
				continue;

			const auto teleportLinkedDoor = teleportRef->extraList.GetTeleportLinkedDoor().get().get(); // get door in the linked cell
			if (!teleportLinkedDoor)
				continue;

			if (isParentInteriorCell(teleportLinkedDoor)) // skip if it's interior
				continue;

			RE::TESObjectREFR* nextRef = nullptr;
			const std::uint32_t nextPos = i + 1;
			if (nextPos < pathRefs.size())
			{
				nextRef = pathRefs[nextPos].unk00;
			}
			else // hopefully handle cases where the target is directly in the worldspace and not in an interior again
			{
				RE::TESObjectREFRPtr target = nullptr;
				if (RE::LookupReferenceByHandle(mapTarget, target))
				{
					nextRef = target.get();
				}
			}

			m_isPlayerNear = nextRef && teleportLinkedDoor->GetDistance(nextRef) <= m_requiredQuestTargetDistance;
			return;
		}

		m_isPlayerNear = false;
		return;
	}

	RE::TESObjectREFRPtr target = nullptr;
	if (RE::LookupReferenceByHandle(mapTarget, target))
	{
		m_isPlayerNear = player->GetDistance(target.get()) <= m_requiredQuestTargetDistance;
		return;
	}

	m_isPlayerNear = false;
}

void Manager::handleQuestTarget(RE::TESQuestTarget* questTarget, RE::TESQuest* quest)
{
	RE::RefHandle finalTarget{};
	finalTarget = Utils::getQuestTargetRef(questTarget, finalTarget, true, quest); // gets the final target ref of the quest

	RE::TESObjectREFRPtr target = nullptr;
	RE::LookupReferenceByHandle(finalTarget, target);

	const auto player = RE::PlayerCharacter::GetSingleton();
	bool isSameInterior = target && player && isParentInteriorCell(player) && target->GetParentCell() == player->GetParentCell();

	RE::RefHandle mapTarget{};
	auto teleportPath = reinterpret_cast<RE::PlayerCharacter::TeleportPath*>(&questTarget[1]);
	mapTarget = Utils::getQuestTargetPathRef(mapTarget, finalTarget, teleportPath, isSameInterior, true); // gets the ref that is shown for the quest in the map menu atm

	setPlayerNear(mapTarget, isSameInterior, teleportPath);

	/*
	RE::TESObjectREFRPtr id = nullptr;
	if (RE::LookupReferenceByHandle(mapTarget, id))
	{
		SKSE::log::info("ID: {0:08X}", id->formID);
	}
	*/

}

const RE::TESWorldSpace* Manager::getRootWorldSpace(const RE::TESWorldSpace* ws)
{
	while (ws && ws->parentWorld)
		ws = ws->parentWorld;
	return ws;
}

// 1402ADB80 - 1.5.97
bool Manager::isParentInteriorCell(const RE::TESObjectREFR* const a1)
{
	const auto parentCell = a1->parentCell;
	if (parentCell)
		return parentCell->IsInteriorCell();

	const auto saveParent = a1->GetSaveParentCell();
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

void Manager::handleCameraState(RE::MapCamera* const camera)
{
	const auto cameraState = camera->currentState.get();
	if (!cameraState)
		return;

	const auto world = asWorld(cameraState);
	const auto transition = asTransition(cameraState);

	if (world && !m_mapOpen)
	{
		m_mapOpen = true;
		m_initOffset = world->currentPositionScrollOffset;
	}
	else if (transition && m_mapOpen)
	{
		const auto transitionWorld = reinterpret_cast<RE::MapCameraStates::World*>(camera->unk68[0].get());
		if (!transitionWorld)
			return;

		auto currentPos = transition->currentPosition;
		SKSE::log::info("X: {} - Y: {} - Z: {}", currentPos.x, currentPos.y, currentPos.z);

		auto dest = transition->zoomDestination;
		SKSE::log::info("OFFSET - X: {} - Y: {} - Z: {}", dest.x, dest.y, dest.z);

		auto origin = transition->zoomOrigin;
		SKSE::log::info("ORIGIN - X: {} - Y: {} - Z: {}", origin.x, origin.y, origin.z);

		/*
		if (transitionWorld)
		{
			transitionWorld->currentPosition.x = x;
			transitionWorld->currentPosition.y = y;
		}
		*/
	}


	/*
	else if (auto* transition = asTransition(cameraState))
	{
		float x = 0.f;
		float y = 0.f;
		world = reinterpret_cast<RE::MapCameraStates::World*>(camera->unk68[0].get());


		if (m_marker && world && world->mapData)
		{
			x = m_marker->GetPositionX();
			y = m_marker->GetPositionY();
		}


		if (!m_mapOpen)
		{
			transition->zoomDestination = { x, transition->zoomDestination.y + (y - transition->currentPosition.y), transition->zoomDestination.z };
			transition->zoomOrigin = { x, transition->zoomOrigin.y + (y - transition->currentPosition.y), transition->zoomOrigin.z };
			transition->currentPosition = { x, y, transition->currentPosition.z };

			if (world)
			{
				world->currentPosition.x = x;
				world->currentPosition.y = y;
			}

		}
		else
		{
			transition->zoomDestination = { x, y, transition->zoomDestination.z };
		}

	}
	*/
}

RE::MapCameraStates::World* Manager::asWorld(RE::TESCameraState* state)
{
	if (state && *reinterpret_cast<std::uintptr_t*>(state) == RE::MapCameraStates::World::VTABLE[0].address())
	{
		return reinterpret_cast<RE::MapCameraStates::World*>(state);
	}
	return nullptr;
}

RE::MapCameraStates::Transition* Manager::asTransition(RE::TESCameraState* state)
{
	if (state && *reinterpret_cast<std::uintptr_t*>(state) == RE::MapCameraStates::Transition::VTABLE[0].address())
	{
		return reinterpret_cast<RE::MapCameraStates::Transition*>(state);
	}
	return nullptr;
}

RE::BSEventNotifyControl Manager::ProcessEvent(const RE::MenuOpenCloseEvent* event, RE::BSTEventSource<RE::MenuOpenCloseEvent>* source)
{
	if (!event || !source)
		return RE::BSEventNotifyControl::kContinue;

	if (event->menuName == RE::MapMenu::MENU_NAME && !event->opening)
	{
		m_mapOpen = false;
	}

	return RE::BSEventNotifyControl::kContinue;
}