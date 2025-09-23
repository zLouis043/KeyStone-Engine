#include "audio.h"

#include "../old/raywrap.h"

class SoundAsset : public IAsset {
private:
    Sound sound;
    
public:
    SoundAsset() {
        sound = {0};
    }
    
    ~SoundAsset() {
        if (valid && sound.frameCount > 0) {
            UnloadSound(sound);
        }
    }
    
    bool load_from_file(const std::string& file_path) override {
        this->file_path = file_path;
        
        if (!std::filesystem::exists(file_path)) {
            error_message = "File does not exist: " + file_path;
            valid = false;
            return false;
        }
        
        sound = LoadSound(file_path.c_str());
        
        if (sound.frameCount > 0) {
            valid = true;
            error_message.clear();
            return true;
        } else {
            error_message = "Failed to load sound from file: " + file_path;
            valid = false;
            return false;
        }
    }
    
    bool load_from_data(const void* data, size_t size) override {
        error_message = "Loading sound from memory data not implemented";
        valid = false;
        return false;
    }
    
    bool is_valid() const override {
        return valid && sound.frameCount > 0;
    }
    
    std::string get_error() const override {
        return error_message;
    }
    
    std::string get_path() const override {
        return file_path;
    }
    
    std::string get_asset_type() const override {
        return "SoundAsset";
    }
    
    Sound& get_sound() { return sound; }
    const Sound& get_sound() const { return sound; }
    
    void play() {
        if (is_valid()) {
            PlaySound(sound);
        }
    }
    
    void stop() {
        if (is_valid()) {
            StopSound(sound);
        }
    }
    
    void set_volume(float volume) {
        if (is_valid()) {
            SetSoundVolume(sound, volume);
        }
    }
};

Result<void> AudioSystem::on_configs_load(EngineContext *ctx)
{
    return Result<void>::Ok();
}

Result<void> AudioSystem::on_lua_register(EngineContext *ctx)
{
    auto view = ctx->get_lua().view();

    sol::table sound_tb = view.create_named_table("audio");

    ctx->get_lua().register_type<SoundAsset>(
        "SoundAsset",
        IASSET_BASE,
        "play", &SoundAsset::play,
        "stop", &SoundAsset::stop,
        "set_volume", &SoundAsset::set_volume,
        "get_sound", sol::resolve<Sound&()>(&SoundAsset::get_sound)
    );

    sound_tb.set_function("load_sound_asset", sol::overload(
        [&ctx](const std::string& asset_id, const std::string& path) -> ke::shared_ptr<SoundAsset> {
            auto res = ctx->get_assets_manager().load_asset<SoundAsset>(asset_id, path);
            if(!res.ok()) {
                LOG_INFO("{}", res.what());
                return nullptr;
            }
            return res.value();
        },
        [&ctx](const std::string& path) -> ke::shared_ptr<SoundAsset> {
            auto res = ctx->get_assets_manager().load_asset<SoundAsset>(path);
            if(!res.ok()) {
                LOG_INFO("{}", res.what());
                return nullptr;
            }
            return res.value();
        }
    ));

    sound_tb.set_function("get_sound_asset", [&ctx](const std::string& asset_id) -> ke::shared_ptr<SoundAsset> {
        auto res = ctx->get_assets_manager().get_asset<SoundAsset>(asset_id);
        if(!res.ok()) {
            LOG_INFO("{}", res.what());
            return nullptr;
        }
        return res.value();
    });
    
    
    sound_tb.set_function("unload_sound", [&ctx](const std::string& asset_id) -> bool {
        return ctx->get_assets_manager().unload_asset(asset_id);
    });
}

Result<void> AudioSystem::on_start(EngineContext *ctx)
{
    InitAudioDevice();
    LOG_INFO("Audio Device Successfully initialized!");
    return Result<void>::Ok();
}

Result<void> AudioSystem::on_end(EngineContext *ctx)
{
    CloseAudioDevice();
    LOG_INFO("Audio Device Successfully closed!");
    return Result<void>::Ok();
}

Result<void> AudioSystem::on_begin_loop(EngineContext *ctx)
{
    return Result<void>::Ok();
}

Result<void> AudioSystem::on_end_loop(EngineContext *ctx)
{
    return Result<void>::Ok();
};