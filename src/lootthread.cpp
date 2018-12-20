#include "lootthread.h"
#pragma warning (push, 0)

#include <loot/api.h>

#pragma warning (pop)

#include <thread>
#include <mutex>
#include <ctype.h>
#include <map>
#include <exception>
#include <stdio.h>
#include <stddef.h>
#include <algorithm>
#include <stdexcept>
#include <utility>
#include <sstream>
#include <fstream>
#include <memory>

#include <boost/assign.hpp>
#include <boost/log/trivial.hpp>
#include <boost/format.hpp>
#include <boost/filesystem.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/locale.hpp>
#include <game_settings.h>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>

#include <cpptoml.h>

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <Shlobj.h>

using namespace loot;
namespace fs = boost::filesystem;

using boost::property_tree::ptree;
using boost::property_tree::write_json;
using std::lock_guard;
using std::recursive_mutex;


LOOTWorker::LOOTWorker()
	: m_GameId(GameType::tes5)
	, m_Language(MessageContent::defaultLanguage)
	, m_GameName("Skyrim")
{
}

std::string ToLower(const std::string &text)
{
	std::string result = text;
	std::transform(text.begin(), text.end(), result.begin(), tolower);
	return result;
}


void LOOTWorker::setGame(const std::string &gameName)
{
	static std::map<std::string, GameType> gameMap = boost::assign::map_list_of
	  ("oblivion", GameType::tes4)
		("fallout3", GameType::fo3)
		("fallout4", GameType::fo4)
		("fallout4vr", GameType::fo4vr)
		("falloutnv", GameType::fonv)
		("skyrim", GameType::tes5)
		("skyrimse", GameType::tes5se)
    ("skyrimvr", GameType::tes5vr);

	auto iter = gameMap.find(ToLower(gameName));
	if (iter != gameMap.end()) {
		m_GameName = gameName;
		if (ToLower(gameName) == "skyrimse") {
			m_GameName = "Skyrim Special Edition";
		}
		m_GameId = iter->second;
	}
	else {
		throw std::runtime_error((boost::format("invalid game name \"%1%\"") % gameName).str());
	}
}

void LOOTWorker::setGamePath(const std::string &gamePath)
{
	m_GamePath = gamePath;
}

void LOOTWorker::setOutput(const std::string &outputPath)
{
	m_OutputPath = outputPath;
}

void LOOTWorker::setUpdateMasterlist(bool update)
{
	m_UpdateMasterlist = update;
}
void LOOTWorker::setPluginListPath(const std::string &pluginListPath) {
	m_PluginListPath = pluginListPath;
}

void LOOTWorker::setLanguageCode(const std::string &languageCode) {
	m_Language = languageCode;
}

/*void LOOTWorker::handleErr(unsigned int resultCode, const char *description)
{
if (resultCode != LVAR(loot_ok)) {
const char *errMessage;
unsigned int lastError = LFUNC(loot_get_error_message)(&errMessage);
throw std::runtime_error((boost::format("%1% failed: %2% (code %3%)") % description % errMessage % lastError).str());
} else {
progress(description);
}
}*/


fs::path GetLOOTAppData() {
	TCHAR path[MAX_PATH];

	HRESULT res = ::SHGetFolderPath(nullptr, CSIDL_LOCAL_APPDATA, nullptr, SHGFP_TYPE_CURRENT, path);

	if (res == S_OK) {
		return fs::path(path) / "LOOT";
	}
	else {
		return fs::path("");
	}
}

fs::path LOOTWorker::masterlistPath() {
	return GetLOOTAppData() / m_GameSettings.FolderName() / "masterlist.yaml";
}

fs::path LOOTWorker::userlistPath() {
	return GetLOOTAppData() / m_GameSettings.FolderName() / "userlist.yaml";
}
fs::path LOOTWorker::settingsPath()
{
	return GetLOOTAppData() / "settings.toml";
}

fs::path LOOTWorker::l10nPath()
{
	return GetLOOTAppData() / "resources" / "l10n";
}

fs::path LOOTWorker::dataPath()
{
	return fs::path(m_GamePath) / "Data";
}

