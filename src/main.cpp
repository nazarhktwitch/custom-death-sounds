#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/FMODAudioEngine.hpp>
#include <filesystem>
#include <fmod.hpp>
#include <random>
#include <unordered_map>

using namespace geode::prelude;

enum class DeathType { SPIKE, BLOCK, OTHER };

static DeathType g_lastDeathType = DeathType::OTHER;
static FMOD::Channel* g_playingChannel = nullptr;

static std::unordered_map<std::string, std::vector<FMOD::Sound*>> g_soundCache;
static std::unordered_map<std::string, std::string> g_soundPaths;

static std::mt19937& rng() {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    return gen;
}

static std::vector<std::filesystem::path> scanAudioFiles(const std::filesystem::path& dir) {
    std::vector<std::filesystem::path> files;
    try {
        for (auto& entry : std::filesystem::directory_iterator(dir)) {
            if (!entry.is_regular_file()) continue;
            auto ext = entry.path().extension().string();
            std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
            if (ext == ".mp3" || ext == ".wav" || ext == ".ogg" || ext == ".flac")
                files.push_back(entry.path());
        }
    } catch (...) {}
    return files;
}

static DeathType getDeathTypeFromObject(GameObject* obj) {
    if (!obj) return DeathType::OTHER;
    auto t = obj->getType();
    if (t == GameObjectType::Hazard || t == GameObjectType::AnimatedHazard)
        return DeathType::SPIKE;
    if (t == GameObjectType::CollisionObject)
        return DeathType::BLOCK;
    return DeathType::OTHER;
}

static std::string settingKeyFor(DeathType type, std::string_view suffix) {
    switch (type) {
        case DeathType::SPIKE: return std::string("spike") + std::string(suffix);
        case DeathType::BLOCK: return std::string("block") + std::string(suffix);
        default: return std::string("other") + std::string(suffix);
    }
}

static std::filesystem::path soundDirFor(DeathType type) {
    auto base = Mod::get()->getConfigDir() / "Sounds";
    switch (type) {
        case DeathType::SPIKE: return base / "Spike";
        case DeathType::BLOCK: return base / "Block";
        default: return base / "Other";
    }
}

static void unloadSound(const std::string& key) {
    auto it = g_soundCache.find(key);
    if (it != g_soundCache.end()) {
        for (auto sound : it->second) {
            if (sound) sound->release();
        }
        g_soundCache.erase(it);
    }
}

static void ensureCachedFromDir(const std::string& key, const std::filesystem::path& dir) {
    auto pathStr = geode::utils::string::pathToString(dir);

    auto pathIt = g_soundPaths.find(key);
    if (pathIt != g_soundPaths.end() && pathIt->second == pathStr) {
        if (g_soundCache.count(key)) return;
    }

    unloadSound(key);
    g_soundPaths[key] = pathStr;

    if (dir.empty() || !std::filesystem::exists(dir)) return;

    auto engine = FMODAudioEngine::sharedEngine();
    if (!engine || !engine->m_system) return;

    auto audioFiles = scanAudioFiles(dir);
    std::vector<FMOD::Sound*> sounds;
    for (auto& file : audioFiles) {
        auto fileStr = geode::utils::string::pathToString(file);
        FMOD::Sound* sound = nullptr;
        if (engine->m_system->createSound(fileStr.c_str(), FMOD_DEFAULT, nullptr, &sound) == FMOD_OK && sound) {
            sounds.push_back(sound);
        }
    }
    if (!sounds.empty()) {
        g_soundCache[key] = std::move(sounds);
    }
}

static void ensureCached(const std::string& key) {
    try {
        auto path = Mod::get()->getSettingValue<std::filesystem::path>(key);
        auto pathStr = geode::utils::string::pathToString(path);

        auto pathIt = g_soundPaths.find(key);
        if (pathIt != g_soundPaths.end() && pathIt->second == pathStr) {
            if (g_soundCache.count(key)) return;
        }

        unloadSound(key);
        g_soundPaths[key] = pathStr;

        if (path.empty() || !std::filesystem::exists(path)) return;

        auto engine = FMODAudioEngine::sharedEngine();
        if (!engine || !engine->m_system) return;

        if (std::filesystem::is_directory(path)) {
            auto audioFiles = scanAudioFiles(path);
            std::vector<FMOD::Sound*> sounds;
            for (auto& file : audioFiles) {
                auto fileStr = geode::utils::string::pathToString(file);
                FMOD::Sound* sound = nullptr;
                if (engine->m_system->createSound(fileStr.c_str(), FMOD_DEFAULT, nullptr, &sound) == FMOD_OK && sound) {
                    sounds.push_back(sound);
                }
            }
            if (!sounds.empty()) {
                g_soundCache[key] = std::move(sounds);
            }
        } else {
            FMOD::Sound* sound = nullptr;
            if (engine->m_system->createSound(pathStr.c_str(), FMOD_DEFAULT, nullptr, &sound) == FMOD_OK && sound) {
                g_soundCache[key] = {sound};
            }
        }
    } catch (...) {}
}

static FMOD::Sound* getCached(const std::string& key) {
    auto it = g_soundCache.find(key);
    if (it == g_soundCache.end() || it->second.empty()) return nullptr;
    auto& sounds = it->second;
    if (sounds.size() == 1) return sounds[0];
    std::uniform_int_distribution<size_t> dist(0, sounds.size() - 1);
    return sounds[dist(rng())];
}

static float getVolumeFromKey(const std::string& key) {
    try { return Mod::get()->getSettingValue<int>(key) / 100.0f; } catch (...) { return 1.0f; }
}

