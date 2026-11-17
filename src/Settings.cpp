#include "Settings.h"

namespace Settings
{
	std::mutex              lock;
	bool                    enabled = true;
	int                     power = 1;
	float                   updateInterval = 0.15f;
	float                   maintInterval = 5.0f;
	bool                    linkPool = true;
	int                     poolMax = 1000;
	bool                    morphsEnabled = true;
	float                   morphMin = 0.0f;
	float                   morphMax = 1.0f;
	std::vector<MorphEntry> morphs;
	std::vector<MorphEntry> defaultMorphs;

	namespace
	{
		std::atomic<bool>                     dirty{ false };
		std::chrono::steady_clock::time_point lastEdit;

		std::filesystem::path ConfigPath()
		{
			return std::filesystem::path("Data") / "SKSE" / "Plugins" / "ManaTanks.json";
		}

		// Rounds to 2 decimals in double precision — floats like 0.4f serialize
		// as 0.4000000059604645 otherwise, which is unreadable in the config.
		double Round2(float v)
		{
			return std::round(static_cast<double>(v) * 100.0) / 100.0;
		}

		// Caller must hold `lock`.
		void SaveUnlocked()
		{
			nlohmann::json j;
			j["enabled"] = enabled;
			j["power"] = power;
			j["updateInterval"] = Round2(updateInterval);
			j["maintInterval"] = Round2(maintInterval);
			j["linkPool"] = linkPool;
			j["poolMax"] = poolMax;

			auto& ms = j["morphSizes"];
			ms["enabled"] = morphsEnabled;
			ms["min"] = Round2(morphMin);
			ms["max"] = Round2(morphMax);
			ms["morphs"] = nlohmann::json::array();
			for (const auto& m : morphs) {
				ms["morphs"].push_back({ { "name", m.name }, { "size", Round2(m.size) } });
			}

			const auto     path = ConfigPath();
			std::error_code ec;
			std::filesystem::create_directories(path.parent_path(), ec);
			std::ofstream file(path, std::ios::trunc);
			if (!file) {
				logger::error("Failed to write {}", path.generic_string());
				return;
			}
			file << j.dump(4);
		}

		void LoadMorphs()
		{
			const auto path = ConfigPath();
			if (!std::filesystem::exists(path)) {
				logger::info("No config file found, writing defaults to {}", path.generic_string());
				return;
			}

			try {
				std::ifstream file(path);
				const auto    j = nlohmann::json::parse(file);

				enabled = j.value("enabled", enabled);
				power = j.value("power", power);
				updateInterval = j.value("updateInterval", updateInterval);
				maintInterval = j.value("maintInterval", maintInterval);
				linkPool = j.value("linkPool", linkPool);
				poolMax = j.value("poolMax", poolMax);

				if (j.contains("morphSizes") && j["morphSizes"].is_object()) {
					const auto& ms = j["morphSizes"];
					morphsEnabled = ms.value("enabled", morphsEnabled);
					morphMin = ms.value("min", morphMin);
					morphMax = ms.value("max", morphMax);
					if (ms.contains("morphs") && ms["morphs"].is_array()) {
						morphs.clear();
						for (const auto& m : ms["morphs"]) {
							if (m.is_string()) {
								// legacy plain-name entries default to a 1.0 multiplier
								morphs.push_back({ m.get<std::string>(), 1.0f });
							} else if (m.is_object() && m.value("name", std::string{}).size()) {
								morphs.push_back({ m.value("name", std::string{}), m.value("size", 1.0f) });
							}
						}
					}
				}
			} catch (const std::exception& e) {
				logger::warn("Failed to parse {}: {}. Defaults restored.", path.generic_string(), e.what());
			}
		}
	}

	void Load()
	{
		std::lock_guard guard(lock);

		defaultMorphs = {
			{ "Breasts", 1.0f }, { "BreastsNewSH", 0.5f }, { "BreastFantasy", 0.5f },
			{ "DoubleMelon", 0.5f },
		};
		morphs = defaultMorphs;

		LoadMorphs();
		SaveUnlocked();
	}

	void MarkDirty()
	{
		dirty.store(true);
		lastEdit = std::chrono::steady_clock::now();
	}

	void FlushIfDue()
	{
		if (!dirty.load() || std::chrono::steady_clock::now() - lastEdit < std::chrono::milliseconds(500)) {
			return;
		}
		dirty.store(false);
		std::lock_guard guard(lock);
		SaveUnlocked();
	}

	Snapshot GetSnapshot()
	{
		std::lock_guard guard(lock);
		Snapshot        snap;
		snap.enabled = enabled;
		snap.power = PowerToActorValue(power);
		snap.updateInterval = updateInterval;
		snap.maintInterval = maintInterval;
		snap.linkPool = linkPool;
		snap.poolMax = static_cast<float>(poolMax);
		snap.morphsEnabled = morphsEnabled;
		snap.morphMin = morphMin;
		snap.morphMax = morphMax;
		snap.morphs = morphs;
		return snap;
	}
}