std::string LOOTWorker::formatDirty(const PluginCleaningData &cleaningData) {

	const std::string itmRecords = std::to_string(cleaningData.GetITMCount()) + " ITM record(s)";
	const std::string deletedReferences = std::to_string(cleaningData.GetDeletedReferenceCount()) + " deleted reference(s)";
	const std::string deletedNavmeshes = std::to_string(cleaningData.GetDeletedNavmeshCount()) + " deleted navmesh(es)";
	std::string message;
	if (cleaningData.GetITMCount() > 0 && cleaningData.GetDeletedReferenceCount() > 0 && cleaningData.GetDeletedNavmeshCount() > 0)
		message = cleaningData.GetCleaningUtility() + " found " + itmRecords + ", " + deletedReferences + " and " + deletedNavmeshes + ".";
	else if (cleaningData.GetITMCount() == 0 && cleaningData.GetDeletedReferenceCount() == 0 && cleaningData.GetDeletedNavmeshCount() == 0)
		message = cleaningData.GetCleaningUtility() + " found dirty edits.";
	else if (cleaningData.GetITMCount() == 0 && cleaningData.GetDeletedReferenceCount() > 0 && cleaningData.GetDeletedNavmeshCount() > 0)
		message = cleaningData.GetCleaningUtility() + " found " + deletedReferences + " and " + deletedNavmeshes + ".";
	else if (cleaningData.GetITMCount() > 0 && cleaningData.GetDeletedReferenceCount() == 0 && cleaningData.GetDeletedNavmeshCount() > 0)
		message = cleaningData.GetCleaningUtility() + " found " + itmRecords + " and " + deletedNavmeshes + ".";
	else if (cleaningData.GetITMCount() > 0 && cleaningData.GetDeletedReferenceCount() > 0 && cleaningData.GetDeletedNavmeshCount() == 0)
		message = cleaningData.GetCleaningUtility() + " found " + itmRecords + " and " + deletedReferences + ".";
	else if (cleaningData.GetITMCount() > 0)
		message = cleaningData.GetCleaningUtility() + " found " + itmRecords + ".";
	else if (cleaningData.GetDeletedReferenceCount() > 0)
		message = cleaningData.GetCleaningUtility() + " found " + deletedReferences + ".";
	else if (cleaningData.GetDeletedNavmeshCount() > 0)
		message = cleaningData.GetCleaningUtility() + " found " + deletedNavmeshes + ".";

	if (cleaningData.GetInfo().empty()) {
		return Message(MessageType::warn, message).ToSimpleMessage(m_Language).text;
	}

	auto info = cleaningData.GetInfo();
	for (auto& content : info) {
		content = MessageContent(message + " " + content.GetText(), content.GetLanguage());
	}

	return Message(MessageType::warn, info).ToSimpleMessage(m_Language).text;
}

