#include "UI.h"

#include <cctype>
#include <cfloat>
#include <cstring>

#include "MorphLists.h"
#include "Settings.h"
#include "SKSEMenuFramework.h"
#include "Updater.h"

namespace UI
{
	namespace
	{
		using ImGuiMCP::BeginCombo;
		using ImGuiMCP::BeginDisabled;
		using ImGuiMCP::BeginTable;
		using ImGuiMCP::Button;
		using ImGuiMCP::Checkbox;
		using ImGuiMCP::CloseCurrentPopup;
		using ImGuiMCP::Combo;
		using ImGuiMCP::EndCombo;
		using ImGuiMCP::EndDisabled;
		using ImGuiMCP::EndTable;
		using ImGuiMCP::InputFloat;
		using ImGuiMCP::InputText;
		using ImGuiMCP::InputTextWithHint;
		using ImGuiMCP::IsItemActive;
		using ImGuiMCP::IsItemDeactivatedAfterEdit;
		using ImGuiMCP::IsItemHovered;
		using ImGuiMCP::IsWindowAppearing;
		using ImGuiMCP::PopID;
		using ImGuiMCP::PushID;
		using ImGuiMCP::SameLine;
		using ImGuiMCP::Selectable;
		using ImGuiMCP::Separator;
		using ImGuiMCP::SeparatorText;
		using ImGuiMCP::SetKeyboardFocusHere;
		using ImGuiMCP::SetNextItemWidth;
		using ImGuiMCP::SetTooltip;
		using ImGuiMCP::SliderFloat;
		using ImGuiMCP::SliderInt;
		using ImGuiMCP::SmallButton;
		using ImGuiMCP::Spacing;
		using ImGuiMCP::TableHeadersRow;
		using ImGuiMCP::TableNextColumn;
		using ImGuiMCP::TableNextRow;
		using ImGuiMCP::TableSetupColumn;
		using ImGuiMCP::Text;
		using ImGuiMCP::TextDisabled;
		using ImGuiMCP::TextWrapped;

		constexpr std::size_t BUF_LEN = 256;

		void HelpMarker(const char* text)
		{
			SameLine();
			TextDisabled("(?)");
			if (IsItemHovered()) {
				SetTooltip("%s", text);
			}
		}

		// "Label (?)" on its own line; the next widget is sized to fill the
		// row below it.
		void RowLabel(const char* label, const char* help)
		{
			Text("%s", label);
			if (help) {
				HelpMarker(help);
			}
			SetNextItemWidth(-FLT_MIN);
		}

		// Native layout: box on the left, label beside it, help marker after.
		bool RowCheckbox(const char* label, const char* help, bool* v, const char* id)
		{
			const std::string lid = std::string(label) + id;
			const bool        changed = Checkbox(lid.c_str(), v);
			if (help) {
				HelpMarker(help);
			}
			return changed;
		}

		std::string Trim(std::string s)
		{
			const auto isSpace = [](char c) { return c == ' ' || c == '\t' || c == '\r' || c == '\n'; };
			while (!s.empty() && isSpace(s.front())) {
				s.erase(s.begin());
			}
			while (!s.empty() && isSpace(s.back())) {
				s.pop_back();
			}
			return s;
		}

		void SetBuf(std::array<char, BUF_LEN>& buf, const std::string& s)
		{
			const auto n = (std::min)(s.size(), BUF_LEN - 1);
			std::memcpy(buf.data(), s.data(), n);
			buf[n] = '\0';
		}

		std::string Lower(std::string s)
		{
			for (auto& c : s) {
				c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
			}
			return s;
		}

