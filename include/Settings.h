#pragma once

namespace Settings
{
	struct MorphEntry
	{
		std::string name;
		float       size = 1.0f;  // multiplier on the computed morph value
	};

	struct Snapshot
	{
		bool                    enabled = true;
		RE::ActorValue          power = RE::ActorValue::kMagicka;
		float                   updateInterval = 0.15f;
		float                   maintInterval = 5.0f;
		bool                    linkPool = true;
		float                   poolMax = 1000.0f;
		bool                    morphsEnabled = true;
		float                   morphMin = 0.0f;
		float                   morphMax = 1.0f;
		std::vector<MorphEntry> morphs;
	};

	extern std::mutex             lock;
	extern bool                   enabled;
	extern int                    power;  // 1 = Magicka, 2 = Health, 3 = Stamina
	extern float                  updateInterval;
	extern float                  maintInterval;
	extern bool                   linkPool;
	extern int                    poolMax;
	extern bool                   morphsEnabled;
	extern float                  morphMin;
	extern float                  morphMax;
	extern std::vector<MorphEntry> morphs;
	extern std::vector<MorphEntry> defaultMorphs;

	// Call before SKSEPluginLoad finishes (creates the file if absent).
	void Load();

	// Mark the config as modified; the write is flushed on the next
	// FlushIfDue() call once ~0.5s have passed without further edits.
	void MarkDirty();

	// Called once per UI frame — writes pending changes to disk when due.
	void FlushIfDue();

	// Deep copy for the game-thread tick.
	Snapshot GetSnapshot();

	[[nodiscard]] constexpr RE::ActorValue PowerToActorValue(int a_power) noexcept
	{
		switch (a_power) {
		case 2:
			return RE::ActorValue::kHealth;
		case 3:
			return RE::ActorValue::kStamina;
		default:
			return RE::ActorValue::kMagicka;
		}
	}
}