static float getPitchFromType(DeathType type) {
    auto keyMin = settingKeyFor(type, "-pitch-min");
    auto keyMax = settingKeyFor(type, "-pitch-max");
    try {
        float min = Mod::get()->getSettingValue<int>(keyMin) / 100.0f;
        float max = Mod::get()->getSettingValue<int>(keyMax) / 100.0f;
        if (min <= 0.f) min = 1.f;
        if (max <= 0.f) max = 1.f;
        if (min == max) return min;
        std::uniform_real_distribution<float> dist(std::min(min, max), std::max(min, max));
        return dist(rng());
    } catch (...) { return 1.0f; }
}

static bool isCustomSoundsEnabled() {
    try { return Mod::get()->getSettingValue<bool>("enable-custom-sounds"); } catch (...) { return true; }
}

static bool isKeepSoundOnReset() {
    try { return Mod::get()->getSettingValue<bool>("keep-sound-on-reset"); } catch (...) { return false; }
}

static std::string getSoundKey(DeathType type) {
    bool useFolder = false;
    try { useFolder = Mod::get()->getSettingValue<bool>(settingKeyFor(type, "-use-folder")); } catch (...) {}
    if (useFolder) {
        auto dir = soundDirFor(type);
        if (std::filesystem::exists(dir) && !scanAudioFiles(dir).empty()) {
            auto key = std::string("folder-") + settingKeyFor(type, "-sound");
            ensureCachedFromDir(key, dir);
            return key;
        }
    }
    auto fileKey = settingKeyFor(type, "-sound");
    ensureCached(fileKey);
    return fileKey;
}

static std::string getLevelCompleteKey() {
    bool useFolder = false;
    try { useFolder = Mod::get()->getSettingValue<bool>("level-complete-use-folder"); } catch (...) {}
    if (useFolder) {
        auto dir = Mod::get()->getConfigDir() / "Sounds" / "LevelComplete";
        if (std::filesystem::exists(dir) && !scanAudioFiles(dir).empty()) {
            auto key = "folder-level-complete-sound";
            ensureCachedFromDir(key, dir);
            return key;
        }
    }
    ensureCached("level-complete-sound");
    return "level-complete-sound";
}

static void preloadFolderIfEnabled(const std::string& useKey, const std::filesystem::path& dir, const std::string& folderCacheKey) {
    bool use = false;
    try { use = Mod::get()->getSettingValue<bool>(useKey); } catch (...) {}
    if (use && std::filesystem::exists(dir)) {
        ensureCachedFromDir(folderCacheKey, dir);
    }
}

$execute {
    auto soundsDir = Mod::get()->getConfigDir() / "Sounds";
    std::filesystem::create_directories(soundsDir / "Spike");
    std::filesystem::create_directories(soundsDir / "Block");
    std::filesystem::create_directories(soundsDir / "Other");
    std::filesystem::create_directories(soundsDir / "LevelComplete");
    ensureCached("spike-sound");
    ensureCached("block-sound");
    ensureCached("other-sound");
    ensureCached("level-complete-sound");
    preloadFolderIfEnabled("spike-use-folder", soundsDir / "Spike", "folder-spike-sound");
    preloadFolderIfEnabled("block-use-folder", soundsDir / "Block", "folder-block-sound");
    preloadFolderIfEnabled("other-use-folder", soundsDir / "Other", "folder-other-sound");
    preloadFolderIfEnabled("level-complete-use-folder", soundsDir / "LevelComplete", "folder-level-complete-sound");
}

class $modify(PlayLayer) {
    void destroyPlayer(PlayerObject* player, GameObject* object) {
        if (isCustomSoundsEnabled()) {
            g_lastDeathType = object ? getDeathTypeFromObject(object) : DeathType::OTHER;
            getSoundKey(g_lastDeathType);
        }
        PlayLayer::destroyPlayer(player, object);
    }

    void resetLevel() {
        PlayLayer::resetLevel();
        if (!isKeepSoundOnReset() && g_playingChannel) {
            bool isPlaying = false;
            if (g_playingChannel->isPlaying(&isPlaying) == FMOD_OK && isPlaying) {
                g_playingChannel->stop();
            }
            g_playingChannel = nullptr;
        }
    }
};

class $modify(FMODAudioEngine) {
    int playEffect(gd::string path, float speed, float p2, float volume) {
        std::string ps(path.c_str());

        if (!isCustomSoundsEnabled())
            return FMODAudioEngine::playEffect(path, speed, p2, volume);

        if (ps.find("explode_11.ogg") != std::string::npos) {
            auto sound = getCached(getSoundKey(g_lastDeathType));
            if (sound) {
                float vol = getVolumeFromKey(settingKeyFor(g_lastDeathType, "-volume"));
                float pitch = getPitchFromType(g_lastDeathType);
                FMOD::Channel* channel = nullptr;
                if (m_system->playSound(sound, nullptr, false, &channel) == FMOD_OK && channel) {
                    channel->setVolume(vol * getEffectsVolume());
                    channel->setPitch(pitch);
                    g_playingChannel = channel;
                }
                return 1;
            }
        }

        if (ps.find("endStart_02.ogg") != std::string::npos) {
            auto sound = getCached(getLevelCompleteKey());
            if (sound) {
                float vol = getVolumeFromKey("level-complete-volume");
                float pitch = 1.f;
                try { pitch = Mod::get()->getSettingValue<int>("level-complete-pitch") / 100.0f; } catch(...){}
                FMOD::Channel* channel = nullptr;
                if (m_system->playSound(sound, nullptr, false, &channel) == FMOD_OK && channel) {
                    channel->setVolume(vol * getEffectsVolume());
                    channel->setPitch(pitch);
                }
                return 1;
            }
        }

        return FMODAudioEngine::playEffect(path, speed, p2, volume);
    }
};
