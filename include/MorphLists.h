#pragma once

namespace MorphLists
{
	struct Entry
	{
		std::string display;  // JSON key — shown to the user
		std::string morph;    // JSON value — internal morph name saved/used by the mod
	};

	// Rescan the MorphJsons directory; reparse only files whose write time
	// changed. Safe to call every frame — directory iteration is the only
	// per-call cost.
	void Refresh();

	// Stems of every *.json file in the directory, sorted. Filenames keep
	// their extension stripped (e.g. "CBBE 3BA").
	const std::vector<std::string>& ListFiles();

	// Parsed entries for a list name (filename without .json). Empty when the
	// file is missing, empty, or malformed.
	const std::vector<Entry>& GetEntries(const std::string& listName);

	// Display name for an internal morph name; falls back to the morph name
	// itself when the list doesn't contain it.
	std::string DisplayNameFor(const std::string& listName, const std::string& morphName);
}
