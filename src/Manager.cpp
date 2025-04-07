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

	const auto file = m_marker->GetFile(0);
	std::string_view filename = file ? file->GetFilename() : ""sv;
	const std::string markerString = std::format("{:08X}|{}", Utils::getTrimmedFormID(m_marker), filename);

	ini.SetValue(section, "MapMarkerReference", markerString.c_str());
	ini.SaveFile(path.c_str());
}

void Manager::handleCameraState(RE::MapCamera* const camera)
{
	const auto cameraState = camera->currentState.get();
	if (m_isShowingQuest || !cameraState)
		return;

	if (auto* world = asWorld(cameraState))
	{
		if (!m_mapOpen)
		{
			float x = 0.f;
			float y = 0.f;

			if (m_marker && world->mapData)
			{
				x = m_marker->GetPositionX();
				y = m_marker->GetPositionY();
			}

			world->currentPosition = { x, y, world->currentPosition.z };
			m_mapOpen = true;
		}
	}
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

	/*
	else if (asExit(cameraState))
	{
		m_mapOpen = false;
		m_isShowingQuest = false;
	}
	*/
}

RE::BSEventNotifyControl Manager::ProcessEvent(const RE::MenuOpenCloseEvent* event, RE::BSTEventSource<RE::MenuOpenCloseEvent>* source)
{
	if (!event || !source)
		return RE::BSEventNotifyControl::kContinue;

	if (event->menuName == RE::MapMenu::MENU_NAME && !event->opening)
	{
		m_mapOpen = false;
		m_isShowingQuest = false;
	}

	return RE::BSEventNotifyControl::kContinue;
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

/*
RE::MapCameraStates::Exit* Manager::asExit(RE::TESCameraState* state)
{
	if (state && *reinterpret_cast<std::uintptr_t*>(state) == RE::MapCameraStates::Exit::VTABLE[0].address())
	{
		return reinterpret_cast<RE::MapCameraStates::Exit*>(state);
	}
	return nullptr;
}
*/

void Manager::draw()
{
	bool valueChanged = false;

	if (ImGui::Checkbox("Hide Player Marker", &m_isPlayerMarkerHidden))
	{
		valueChanged = true;
	}

	static std::string selected = constructKey(m_marker);
	if (createCombo("Select Marker", selected, m_mapMarkers, ImGuiComboFlags_None))
	{
		valueChanged = true;

		const size_t firstSep = selected.find('|');
		const auto first = selected.substr(0, firstSep);

		const size_t secondSep = selected.find('|', firstSep + 1);
		const auto second = selected.substr(firstSep + 1, secondSep - firstSep - 1);

		m_marker = RE::TESDataHandler::GetSingleton()->LookupForm<RE::TESObjectREFR>(std::stoul(first, nullptr, 16), second);
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

std::vector<std::string> Manager::enumerateMapMarkers() const
{
	const auto& [map, lock] = RE::TESForm::GetAllForms();
	[[maybe_unused]] const RE::BSReadWriteLock l{ lock };

	std::vector<std::string> markerVec{};

	if (!map)
		return markerVec;

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


