#pragma once

namespace Updater
{
	// Start the scheduler thread that enqueues Tick tasks. Idempotent.
	void Start();

	// Game-thread entry point, driven by the scheduler thread.
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
