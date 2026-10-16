#include "UI.h"

#include <cfloat>
#include <cstring>

#include "Settings.h"
#include "SKSEMenuFramework.h"
#include "Updater.h"

namespace UI
{
	namespace
	{
		using ImGuiMCP::AlignTextToFramePadding;
		using ImGuiMCP::BeginDisabled;
		using ImGuiMCP::BeginTable;
		using ImGuiMCP::Button;
		using ImGuiMCP::Checkbox;
		using ImGuiMCP::Combo;
		using ImGuiMCP::EndDisabled;
		using ImGuiMCP::EndTable;
		using ImGuiMCP::GetContentRegionMax;
		using ImGuiMCP::GetFrameHeight;
		using ImGuiMCP::InputFloat;
		using ImGuiMCP::InputText;
		using ImGuiMCP::InputTextWithHint;
		using ImGuiMCP::IsItemActive;
		using ImGuiMCP::IsItemDeactivatedAfterEdit;
		using ImGuiMCP::IsItemHovered;
		using ImGuiMCP::PopID;
		using ImGuiMCP::PushID;
		using ImGuiMCP::SameLine;
		using ImGuiMCP::Separator;
		using ImGuiMCP::SeparatorText;
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

		constexpr float       LABEL_X = 160.0f;  // x offset of the control column in settings rows
		constexpr std::size_t BUF_LEN = 256;

		void HelpMarker(const char* text)
		{
			SameLine();
			TextDisabled("(?)");
			if (IsItemHovered()) {
				SetTooltip("%s", text);
			}
		}

		// "Label (?)" on the left, cursor moved to the control column; the next
		// widget is sized to fill the rest of the row.
		void RowLabel(const char* label, const char* help)
		{
			AlignTextToFramePadding();
			Text("%s", label);
			if (help) {
				HelpMarker(help);
			}
			SameLine(LABEL_X);
			SetNextItemWidth(-FLT_MIN);
		}

		// Checkbox variant: the box anchors to the row's right edge.
		bool RowCheckbox(const char* label, const char* help, bool* v, const char* id)
		{
			AlignTextToFramePadding();
			Text("%s", label);
			if (help) {
				HelpMarker(help);
			}
			SameLine(GetContentRegionMax().x - GetFrameHeight());
			return Checkbox(id, v);
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

		TextWrapped("Morphs applied via RaceMenu (NiOverride). The computed value is multiplied by "
		            "each row's Value (negative inverts the morph). Names must exist in your body's "
		            "TRI morph data (e.g. BodySlide-built bodies); unknown names are ignored. "
		            "Press Enter or click away to commit an edited name.");
		Spacing();

		std::vector<std::string> removed;
		{
			std::lock_guard guard(Settings::lock);

			// Per-row name edit buffers plus the "add row" inputs. Buffers are
			// refreshed from the config whenever their field isn't being edited,
			// so external changes (delete/reset/Escape) self-correct.
			static std::vector<std::array<char, BUF_LEN>> nameBufs;
			static char                                 newNameBuf[BUF_LEN] = {};
			static float                                newSize = 1.0f;

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
				bool add = InputTextWithHint("##name", "new morph name", newNameBuf, BUF_LEN,
				                             ImGuiMCP::ImGuiInputTextFlags_EnterReturnsTrue);
				TableNextColumn();
				SetNextItemWidth(-FLT_MIN);
				InputFloat("##val", &newSize, 0.0f, 0.0f, "%.2f");
				TableNextColumn();
				add = add || SmallButton("+");
				PopID();
				EndTable();

				if (add) {
					const std::string name = Trim(newNameBuf);
					const auto        dupe = [&](const Settings::MorphEntry& e) { return e.name == name; };
					if (!name.empty() &&
					    std::find_if(morphs.begin(), morphs.end(), dupe) == morphs.end()) {
						morphs.push_back({ name, newSize });
						Settings::MarkDirty();
					}
					newNameBuf[0] = '\0';
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
