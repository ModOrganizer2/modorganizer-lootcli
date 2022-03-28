#include "lootthread.h"
#include "game_settings.h"
#include "version.h"

//using namespace loot;
namespace fs = std::filesystem;

using std::lock_guard;
using std::recursive_mutex;

namespace lootcli
{

std::string toString(loot::MessageType type)
{
  switch (type)
  {
    case loot::MessageType::say:   return "info";
    case loot::MessageType::warn:  return "warn";
    case loot::MessageType::error: return "error";
    default: return "unknown";
  }
}


LOOTWorker::LOOTWorker()
    : m_GameId(loot::GameType::tes5)
    , m_GameName("Skyrim")
    , m_LogLevel(loot::LogLevel::info)
{
}

std::string ToLower(std::string text)
{
    std::transform(
      text.begin(), text.end(), text.begin(),
      [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

    return text;
}


void LOOTWorker::setGame(const std::string& gameName)
{
  static std::map<std::string, loot::GameType> gameMap = {
    {"morrowind",  loot::GameType::tes3},
    {"oblivion",   loot::GameType::tes4},
    {"fallout3",   loot::GameType::fo3},
    {"fallout4",   loot::GameType::fo4},
    {"fallout4vr", loot::GameType::fo4vr},
    {"falloutnv",  loot::GameType::fonv},
    {"skyrim",     loot::GameType::tes5},
    {"skyrimse",   loot::GameType::tes5se},
    {"skyrimvr",   loot::GameType::tes5vr},
  };

    auto iter = gameMap.find(ToLower(gameName));

    if (iter != gameMap.end()) {
        m_GameName = gameName;
        if (ToLower(gameName) == "skyrimse") {
            m_GameName = "Skyrim Special Edition";
        }
        m_GameId = iter->second;
    }
    else {
        throw std::runtime_error("invalid game name \"" + gameName + "\"");
    }
}

void LOOTWorker::setGamePath(const std::string& gamePath)
{
    m_GamePath = gamePath;
}

void LOOTWorker::setOutput(const std::string& outputPath)
{
    m_OutputPath = outputPath;
}

void LOOTWorker::setUpdateMasterlist(bool update)
{
    m_UpdateMasterlist = update;
}

void LOOTWorker::setPluginListPath(const std::string& pluginListPath)
{
    m_PluginListPath = pluginListPath;
}

void LOOTWorker::setLanguageCode(const std::string& languageCode)
{
    m_Language = languageCode;
}

void LOOTWorker::setLogLevel(loot::LogLevel level)
{
  m_LogLevel = level;
}

fs::path GetLOOTAppData() {
    TCHAR path[MAX_PATH];

    HRESULT res = ::SHGetFolderPath(nullptr, CSIDL_LOCAL_APPDATA, nullptr, SHGFP_TYPE_CURRENT, path);

    if (res == S_OK) {
        return fs::path(path) / "LOOT";
    } else {
        return fs::path("");
    }
}

fs::path LOOTWorker::masterlistPath() const
{
    return GetLOOTAppData() / m_GameSettings.FolderName() / "masterlist.yaml";
}

fs::path LOOTWorker::userlistPath() const
{
    return GetLOOTAppData() / m_GameSettings.FolderName() / "userlist.yaml";
}
fs::path LOOTWorker::settingsPath() const
{
    return GetLOOTAppData() / "settings.toml";
}

fs::path LOOTWorker::l10nPath() const
{
    return GetLOOTAppData() / "resources" / "l10n";
}

fs::path LOOTWorker::dataPath() const
{
    return m_GameSettings.DataPath();
}

void LOOTWorker::getSettings(const fs::path& file) {
    lock_guard<recursive_mutex> guard(mutex_);

    auto settings = cpptoml::parse_file(file.string());
    auto games = settings->get_table_array("games");
    if (games) {
        for (const auto& game : *games) {
            try {
                using loot::GameSettings;
                using loot::GameType;

                GameSettings newSettings;

                auto type = game->get_as<std::string>("type");
                if (!type) {
                    throw std::runtime_error("'type' key missing from game settings table");
                }

                auto folder = game->get_as<std::string>("folder");
                if (!folder) {
                    throw std::runtime_error("'folder' key missing from game settings table");
                }

                if (*type == "SkyrimSE" && *folder == *type) {
                    type = cpptoml::option<std::string>(
                        GameSettings(GameType::tes5se).FolderName());
                    folder = type;

                    auto path = GetLOOTAppData() / "SkyrimSE";
                    if (fs::exists(path)) {
                        fs::rename(path, GetLOOTAppData() / fs::path(*folder));
                    }
                }

                if (*type == GameSettings(GameType::tes3).FolderName()) {
                    newSettings = GameSettings(GameType::tes3, *folder);
                } else if (*type == GameSettings(GameType::tes4).FolderName()) {
                    newSettings = GameSettings(GameType::tes4, *folder);
                } else if (*type == GameSettings(GameType::tes5).FolderName()) {
                    newSettings = GameSettings(GameType::tes5, *folder);
                } else if (*type == GameSettings(GameType::tes5se).FolderName()) {
                    newSettings = GameSettings(GameType::tes5se, *folder);
                } else if (*type == GameSettings(GameType::tes5vr).FolderName()) {
                    newSettings = GameSettings(GameType::tes5vr, *folder);
                } else if (*type == GameSettings(GameType::fo3).FolderName()) {
                    newSettings = GameSettings(GameType::fo3, *folder);
                } else if (*type == GameSettings(GameType::fonv).FolderName()) {
                    newSettings = GameSettings(GameType::fonv, *folder);
                } else if (*type == GameSettings(GameType::fo4).FolderName()) {
                    newSettings = GameSettings(GameType::fo4, *folder);
                } else if (*type == GameSettings(GameType::fo4vr).FolderName()) {
                    newSettings = GameSettings(GameType::fo4vr, *folder);
                } else
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

                    auto minimumHeaderVersion = game->get_as<double>("minimumHeaderVersion");
                    if (minimumHeaderVersion) {
                        newSettings.SetMinimumHeaderVersion((float)* minimumHeaderVersion);
                    }

                    auto repo = game->get_as<std::string>("repo");
                    if (repo) {
                        newSettings.SetRepoURL(*repo);
                    }

                    auto branch = game->get_as<std::string>("branch");
                    if (branch) {
                        newSettings.SetRepoBranch(*branch);
                    }

                    auto path = game->get_as<std::string>("path");
                    if (path) {
                        newSettings.SetGamePath(fs::path(*path));
                    }

                    auto localPath = game->get_as<std::string>("local_path");
                    auto localFolder = game->get_as<std::string>("local_folder");
                    if (localPath && localFolder) {
                        throw std::runtime_error(
                            "Game settings have local_path and local_folder set, use only one.");
                    }
                    else if (localPath) {
                        newSettings.SetGameLocalPath(fs::path(*localPath));
                    }
                    else if (localFolder) {
                        newSettings.SetGameLocalFolder(*localFolder);
                    }

                    auto registryArray = game->get_array_of<std::string>("registry");
                    if (registryArray) {
                        newSettings.SetRegistryKeys(*registryArray);
                    }
                    else {
                        auto registry = game->get_as<std::string>("registry");
                        if (registry) {
                            newSettings.SetRegistryKeys({ *registry });
                        }
                    }

                    newSettings.MigrateSettings();

                    m_GameSettings = newSettings;
                    break;
                }
            }
            catch (...) {
                // Skip invalid games.
            }
        }
    }

    if (m_Language.empty()) {
      m_Language = settings->get_as<std::string>("language")
        .value_or(loot::MessageContent::DEFAULT_LANGUAGE);
    }
}

std::string escape(const std::string& s)
{
  return boost::replace_all_copy(s, "\"", "\\\"");
}

int LOOTWorker::run()
{
    m_startTime = std::chrono::high_resolution_clock::now();

    {
      // Do some preliminary locale / UTF-8 support setup here, in case the settings file reading requires it.
      //Boost.Locale initialisation: Specify location of language dictionaries.
      boost::locale::generator gen;
      gen.add_messages_path(l10nPath().string());
      gen.add_messages_domain("loot");

      //Boost.Locale initialisation: Generate and imbue locales.
      std::locale::global(gen("en.UTF-8"));
    }

    loot::SetLoggingCallback([&](loot::LogLevel level, const char* message) {
      log(level, message);
    });


    try {
        // ensure the loot directory exists
        fs::path lootAppData = GetLOOTAppData();
        if (lootAppData.empty()) {
            log(loot::LogLevel::error, "failed to create loot app data path");
            return 1;
        }

        if (!fs::exists(lootAppData)) {
            fs::create_directory(lootAppData);
        }

        fs::path profile(m_PluginListPath);
        profile = profile.parent_path();
        std::unique_ptr<loot::GameInterface> gameHandle = CreateGameHandle(
          m_GameId, m_GamePath, profile.string());
        loot::DatabaseInterface& db = gameHandle->GetDatabase();

        m_GameSettings = loot::GameSettings(m_GameId);

        fs::path settings = settingsPath();

        if (fs::exists(settings))
            getSettings(settings);

        m_GameSettings.SetGamePath(m_GamePath);

        if (m_Language != loot::MessageContent::DEFAULT_LANGUAGE) {
          log(loot::LogLevel::debug, "initialising language settings");
          log(loot::LogLevel::debug, "selected language: " + m_Language);

          //Boost.Locale initialisation: Generate and imbue locales.
          boost::locale::generator gen;
          std::locale::global(gen(m_Language + ".UTF-8"));
        }

        bool mlUpdated = false;
        if (m_UpdateMasterlist) {
            progress(Progress::CheckingMasterlistExistence);
            if (!fs::exists(masterlistPath())) {
                fs::create_directories(masterlistPath().parent_path());
            }

            progress(Progress::UpdatingMasterlist);

            //TODO: Removed in 0.18.0
            //mlUpdated = loot::UpdateFile(
            //  masterlistPath().string(),
            //  m_GameSettings.RepoURL(),
            //  m_GameSettings.RepoBranch()
            //);

            //TODO: Removed in 0.18.0
            //if (mlUpdated && !loot::IsLatestFile(masterlistPath().string(), m_GameSettings.RepoBranch())) {
            //    log(loot::LogLevel::error,
            //      "the latest masterlist revision contains a syntax error, "
            //      "LOOT is using the most recent valid revision instead. "
            //      "Syntax errors are usually minor and fixed within hours.");
            //}
        }

        progress(Progress::LoadingLists);

        fs::path userlist = userlistPath();
        db.LoadLists(
          masterlistPath().string(),
          fs::exists(userlist) ? userlistPath().string() : fs::path());

        progress(Progress::ReadingPlugins);
        gameHandle->LoadCurrentLoadOrderState();
        const std::vector<std::string> pluginsList = gameHandle->GetLoadOrder();

        progress(Progress::SortingPlugins);
        std::vector<std::string> sortedPlugins = gameHandle->SortPlugins(pluginsList);

        progress(Progress::WritingLoadorder);

        std::ofstream outf(m_PluginListPath);
        if (!outf) {
            log(loot::LogLevel::error, "failed to open " + m_PluginListPath + " to rewrite it");
            return 1;
        }
        outf << "# This file was automatically generated by Mod Organizer." << std::endl;
        for (const std::string& plugin : sortedPlugins) {
            outf << plugin << std::endl;
        }
        outf.close();

        progress(Progress::ParsingLootMessages);
        std::ofstream(m_OutputPath) << createJsonReport(*gameHandle, sortedPlugins);
    }
    catch(std::system_error& e) {
      //TODO: Removed in 0.18.0
      //if (e.code().category() == loot::libgit2_category()) {
      //  log(loot::LogLevel::error, "A firewall may be blocking LOOT.");
      //}

      log(loot::LogLevel::error, e.what());

      return 1;
    }
    catch (const std::exception & e) {
        log(loot::LogLevel::error, e.what());
        return 1;
    }

    progress(Progress::Done);

    return 0;
}

void set(QJsonObject& o, const char* e, const QJsonValue& v)
{
  if (v.isObject() && v.toObject().isEmpty()) {
    return;
  }

  if (v.isArray() && v.toArray().isEmpty()) {
    return;
  }

  if (v.isString() && v.toString().isEmpty()) {
    return;
  }

  o[e] = v;
}

std::string LOOTWorker::createJsonReport(
  loot::GameInterface& game, const std::vector<std::string>& sortedPlugins) const
{
  QJsonObject root;

  set(root, "messages", createMessages(game.GetDatabase().GetGeneralMessages(true)));
  set(root, "plugins", createPlugins(game, sortedPlugins));

  const auto end = std::chrono::high_resolution_clock::now();

  set(root, "stats", QJsonObject{
    {"time", std::chrono::duration_cast<std::chrono::milliseconds>(end - m_startTime).count()},
    {"lootcliVersion", LOOTCLI_VERSION_STRING},
    {"lootVersion", QString::fromStdString(loot::GetLiblootVersion())}
  });

  QJsonDocument doc(root);
  return doc.toJson(QJsonDocument::Indented).toStdString();
}

template <class Container>
QJsonArray createStringArray(const Container& c)
{
  QJsonArray array;

  for (auto&& e : c) {
    array.push_back(QString::fromStdString(e));
  }

  return array;
}

QJsonArray LOOTWorker::createPlugins(
  loot::GameInterface& game,
  const std::vector<std::string>& sortedPlugins) const
{
  QJsonArray plugins;

  for (auto&& pluginName : sortedPlugins) {

    auto plugin = game.GetPlugin(pluginName);

    QJsonObject o;
    o["name"] = QString::fromStdString(pluginName);

    if (auto metaData=game.GetDatabase().GetPluginMetadata(pluginName, true, true)) {
      set(o, "incompatibilities", createIncompatibilities(game, metaData->GetIncompatibilities()));
      set(o, "messages", createMessages(metaData->GetMessages()));
      set(o, "dirty", createDirty(metaData->GetDirtyInfo()));
      set(o, "clean", createClean(metaData->GetCleanInfo()));
    }

    set(o, "missingMasters", createMissingMasters(game, pluginName));

    if (plugin->LoadsArchive()) {
      o["loadsArchive"] = true;
    }

    if (plugin->IsMaster()) {
      o["isMaster"] = true;
    }

    if (plugin->IsLightPlugin()) {
      o["isLightMaster"] = true;
    }

    // don't add if the name is the only thing in there
    if (o.size() > 1) {
      plugins.push_back(o);
    }
  }

  return plugins;
}

QJsonValue LOOTWorker::createMessages(const std::vector<loot::Message>& list) const
{
  QJsonArray messages;

  for (loot::Message m : list) {
      auto simpleMessage = loot::ToSimpleMessage(m, m_Language);
      if (simpleMessage.has_value()) {
          messages.push_back(QJsonObject{
              {"type", QString::fromStdString(toString(m.GetType()))},
              {"text", QString::fromStdString(simpleMessage.value().text)}
              });
      }
  }

  return messages;
}

QJsonValue LOOTWorker::createDirty(
  const std::vector<loot::PluginCleaningData>& data) const
{
  QJsonArray array;

  for (const auto& d : data) {
    QJsonObject o{
      {"crc", static_cast<qint64>(d.GetCRC())},
      {"itm", static_cast<qint64>(d.GetITMCount())},
      {"deletedReferences", static_cast<qint64>(d.GetDeletedReferenceCount())},
      {"deletedNavmesh", static_cast<qint64>(d.GetDeletedNavmeshCount())},
    };

    set(o, "cleaningUtility", QString::fromStdString(d.GetCleaningUtility()));
    auto simpleMessage = loot::ToSimpleMessage(loot::Message(loot::MessageType::say, d.GetDetail()), m_Language);
    if (simpleMessage.has_value()) {
        set(o, "info", QString::fromStdString(simpleMessage.value().text));
    } else {
        set(o, "info", QString::fromStdString(""));
    }

    array.push_back(o);
  }

  return array;
}

QJsonValue LOOTWorker::createClean(
  const std::vector<loot::PluginCleaningData>& data) const
{
  QJsonArray array;

  for (const auto& d : data) {
    QJsonObject o{
      {"crc", static_cast<qint64>(d.GetCRC())},
    };

    set(o, "cleaningUtility", QString::fromStdString(d.GetCleaningUtility()));
    auto simpleMessage = loot::ToSimpleMessage(loot::Message(loot::MessageType::say, d.GetDetail()), m_Language);
    if (simpleMessage.has_value()) {
        set(o, "info", QString::fromStdString(simpleMessage.value().text));
    }
    else {
        set(o, "info", QString::fromStdString(""));
    }

    array.push_back(o);
  }

  return array;
}


QJsonValue LOOTWorker::createIncompatibilities(
  loot::GameInterface& game, const std::vector<loot::File>& data) const
{
  QJsonArray array;

  for (auto&& f : data) {
    const auto n = static_cast<std::string>(f.GetName());
    if (!game.GetPlugin(n)) {
      continue;
    }

    const auto name = QString::fromStdString(n);
    const auto displayName = QString::fromStdString(f.GetDisplayName());

    QJsonObject o{
      {"name", name}
    };

    if (displayName != name) {
      set(o, "displayName", displayName);
    }

    array.push_back(std::move(o));
  }

  return array;
}

QJsonValue LOOTWorker::createMissingMasters(
  loot::GameInterface& game, const std::string& pluginName) const
{
  QJsonArray array;

  for (auto&& master : game.GetPlugin(pluginName)->GetMasters()) {
    if (!game.GetPlugin(master)) {
      array.push_back(QString::fromStdString(master));
    }
  }

  return array;
}

void LOOTWorker::progress(Progress p)
{
  std::cout << "[progress] " << static_cast<int>(p) << "\n";
  std::cout.flush();
}

std::string escapeNewlines(const std::string& s)
{
  auto ss = boost::replace_all_copy(s, "\n", "\\n");
  boost::replace_all(ss, "\r", "\\r");
  return ss;
}

void LOOTWorker::log(loot::LogLevel level, const std::string& message) const
{
  if (level < m_LogLevel) {
    return;
  }

  const auto ll = fromLootLogLevel(level);
  const auto levelName = logLevelToString(ll);

  std::cout << "[" << levelName << "] " << escapeNewlines(message) << "\n";
  std::cout.flush();
}


loot::LogLevel toLootLogLevel(lootcli::LogLevels level)
{
  using L = loot::LogLevel;
  using LC = lootcli::LogLevels;

  switch (level)
  {
    case LC::Trace:   return L::trace;
    case LC::Debug:   return L::debug;
    case LC::Info:    return L::info;
    case LC::Warning: return L::warning;
    case LC::Error:   return L::error;
    default:          return L::info;
  }
}

lootcli::LogLevels fromLootLogLevel(loot::LogLevel level)
{
  using L = loot::LogLevel;
  using LC = lootcli::LogLevels;

  switch (level)
  {
    case L::trace:
      return LC::Trace;

    case L::debug:
      return LC::Debug;

    case L::info:
      return LC::Info;

    case L::warning:
      return LC::Warning;

    case L::error:  // fall-through
    case L::fatal:
      return LC::Error;

    default:
      return LC::Info;
  }
}

} // namespace
