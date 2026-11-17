#pragma once

namespace Updater
{
	// Install the PlayerCharacter::Update hook that drives Tick. Idempotent.
	void Start();

	// Per-frame entry point, driven by the player Update hook on the main
	// thread. Rate-limits itself against the configured interval.
	void Tick();

	// Track save-load state; morphs only apply while a game is loaded.
	void SetGameLoaded(bool a_loaded);

	// Remove every morph this mod owns (key "ManaTanks.Scale"). Safe to call
	// from any thread — the work is dispatched to the game task queue.
	void ClearAllMorphs();

	// Remove the given morph names from the player. Any thread.
	void ClearMorphs(std::vector<std::string> morphs);

	[[nodiscard]] bool  SkeeAvailable();
	[[nodiscard]] float LastPct();
	[[nodiscard]] float LastInterval();
}
