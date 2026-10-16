#include "Updater.h"

#include "SKEE.h"
#include "Settings.h"

namespace Updater
{
	namespace
	{
		constexpr const char* MORPH_KEY = "ManaTanks.Scale";

		SKEE::IBodyMorphInterface*              g_morph = nullptr;
		std::atomic<bool>                       g_running{ false };
		std::atomic<bool>                       g_skeeOk{ false };
		std::atomic<float>                      g_lastPct{ -1.0f };
		std::atomic<float>                      g_lastInterval{ 0.0f };
		std::atomic<bool>                       g_gameLoaded{ false };
		std::chrono::steady_clock::time_point   g_lastRun{};
		std::chrono::steady_clock::time_point   g_loadedAt{};
		float                                   g_loggedValue = std::numeric_limits<float>::quiet_NaN();

		// skee64 registers its exchange listener at plugin load; plugin load
		// order is not deterministic, so retry until we get the interface.
		bool AcquireMorph()
		{
			if (g_morph) {
				return true;
			}
			if (!GetModuleHandleW(L"skee64")) {
				return false;
			}
			g_morph = SKEE::GetBodyMorphInterface();
			if (!g_morph) {
				return false;
			}
			g_skeeOk.store(true);
			logger::info("Acquired SKEE BodyMorph interface v{}", g_morph->GetVersion());
			return true;
		}
	}

	void Start()
	{
		bool expected = false;
		if (!g_running.compare_exchange_strong(expected, true)) {
			return;
		}
		// A task must not re-enqueue itself: the task queue drains until
		// empty, so a self-chain spins inside a single frame and hangs the
		// main thread. Poll from a worker and enqueue one-shot ticks instead.
		std::thread([] {
			while (true) {
				std::this_thread::sleep_for(std::chrono::milliseconds(4));
				if (auto* tasks = SKSE::GetTaskInterface()) {
					tasks->AddTask(Tick);
				}
			}
		}).detach();
	}

	void SetGameLoaded(bool a_loaded)
	{
		g_gameLoaded.store(a_loaded);
		g_loadedAt = std::chrono::steady_clock::now();
		g_lastRun = {};
	}

	void Tick()
	{
		auto* player = RE::PlayerCharacter::GetSingleton();
		if (!player || !g_gameLoaded.load() ||
		    // Let the save finish settling — RaceMenu rebuilds morph data
		    // during load, and writing morphs mid-rebuild corrupts it.
		    std::chrono::steady_clock::now() - g_loadedAt < std::chrono::seconds(1) ||
		    !player->GetParentCell() || !player->Is3DLoaded()) {
			return;
		}

		const auto snap = Settings::GetSnapshot();
		if (!snap.enabled || !AcquireMorph()) {
			g_lastPct.store(-1.0f);
			return;
		}

		// ActorValueOwner sits at a different base offset per runtime
		// (0xB0 SE, 0xB8 AE) — a direct member call applies a compile-time
		// this-adjustment that is wrong on one runtime. AsActorValueOwner()
		// resolves the offset per runtime version.
		// GetClampedActorValue returns the *clamped current* value, not the
		// pool maximum — cur/cur would pin pct at 1.0. GetActorValueMax is the
		// true maximum (permanent + temporary modifiers).
		auto* avo = player->AsActorValueOwner();
		const float max = player->GetActorValueMax(snap.power);
		const float cur = avo->GetActorValue(snap.power);
		if (!(max > 0.0f)) {
			return;
		}
		const float pct = cur / max;
		g_lastPct.store(pct);

		const float interval = pct < 1.0f ? snap.updateInterval : snap.maintInterval;
		g_lastInterval.store(interval);

		const auto now = std::chrono::steady_clock::now();
		if (now - g_lastRun < std::chrono::duration<float>(interval)) {
			return;
		}
		g_lastRun = now;

		// "Limit by attribute": scale diffs by maxPool/poolMax when the
		// buffed maximum is below the configured Attribute Limit.
		float poolScale = 1.0f;
		if (snap.linkPool && max < snap.poolMax) {
			poolScale = max / snap.poolMax;
		}

		if (snap.morphsEnabled) {
			const float value = pct * std::abs(snap.morphMax - snap.morphMin) * poolScale + snap.morphMin;
			for (const auto& morph : snap.morphs) {
				if (morph.name.empty()) {
					continue;
				}
				g_morph->SetMorph(player, morph.name.c_str(), MORPH_KEY, value * morph.size);
			}
			const float logged = std::round(value * 10.0f) / 10.0f;
			if (g_loggedValue != logged) {
				g_loggedValue = logged;
				logger::info("Apply morphs = {:.3f} (cur {:.1f} / max {:.1f}, poolScale {:.3f})", value, cur, max, poolScale);
			}
		}
		g_morph->UpdateModelWeight(player, false);
	}

	void ClearAllMorphs()
	{
		auto* tasks = SKSE::GetTaskInterface();
		if (!tasks) {
			return;
		}
		tasks->AddTask([] {
			auto* player = RE::PlayerCharacter::GetSingleton();
			if (!player || !AcquireMorph()) {
				return;
			}
			g_morph->ClearBodyMorphKeys(player, MORPH_KEY);
			g_morph->UpdateModelWeight(player, false);
		});
	}

	void ClearMorphs(std::vector<std::string> morphs)
	{
		auto* tasks = SKSE::GetTaskInterface();
		if (!tasks || morphs.empty()) {
			return;
		}
		tasks->AddTask([m = std::move(morphs)] {
			auto* player = RE::PlayerCharacter::GetSingleton();
			if (!player || !AcquireMorph()) {
				return;
			}
			for (const auto& name : m) {
				g_morph->ClearMorph(player, name.c_str(), MORPH_KEY);
			}
			g_morph->UpdateModelWeight(player, false);
		});
	}

	bool  SkeeAvailable() { return g_skeeOk.load(); }
	float LastPct() { return g_lastPct.load(); }
	float LastInterval() { return g_lastInterval.load(); }
}
