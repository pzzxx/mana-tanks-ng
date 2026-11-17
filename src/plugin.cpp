#include "Settings.h"
#include "UI.h"
#include "Updater.h"

namespace
{
	void SetupLog()
	{
		const auto logsFolder = SKSE::log::log_directory();
		if (!logsFolder) {
			return;
		}

		auto path = *logsFolder / "ManaTanks.log";
		auto sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(path.string(), true);
		auto log  = std::make_shared<spdlog::logger>("global", std::move(sink));
		spdlog::set_default_logger(std::move(log));
		spdlog::set_level(spdlog::level::info);
		spdlog::flush_on(spdlog::level::info);
	}

	void OnMessage(SKSE::MessagingInterface::Message* a_msg)
	{
		if (!a_msg) {
			return;
		}
		switch (a_msg->type) {
		case SKSE::MessagingInterface::kDataLoaded:
			UI::Register();
			Updater::Start();
			break;
		case SKSE::MessagingInterface::kNewGame:
		case SKSE::MessagingInterface::kPostLoadGame:
			Updater::Start();
			Updater::SetGameLoaded(true);
			break;
		case SKSE::MessagingInterface::kPreLoadGame:
			Updater::SetGameLoaded(false);
			break;
		default:
			break;
		}
	}
}

SKSEPluginLoad(const SKSE::LoadInterface* a_skse)
{
	SKSE::Init(a_skse);
	SetupLog();

	Settings::Load();
	SKSE::GetMessagingInterface()->RegisterListener(OnMessage);

	logger::info("Mana Tanks NG loaded");
	return true;
}
