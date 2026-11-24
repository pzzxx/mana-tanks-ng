#include "Updater.h"

#include "SKEE.h"
#include "Settings.h"

namespace Updater
{
	namespace
	{
		constexpr const char* MORPH_KEY = "ManaTanks.Scale";

		// Skip morph applications while the computed value stays within this
		// distance of the last applied one — skee's rebuild cost is the
		// body's, not ours, so redundant applies are what tax the CPU. The
		// resync interval bounds how long an external morph reset (RaceMenu
		// rebuild, cell change) can go unnoticed.
		constexpr float kValueEpsilon   = 0.02f;
		constexpr auto  kResyncInterval = std::chrono::seconds(5);

		// skee64's ApplyBodyMorphs deep-copies every skin partition of every
		// previously-morphed shape, replays all morph deltas per vertex,
		// recalculates normals and re-uploads the vertex buffers — tens of
		// ms on a dense body. Floor the apply interval so rebuilds can
		// never exceed ~10% of the main thread; cheap bodies are unaffected
		// because the floor lands far below the configured interval.
		constexpr float                kRebuildBudgetShare  = 0.1f;
		constexpr std::chrono::milliseconds kMaxApplyInterval{ 1000 };

		// A morph whose effective value is ~0 is cleared rather than set,
		// keeping skee's per-actor morph map free of phantom zero keys.
		constexpr float kMorphEpsilon = 0.0001f;

		SKEE::IBodyMorphInterface*              g_morph = nullptr;
		std::atomic<bool>                       g_skeeOk{ false };
		std::atomic<float>                      g_lastPct{ -1.0f };
		std::atomic<float>                      g_lastInterval{ 0.0f };
		std::atomic<bool>                       g_gameLoaded{ false };
		std::chrono::steady_clock::time_point   g_lastRun{};
		std::chrono::steady_clock::time_point   g_loadedAt{};
		std::chrono::steady_clock::time_point   g_lastResync{};
		std::chrono::steady_clock::time_point   g_lastLog{};
		float                                   g_loggedValue = std::numeric_limits<float>::quiet_NaN();

		// Settings snapshot cached on the game thread; refreshed only when
		// Settings::Generation() changes, keeping the per-frame path free of
		// the settings lock and vector copies.
		Settings::Snapshot                      g_snap;
		std::uint64_t                           g_settingsGen = (std::numeric_limits<std::uint64_t>::max)();

		// Last morph state actually pushed to skee64 — drives change
		// detection so unchanged values cost nothing.
		bool                                    g_appliedValid = false;
		float                                   g_appliedValue = 0.0f;
		std::vector<Settings::MorphEntry>       g_appliedMorphs;

		// EMA of the last apply's wall-time in ms; 0 until the first apply
		// is measured, so the adaptive floor starts out inert.
		float                                   g_applyCostMs = 0.0f;

		bool SameMorphs(const std::vector<Settings::MorphEntry>& a,
		                const std::vector<Settings::MorphEntry>& b)
		{
			if (a.size() != b.size()) {
				return false;
			}
			for (std::size_t i = 0; i < a.size(); ++i) {
				if (a[i].name != b[i].name || a[i].size != b[i].size) {
					return false;
				}
			}
			return true;
		}

		// SKSE loads plugins in reverse alphabetical order, so skee64 is
		// always loaded before ManaTanks and its exchange listener is
		// registered before our plugin load — no retry needed.
		bool AcquireMorph()
		{
			if (!g_morph) {
				g_morph = SKEE::GetBodyMorphInterface();
				if (g_morph) {
					g_skeeOk.store(true);
					logger::info("Acquired SKEE BodyMorph interface v{}", g_morph->GetVersion());
				}
			}
			return g_morph != nullptr;
		}

		REL::Relocation<void(RE::PlayerCharacter*, float)> s_playerUpdate;

		void UpdatePlayer(RE::PlayerCharacter* a_player, float a_delta)
		{
			s_playerUpdate(a_player, a_delta);
			Tick();
		}
	}

	void Start()
	{
		static std::once_flag once;
		std::call_once(once, [] {
			REL::Relocation<std::uintptr_t> vtbl{ RE::PlayerCharacter::VTABLE[0] };
			s_playerUpdate = vtbl.write_vfunc(0xAD, &UpdatePlayer);
			AcquireMorph();
		});
	}

	void SetGameLoaded(bool a_loaded)
	{
		g_gameLoaded.store(a_loaded);
		g_loadedAt = std::chrono::steady_clock::now();
		g_lastRun = {};
		// Morph keys live on the actor/save — a new game or reload starts
		// from a clean slate, so force the next apply to re-push everything.
		g_appliedValid = false;
		g_appliedMorphs.clear();
		g_appliedValue = 0.0f;
		g_lastResync = {};
	}