		// Searchable dropdown for morph names: shows each entry's display
		// name but yields the internal morph name via outMorph. `preview` is
		// the text on the closed combo. `usedBy`/`self` mark morphs already
		// used by other rows as disabled so duplicates can't be picked.
		// Returns true when the user picked a different entry. Only one combo
		// popup is open at a time, so the shared static filter buffer is
		// safe — it resets on appear.
		bool MorphCombo(const char* label, const std::string& preview,
		                const std::vector<MorphLists::Entry>& entries,
		                const std::string& currentMorph, std::string& outMorph,
		                const std::vector<Settings::MorphEntry>* usedBy = nullptr,
		                const Settings::MorphEntry* self = nullptr)
		{
			static char filterBuf[BUF_LEN] = {};
			bool        changed = false;

			if (!BeginCombo(label, preview.c_str())) {
				return false;
			}

			// Clear before InputTextWithHint consumes the buffer, and focus
			// the filter box itself (SetKeyboardFocusHere applies to the next
			// widget submitted).
			if (IsWindowAppearing()) {
				filterBuf[0] = '\0';
				SetKeyboardFocusHere();
			}
			InputTextWithHint("##filter", "Search...", filterBuf, BUF_LEN);
			const std::string filter = Lower(filterBuf);

			const bool currentListed = currentMorph.empty() ||
			    std::any_of(entries.begin(), entries.end(),
			                [&](const MorphLists::Entry& e) { return e.morph == currentMorph; });
			if (!currentListed) {
				Selectable((preview + " (not in list)##notInList").c_str(), true,
			           ImGuiMCP::ImGuiSelectableFlags_Disabled);
			}

			std::size_t shown = 0;
			for (std::size_t i = 0; i < entries.size(); ++i) {
				const auto& e = entries[i];
				if (!filter.empty() &&
				    Lower(e.display).find(filter) == std::string::npos &&
				    Lower(e.morph).find(filter) == std::string::npos) {
					continue;
				}
				++shown;
				const bool sel = (e.morph == currentMorph);
				bool       inUse = false;
				if (usedBy) {
					for (const auto& r : *usedBy) {
						if (&r != self && r.name == e.morph) {
							inUse = true;
							break;
						}
					}
				}
				const std::string itemLabel = e.display + "##" + std::to_string(i);
				const auto        flags = inUse ? ImGuiMCP::ImGuiSelectableFlags_Disabled : ImGuiMCP::ImGuiSelectableFlags_None;
				if (Selectable(itemLabel.c_str(), sel, flags) && !sel) {
					outMorph = e.morph;
					changed = true;
					CloseCurrentPopup();
					break;
				}
			}
			if (entries.empty()) {
				TextDisabled("No morphs in this list.");
			} else if (shown == 0) {
				TextDisabled("No matches.");
			}
			EndCombo();
			return changed;
		}
	}

	void Register()
	{
		if (!SKSEMenuFramework::IsInstalled()) {
			logger::warn("SKSEMenuFramework.dll not installed; menu disabled");
			return;
		}
		SKSEMenuFramework::SetSection("Mana Tanks NG");
		SKSEMenuFramework::AddSectionItem("Settings", RenderSettings);
		SKSEMenuFramework::AddSectionItem("Morphs", RenderMorphs);
	}