void LOOTWorker::getSettings(const fs::path& file) {
  lock_guard<recursive_mutex> guard(mutex_);

  auto settings = cpptoml::parse_file(file.string());
  auto games = settings->get_table_array("games");
  if (games) {
    for (const auto& game : *games) {
      try {
        GameSettings newSettings;

        auto type = game->get_as<std::string>("type");
        if (!type) {
          throw std::runtime_error("'type' key missing from game settings table");
        }

        auto folder = game->get_as<std::string>("folder");
        if (!folder) {
          throw std::runtime_error("'folder' key missing from game settings table");
        }

        if (*type == GameSettings(GameType::tes4).FolderName()) {
          newSettings = GameSettings(GameType::tes4, *folder);
        }
        else if (*type == GameSettings(GameType::tes5).FolderName()) {
          newSettings = GameSettings(GameType::tes5, *folder);
        }
        else if (*type == GameSettings(GameType::tes5se).FolderName()) {
          newSettings = GameSettings(GameType::tes5se, *folder);
        }
        else if (*type == GameSettings(GameType::tes5vr).FolderName()) {
          newSettings = GameSettings(GameType::tes5vr, *folder);
        }
        else if (*type == GameSettings(GameType::fo3).FolderName()) {
          newSettings = GameSettings(GameType::fo3, *folder);
        }
        else if (*type == GameSettings(GameType::fonv).FolderName()) {
          newSettings = GameSettings(GameType::fonv, *folder);
        }
        else if (*type == GameSettings(GameType::fo4).FolderName()) {
          newSettings = GameSettings(GameType::fo4, *folder);
        }
        else if (*type == GameSettings(GameType::fo4vr).FolderName()) {
          newSettings = GameSettings(GameType::fo4vr, *folder);
        }
        else
          throw std::runtime_error(
            "invalid value for 'type' key in game settings table");

        if (newSettings.Type() == m_GameSettings.Type()) {

          auto name = game->get_as<std::string>("name");
          if (name) {
            newSettings.SetName(*name);
          }

          auto master = game->get_as<std::string>("master");
          if (master) {
            newSettings.SetMaster(*master);
          }

          auto repo = game->get_as<std::string>("repo");
          if (repo) {
            newSettings.SetRepoURL(*repo);
          }

          auto branch = game->get_as<std::string>("branch");
          if (branch) {
            newSettings.SetRepoBranch(*branch);

            auto defaultGame = GameSettings(newSettings.Type());
            if (newSettings.RepoURL() == defaultGame.RepoURL() &&
              newSettings.IsRepoBranchOldDefault()) {
              newSettings.SetRepoBranch(defaultGame.RepoBranch());
            }
          }

          auto path = game->get_as<std::string>("path");
          if (path) {
            newSettings.SetGamePath(*path);
          }

          auto registry = game->get_as<std::string>("registry");
          if (registry) {
            newSettings.SetRegistryKey(*registry);
          }

          m_GameSettings = newSettings;
          break;
        }
      } catch (...) {
        // Skip invalid games.
      }
    }
	}

  m_Language = settings->get_as<std::string>("language").value_or(m_Language);

	if (m_Language != MessageContent::defaultLanguage) {
		BOOST_LOG_TRIVIAL(debug) << "Initialising language settings.";
		BOOST_LOG_TRIVIAL(debug) << "Selected language: " << m_Language;

		//Boost.Locale initialisation: Generate and imbue locales.
		boost::locale::generator gen;
		std::locale::global(gen(m_Language + ".UTF-8"));
		InitialiseLocale(m_Language + ".UTF-8");
		fs::path::imbue(std::locale());
	}
}