	void Tick()
	{
		auto* player = RE::PlayerCharacter::GetSingleton();
		if (!player || !g_gameLoaded.load()) {
			return;
		}
		const auto now = std::chrono::steady_clock::now();
		if (// Let the save finish settling — RaceMenu rebuilds morph data
		    // during load, and writing morphs mid-rebuild corrupts it.
		    now - g_loadedAt < std::chrono::seconds(1) ||
		    !player->GetParentCell() || !player->Is3DLoaded()) {
			return;
		}

		// Cheap change check: the UI thread bumps the generation on every
		// settings edit, so the per-frame path never takes the settings
		// lock or copies the morph list.
		if (Settings::Generation() != g_settingsGen) {
			g_snap = Settings::GetSnapshot();
			g_settingsGen = Settings::Generation();
		}
		if (!g_snap.enabled) {
			g_lastPct.store(-1.0f);
			return;
		}

		// The interval gate uses the percentage from the previous apply;
		// it lags a bar crossing 100% by at most one interval, which is
		// immaterial at these rates. Actor-value reads and all skee64 work
		// only happen once the gate opens.
		float interval = g_lastPct.load() >= 1.0f ? g_snap.maintInterval : g_snap.updateInterval;
		if (g_applyCostMs > 0.0f) {
			// Adaptive floor: rebuilds may take at most kRebuildBudgetShare
			// of the main thread, so the minimum spacing is cost/share —
			// capped so morphs can never fall more than kMaxApplyInterval
			// behind.
			const float floorSec = (std::min)(g_applyCostMs / kRebuildBudgetShare,
			                                  static_cast<float>(kMaxApplyInterval.count())) / 1000.0f;
			interval = (std::max)(interval, floorSec);
		}
		g_lastInterval.store(interval);
		if (now - g_lastRun < std::chrono::duration<float>(interval)) {
			return;
		}
		g_lastRun = now;

		if (!AcquireMorph()) {
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
		const float max = player->GetActorValueMax(g_snap.power);
		const float cur = avo->GetActorValue(g_snap.power);
		if (!(max > 0.0f)) {
			return;
		}
		const float pct = cur / max;
		g_lastPct.store(pct);

		// "Limit by attribute": scale diffs by maxPool/poolMax when the
		// buffed maximum is below the configured Attribute Limit.
		float poolScale = 1.0f;
		if (g_snap.linkPool && max < g_snap.poolMax) {
			poolScale = max / g_snap.poolMax;
		}

		if (g_snap.morphsEnabled && !g_snap.morphs.empty()) {
			const float value = pct * std::abs(g_snap.morphMax - g_snap.morphMin) * poolScale + g_snap.morphMin;

			const bool resync = now - g_lastResync >= kResyncInterval;
			if (resync) {
				g_lastResync = now;
			}
			if (resync || !g_appliedValid || !SameMorphs(g_appliedMorphs, g_snap.morphs) ||
			    std::abs(value - g_appliedValue) > kValueEpsilon) {
				// ApplyBodyMorphs runs the full skee rebuild synchronously on
				// this thread — identical work to what UpdateModelWeight queues
				// through the task interface (skee's deferUpdate parameter is
				// dead code; the partition updates key off the calling
				// thread), but inline so the cost is measurable here.
				const auto applyStart = std::chrono::steady_clock::now();
				for (const auto& morph : g_snap.morphs) {
					if (morph.name.empty()) {
						continue;
					}
					const float effective = value * morph.size;
					if (std::abs(effective) < kMorphEpsilon) {
						g_morph->ClearMorph(player, morph.name.c_str(), MORPH_KEY);
					} else {
						g_morph->SetMorph(player, morph.name.c_str(), MORPH_KEY, effective);
					}
				}
				g_morph->ApplyBodyMorphs(player, true);
				const float costMs = std::chrono::duration<float, std::milli>(
				    std::chrono::steady_clock::now() - applyStart).count();
				g_applyCostMs = g_applyCostMs == 0.0f ? costMs
				                                   : g_applyCostMs * 0.6f + costMs * 0.4f;
				g_appliedValid = true;
				g_appliedValue = value;
				g_appliedMorphs = g_snap.morphs;

				const float logged = std::round(value * 10.0f) / 10.0f;
				const bool  bigJump = std::abs(logged - g_loggedValue) >= 0.5f;
				if (g_loggedValue != logged &&
				    (bigJump || now - g_lastLog >= std::chrono::seconds(1))) {
					g_lastLog = now;
					g_loggedValue = logged;
					logger::info("Apply morphs = {:.3f} (cur {:.1f} / max {:.1f}, poolScale {:.3f}, rebuild {:.1f} ms)",
					             value, cur, max, poolScale, costMs);
				}
			}
		}
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
			g_morph->ApplyBodyMorphs(player, true);
			g_appliedValid = false;
			g_appliedMorphs.clear();
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
			g_morph->ApplyBodyMorphs(player, true);
			g_appliedValid = false;
		});
	}

	bool  SkeeAvailable() { return g_skeeOk.load(); }
	float LastPct() { return g_lastPct.load(); }
	float LastInterval() { return g_lastInterval.load(); }
}