	void __stdcall RenderSettings()
	{
		Settings::FlushIfDue();
		std::lock_guard guard(Settings::lock);

		if (!Updater::SkeeAvailable()) {
			TextWrapped("Warning: skee64.dll (RaceMenu) was not detected. Body morphs will not apply until it is loaded.");
			Separator();
		}

		const bool wasEnabled = Settings::enabled;
		if (RowCheckbox("Enabled", "Master switch. Turning this off removes all Mana Tanks NG morphs.",
		                &Settings::enabled, "##enabled")) {
			Settings::MarkDirty();
			if (wasEnabled && !Settings::enabled) {
				Updater::ClearAllMorphs();
			}
		}

		constexpr const char* powerItems = "Magicka\0Health\0Stamina\0";
		int                   powerIdx = Settings::power - 1;
		RowLabel("Power source", "Which bar drives the scaling.");
		if (Combo("##power", &powerIdx, powerItems)) {
			Settings::power = powerIdx + 1;
			Settings::MarkDirty();
		}

		Spacing();
		SeparatorText("Timing");
		RowLabel("Update interval", "How often morphs update while the bar is not full. Smaller = smoother.");
		if (SliderFloat("##updateInterval", &Settings::updateInterval, 0.01f, 1.0f, "%.2f s")) {
			Settings::MarkDirty();
		}
		RowLabel("Maintenance interval", "How often morphs refresh while the bar is full.");
		if (SliderFloat("##maintInterval", &Settings::maintInterval, 0.5f, 30.0f, "%.1f s")) {
			Settings::MarkDirty();
		}

		Spacing();
		SeparatorText("Pool scaling");
		if (RowCheckbox("Limit by attribute",
		                "The max morph is applied when the attribute (magicka/stamina/health) reaches the "
		                "Attribute Limit; otherwise morphs always reach max regardless.",
		                &Settings::linkPool, "##linkPool")) {
			Settings::MarkDirty();
		}
		if (Settings::linkPool) {
			RowLabel("Attribute Limit",
			         "The attribute value (magicka/stamina/health) considered maxed out - morphs reach full size here.");
			if (SliderInt("##poolMax", &Settings::poolMax, 100, 2000)) {
				Settings::MarkDirty();
			}
		}

		Spacing();
		SeparatorText("Body scaling");

		if (RowCheckbox("Enabled", "Turn the morphs on or off.", &Settings::morphsEnabled, "##morphsEnabled")) {
			Settings::MarkDirty();
			if (!Settings::morphsEnabled) {
				std::vector<std::string> names;
				for (const auto& m : Settings::morphs) {
					names.push_back(m.name);
				}
				Updater::ClearMorphs(std::move(names));
			}
		}
		if (!Settings::morphsEnabled) {
			BeginDisabled();
		}
		RowLabel("Min", "Morph value when the bar is empty.");
		if (SliderFloat("##min", &Settings::morphMin, -6.0f, 6.0f, "%.1f")) {
			Settings::MarkDirty();
		}
		RowLabel("Max", "Morph value when the bar is full.");
		if (SliderFloat("##max", &Settings::morphMax, -6.0f, 6.0f, "%.1f")) {
			Settings::MarkDirty();
		}
		if (!Settings::morphsEnabled) {
			EndDisabled();
		}

		Spacing();
		Separator();
		const float pct = Updater::LastPct();
		if (pct >= 0.0f) {
			Text("Current: %.1f%%   Updating every %.2fs", pct * 100.0f, Updater::LastInterval());
		} else {
			Text("Inactive");
		}
	}