int LOOTWorker::run()
{
	// Do some preliminary locale / UTF-8 support setup here, in case the settings file reading requires it.
	//Boost.Locale initialisation: Specify location of language dictionaries.
	boost::locale::generator gen;
	gen.add_messages_path(l10nPath().string());
	gen.add_messages_domain("loot");

	//Boost.Locale initialisation: Generate and imbue locales.
	std::locale::global(gen("en.UTF-8"));
	InitialiseLocale("en.UTF-8");
	fs::path::imbue(std::locale());
    SetLoggingCallback([&](LogLevel level, const char * message) {
        switch (level) {
            case LogLevel::trace:
				progress();
                break;
            case LogLevel::debug:
				progress();
                break;
            case LogLevel::info:
                progress();
                break;
            case LogLevel::warning:
                BOOST_LOG_TRIVIAL(warning) << message;
                break;
            case LogLevel::error:
                BOOST_LOG_TRIVIAL(error) << message;
                break;
            case LogLevel::fatal:
                BOOST_LOG_TRIVIAL(fatal) << message;
                break;
            default:
                BOOST_LOG_TRIVIAL(trace) << message;
                break;
        }
    });

	try {
		// ensure the loot directory exists
		fs::path lootAppData = GetLOOTAppData();
		if (lootAppData.empty()) {
			errorOccured("failed to create loot app data path");
			return 1;
		}

		if (!fs::exists(lootAppData)) {
			fs::create_directory(lootAppData);
		}

		fs::path profile(m_PluginListPath);
		profile = profile.parent_path();
		auto gameHandle = CreateGameHandle(m_GameId, m_GamePath, profile.string());
		auto db = gameHandle->GetDatabase();

    m_GameSettings = GameSettings(m_GameId);

		fs::path settings = settingsPath();

    if (fs::exists(settings))
      getSettings(settings);

		bool mlUpdated = false;
		if (m_UpdateMasterlist) {
			m_ProgressStep = "Checking Masterlist Existence";
			progress();
			if (!fs::exists(masterlistPath())) {
				fs::create_directories(masterlistPath().parent_path());
			}
			m_ProgressStep = "Updating Masterlist";
			progress();

			mlUpdated = db->UpdateMasterlist(masterlistPath().string(), m_GameSettings.RepoURL(), m_GameSettings.RepoBranch());
			if (mlUpdated && !db->IsLatestMasterlist(masterlistPath().string(), m_GameSettings.RepoBranch())) {
				errorOccured(boost::locale::translate("The latest masterlist revision contains a syntax error, LOOT is using the most recent valid revision instead. Syntax errors are usually minor and fixed within hours."));
			}
		}

		fs::path userlist = userlistPath();

		m_ProgressStep = "Loading Lists";
		progress();
		db->LoadLists(masterlistPath().string(), fs::exists(userlist) ? userlistPath().string() : "");

		m_ProgressStep = "Reading Plugins";
		progress();
		std::vector<std::string> pluginsList;
		for (fs::directory_iterator it(dataPath()); it != fs::directory_iterator(); ++it) {
			if (fs::is_regular_file(it->status()) && gameHandle->IsValidPlugin(it->path().filename().string())) {
				std::string name = it->path().filename().string();
				BOOST_LOG_TRIVIAL(info) << "Found plugin: " << name;

				pluginsList.push_back(name);
			}
		}

		m_ProgressStep = "Sorting Plugins";
		progress();

    gameHandle->LoadCurrentLoadOrderState();
		std::vector<std::string> sortedPlugins = gameHandle->SortPlugins(pluginsList);

		m_ProgressStep = "Writing loadorder.txt";
		progress();
		std::ofstream outf(m_PluginListPath);
		if (!outf) {
			errorOccured("failed to open loadorder.txt to rewrite it");
			return 1;
		}
		outf << "# This file was automatically generated by Mod Organizer." << std::endl;
		for (const std::string &plugin : sortedPlugins) {
			outf << plugin << std::endl;
		}
		outf.close();

#if false // TODO: Disabling this as it seems it's never been useful and now it's throwing an exception
		ptree report;

		m_ProgressStep = "Parsing LOOT Messages";
		progress();
		for (size_t i = 0; i < sortedPlugins.size(); ++i) {
			report.add("name", sortedPlugins[i]);

			std::vector<Message> pluginMessages;
			pluginMessages = db->GetGeneralMessages(true);

			if (!pluginMessages.empty()) {
				for (Message message : pluginMessages) {
					const char *type;
					if (message.GetType() == MessageType::say) {
						type = "info";
					}
					else if (message.GetType() == MessageType::warn) {
						type = "warn";
					}
					else if (message.GetType() == MessageType::error) {
						type = "error";
					}
					else {
						type = "unknown";
						errorOccured((boost::format("invalid message type %1%") % type).str());
					}
					report.add("messages.type", type);
					report.add("messages.message", message.ToSimpleMessage(m_Language).text);
				}
			}

			std::set<PluginCleaningData> dirtyInfo = db->GetPluginMetadata(sortedPlugins[i])->GetDirtyInfo(); //Error 5, Access violation
			for (const auto &element : dirtyInfo) {
				report.add("dirty", formatDirty(element));
			}
		}

		std::ofstream buf;
		buf.open(m_OutputPath.c_str());
		write_json(buf, report, false);
#endif
	}
	catch (const std::exception &e) {
		errorOccured((boost::format("LOOT failed: %1%") % e.what()).str());
		return 1;
	}
	progress("Done!");

	return 0;
}

void LOOTWorker::progress(const std::string &step)
{
	BOOST_LOG_TRIVIAL(info) << "[progress] " << m_ProgressStep;
	if (step.size())
		BOOST_LOG_TRIVIAL(info) << "[detail] " << step;
	else
	fflush(stdout);
}

void LOOTWorker::errorOccured(const std::string &message)
{
	BOOST_LOG_TRIVIAL(error) << message;
	fflush(stdout);
}
