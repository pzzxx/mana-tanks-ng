#include "MorphLists.h"

#include <cctype>
#include <map>

namespace MorphLists
{
	namespace
	{
		struct CachedList
		{
			std::filesystem::path                     path;
			std::filesystem::file_time_type           writeTime{};
			std::uintmax_t                            size = 0;
			bool                                      parsed = false;
			std::vector<Entry>                        entries;
			std::unordered_map<std::string, std::string> morphToDisplay;
		};

		std::filesystem::path MorphJsonsDir()
		{
			// Resolution is deterministic for the process lifetime (DLL location
			// and working directory don't change), so resolve once.
			static const auto dir = [] {
				// Primary: sibling of the plugin DLL — <dllDir>/ManaTanks/MorphJsons.
				// Under MO2 the DLL is loaded from the real mod path, so this
				// resolves regardless of the process working directory.
				std::filesystem::path dllDir;
				HMODULE                hMod = nullptr;
				if (GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
				                       reinterpret_cast<LPCWSTR>(&MorphJsonsDir), &hMod) &&
				    hMod) {
					std::wstring buf(32768, L'\0');
					const DWORD  n = GetModuleFileNameW(hMod, buf.data(), static_cast<DWORD>(buf.size()));
					if (n > 0 && n < buf.size()) {
						dllDir = std::filesystem::path(std::wstring_view(buf.data(), n)).parent_path();
					}
				}
				if (!dllDir.empty()) {
					const auto p = dllDir / "ManaTanks" / "MorphJsons";
					std::error_code ec;
					if (std::filesystem::exists(p, ec)) {
						return p;
					}
				}

				// Fallbacks for installs that place the files under Data/.
				for (const auto* candidate : { "SKSE/Plugins/ManaTanks/MorphJsons",
				                               "Data/SKSE/Plugins/ManaTanks/MorphJsons" }) {
					std::error_code ec;
					if (std::filesystem::exists(candidate, ec)) {
						return std::filesystem::path(candidate);
					}
				}

				return dllDir.empty() ? std::filesystem::path("SKSE/Plugins/ManaTanks/MorphJsons")
				                      : dllDir / "ManaTanks" / "MorphJsons";
			}();
			return dir;
		}

		bool IsJsonFile(const std::filesystem::path& a_path)
		{
			std::string ext = a_path.extension().generic_string();
			for (auto& c : ext) {
				c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
			}
			return ext == ".json";
		}

		// generic_string()/u8string() on Windows: generic_string returns the
		// ANSI code page (not UTF-8), which would corrupt non-ASCII filenames
		// in the config and the UI. u8string() is always UTF-8; char8_t→char
		// is a bit-preserving copy since char8_t values are already UTF-8.
		std::string StemUtf8(const std::filesystem::path& a_path)
		{
			const auto u8 = a_path.stem().u8string();
			std::string out;
			out.reserve(u8.size());
			for (const char8_t c : u8) {
				out.push_back(static_cast<char>(c));
			}
			return out;
		}

		void ParseList(CachedList& a_slot)
		{
			try {
				std::ifstream file(a_slot.path);
				const auto    j = nlohmann::json::parse(file);
				if (!j.is_object()) {
					logger::warn("{}: expected a JSON object, got {}", a_slot.path.generic_string(), j.type_name());
					return;
				}
				for (const auto& item : j.items()) {
					const auto& display = item.key();
					const auto& morph   = item.value();
					if (!morph.is_string() || display.empty()) {
						continue;
					}
					const auto m = morph.get<std::string>();
					if (m.empty()) {
						continue;
					}
					a_slot.entries.push_back({ display, m });
					a_slot.morphToDisplay.emplace(m, display);
				}
			} catch (const std::exception& e) {
				logger::warn("Failed to parse {}: {}", a_slot.path.generic_string(), e.what());
			}
		}

		std::vector<std::string>                              files;
		std::map<std::string, CachedList>                     cache;
		const std::vector<Entry>                              emptyEntries;
	}

	void Refresh()
	{
		// Called every frame while the Morphs tab is open — the directory
		// scan is throttled so it runs at most twice a second; new files
		// still appear promptly.
		static std::chrono::steady_clock::time_point lastRefresh{};
		const auto now = std::chrono::steady_clock::now();
		if (now - lastRefresh < std::chrono::milliseconds(500)) {
			return;
		}
		lastRefresh = now;

		const auto dir = MorphJsonsDir();

		std::vector<std::pair<std::string, std::filesystem::path>> found;
		std::error_code                                             ec;
		if (std::filesystem::is_directory(dir, ec)) {
			for (const auto& entry : std::filesystem::directory_iterator(dir, ec)) {
				std::error_code ec2;
				if (!entry.is_regular_file(ec2) || !IsJsonFile(entry.path())) {
					continue;
				}
				found.emplace_back(StemUtf8(entry.path()), entry.path());
			}
		}
		std::sort(found.begin(), found.end(),
		          [](const auto& a, const auto& b) { return a.first < b.first; });

		files.clear();
		files.reserve(found.size());
		for (const auto& [stem, path] : found) {
			files.push_back(stem);
		}

		// Drop cached lists whose file was deleted.
		for (auto it = cache.begin(); it != cache.end();) {
			if (!std::binary_search(files.begin(), files.end(), it->first)) {
				it = cache.erase(it);
			} else {
				++it;
			}
		}

		for (const auto& [stem, path] : found) {
			auto&           slot = cache[stem];
			std::error_code ec3;
			const auto      writeTime = std::filesystem::last_write_time(path, ec3);
			const auto      size      = std::filesystem::file_size(path, ec3);
			if (ec3) {
				continue;  // can't stat — keep whatever we had rather than reparse-spam
			}
			if (!slot.parsed || writeTime != slot.writeTime || size != slot.size) {
				slot         = CachedList{};
				slot.path    = path;
				slot.writeTime = writeTime;
				slot.size    = size;
				slot.parsed  = true;
				ParseList(slot);
			}
		}
	}

	const std::vector<std::string>& ListFiles()
	{
		return files;
	}

	const std::vector<Entry>& GetEntries(const std::string& listName)
	{
		const auto it = cache.find(listName);
		return it != cache.end() ? it->second.entries : emptyEntries;
	}

	std::string DisplayNameFor(const std::string& listName, const std::string& morphName)
	{
		const auto it = cache.find(listName);
		if (it == cache.end()) {
			return morphName;
		}
		const auto jt = it->second.morphToDisplay.find(morphName);
		return jt != it->second.morphToDisplay.end() ? jt->second : morphName;
	}
}