	void __stdcall RenderMorphs()
	{
		Settings::FlushIfDue();
		MorphLists::Refresh();

		std::vector<std::string> removed;
		{
			std::lock_guard guard(Settings::lock);

			// Per-row name edit buffers plus the "add row" inputs. Buffers are
			// refreshed from the config whenever their field isn't being edited,
			// so external changes (delete/reset/Escape) self-correct.
			static std::vector<std::array<char, BUF_LEN>> nameBufs;
			static char                                 newNameBuf[BUF_LEN] = {};
			static float                                newSize = 1.0f;
			static std::string                          pendingMorph;  // add-row pick in list mode

			const bool  inListMode   = !Settings::morphList.empty();
			const auto* listEntries  = inListMode ? &MorphLists::GetEntries(Settings::morphList) : nullptr;

			RowLabel("Current Morph List",
			         "Pick a predefined morph name list (JSON files in "
			         "SKSE/Plugins/ManaTanks/MorphJsons). Name fields then offer the "
			         "list's display names; the internal morph name is what gets "
			         "saved. Leave empty for manual entry.");
			const std::string listPreview = Settings::morphList.empty()
			    ? "Select a morph list..."
			    : Settings::morphList;
			if (BeginCombo("##morphList", listPreview.c_str())) {
				if (Selectable("Select a morph list...##empty", Settings::morphList.empty())) {
					if (!Settings::morphList.empty()) {
						Settings::morphList.clear();
						pendingMorph.clear();
						Settings::MarkDirty();
					}
				}
				const auto& files  = MorphLists::ListFiles();
				const bool  listed =
				    std::find(files.begin(), files.end(), Settings::morphList) != files.end();
				for (std::size_t i = 0; i < files.size(); ++i) {
					const bool        sel = (files[i] == Settings::morphList);
					const std::string itemLabel = files[i] + "##" + std::to_string(i);
					if (Selectable(itemLabel.c_str(), sel) && !sel) {
						Settings::morphList = files[i];
						pendingMorph.clear();
						Settings::MarkDirty();
					}
				}
				if (!Settings::morphList.empty() && !listed) {
					Selectable((Settings::morphList + " (file not found)##missing").c_str(), true,
					           ImGuiMCP::ImGuiSelectableFlags_Disabled);
				}
				EndCombo();
			}
			if (inListMode && listEntries->empty()) {
				TextDisabled("This list contains no morphs.");
			}
			Spacing();

			if (inListMode) {
				TextWrapped("Morph names are picked from the selected list's display names; the "
				            "internal morph name is saved and applied via RaceMenu (NiOverride). "
				            "The computed value is multiplied by each row's Value (negative inverts "
				            "the morph). Names must exist in your body's TRI morph data; unknown "
				            "names are ignored.");
			} else {
				TextWrapped("Morphs applied via RaceMenu (NiOverride). The computed value is multiplied by "
				            "each row's Value (negative inverts the morph). Names must exist in your body's "
				            "TRI morph data (e.g. BodySlide-built bodies); unknown names are ignored. "
				            "Press Enter or click away to commit an edited name.");
			}
			Spacing();

			auto&       morphs = Settings::morphs;
			nameBufs.resize(morphs.size());
			std::size_t eraseIdx = std::string::npos;

			constexpr auto tableFlags = ImGuiMCP::ImGuiTableFlags_Borders |
			                            ImGuiMCP::ImGuiTableFlags_Resizable |
			                            ImGuiMCP::ImGuiTableFlags_RowBg |
			                            ImGuiMCP::ImGuiTableFlags_SizingStretchProp;
			if (BeginTable("##rows", 3, tableFlags)) {
				TableSetupColumn("Morph", ImGuiMCP::ImGuiTableColumnFlags_WidthStretch, 1.0f);
				TableSetupColumn("Value", ImGuiMCP::ImGuiTableColumnFlags_WidthFixed, 90.0f);
				TableSetupColumn("##del", ImGuiMCP::ImGuiTableColumnFlags_WidthFixed, 30.0f);
				TableHeadersRow();

				for (std::size_t m = 0; m < morphs.size(); ++m) {
					auto& entry = morphs[m];
					auto& buf   = nameBufs[m];
					PushID(static_cast<int>(m));
					TableNextRow();

					TableNextColumn();
					SetNextItemWidth(-FLT_MIN);
					if (inListMode) {
						const std::string preview = MorphLists::DisplayNameFor(Settings::morphList, entry.name);
						std::string       picked;
						if (MorphCombo("##name", preview, *listEntries, entry.name, picked, &morphs, &entry) &&
						    !picked.empty() && picked != entry.name) {
							const auto dupe = [&](const Settings::MorphEntry& e) { return e.name == picked; };
							if (std::find_if(morphs.begin(), morphs.end(), dupe) == morphs.end()) {
								// rename: the old morph name leaves the config, so it
								// must be cleared on the actor like a deleted row
								removed.push_back(entry.name);
								entry.name = picked;
								Settings::MarkDirty();
							}
						}
					} else {
						bool commit = InputText("##name", buf.data(), BUF_LEN,
						                        ImGuiMCP::ImGuiInputTextFlags_EnterReturnsTrue);
						commit = commit || IsItemDeactivatedAfterEdit();
						if (commit) {
							const std::string name = Trim(buf.data());
							const auto        dupe = [&](const Settings::MorphEntry& e) { return e.name == name; };
							if (!name.empty() && name != entry.name &&
							    std::find_if(morphs.begin(), morphs.end(), dupe) == morphs.end()) {
								// rename: the old morph name leaves the config, so it
								// must be cleared on the actor like a deleted row
								removed.push_back(entry.name);
								entry.name = name;
								Settings::MarkDirty();
							}
							SetBuf(buf, entry.name);
						} else if (!IsItemActive() && entry.name != buf.data()) {
							SetBuf(buf, entry.name);
						}
					}

					TableNextColumn();
					SetNextItemWidth(-FLT_MIN);
					if (InputFloat("##val", &entry.size, 0.0f, 0.0f, "%.2f")) {
						Settings::MarkDirty();
					}

					TableNextColumn();
					if (SmallButton("X")) {
						eraseIdx = m;
					}
					PopID();
				}

				TableNextRow();
				PushID("new");
				TableNextColumn();
				SetNextItemWidth(-FLT_MIN);
				bool add = false;
				if (inListMode) {
					const std::string preview = pendingMorph.empty()
					    ? "select morph..."
					    : MorphLists::DisplayNameFor(Settings::morphList, pendingMorph);
					std::string picked;
					if (MorphCombo("##name", preview, *listEntries, pendingMorph, picked, &morphs, nullptr) &&
					    !picked.empty()) {
						pendingMorph = picked;
					}
				} else {
					add = InputTextWithHint("##name", "new morph name", newNameBuf, BUF_LEN,
					                     ImGuiMCP::ImGuiInputTextFlags_EnterReturnsTrue);
				}
				TableNextColumn();
				SetNextItemWidth(-FLT_MIN);
				InputFloat("##val", &newSize, 0.0f, 0.0f, "%.2f");
				TableNextColumn();
				add = add || SmallButton("+");
				PopID();
				EndTable();

				if (add) {
					std::string name;
					if (inListMode) {
						// pendingMorph can go stale if the JSON changed since it
						// was picked — only accept names still in the list.
						const bool found = std::any_of(listEntries->begin(), listEntries->end(),
						    [&](const MorphLists::Entry& e) { return e.morph == pendingMorph; });
						name = found ? pendingMorph : std::string{};
					} else {
						name = Trim(newNameBuf);
					}
					const auto dupe = [&](const Settings::MorphEntry& e) { return e.name == name; };
					if (!name.empty() &&
					    std::find_if(morphs.begin(), morphs.end(), dupe) == morphs.end()) {
						morphs.push_back({ name, newSize });
						Settings::MarkDirty();
					}
					newNameBuf[0] = '\0';
					pendingMorph.clear();
					newSize = 1.0f;
				}
			}

			if (eraseIdx != std::string::npos && eraseIdx < morphs.size()) {
				removed.push_back(morphs[eraseIdx].name);
				morphs.erase(morphs.begin() + static_cast<std::ptrdiff_t>(eraseIdx));
				Settings::MarkDirty();
			}

			if (Button("Reset to defaults")) {
				for (const auto& m : morphs) {
					const auto kept = [&](const Settings::MorphEntry& e) { return e.name == m.name; };
					if (std::find_if(Settings::defaultMorphs.begin(), Settings::defaultMorphs.end(), kept) ==
					    Settings::defaultMorphs.end()) {
						removed.push_back(m.name);
					}
				}
				morphs = Settings::defaultMorphs;
				Settings::MarkDirty();
			}
		}

		if (!removed.empty()) {
			Updater::ClearMorphs(std::move(removed));
		}
	}
}
