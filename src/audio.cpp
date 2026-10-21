#define MINIAUDIO_IMPLEMENTATION
#define MA_NO_ENCODING
#define MA_NO_GENERATION
#include "miniaudio.h"

#include "audio.hpp"

#include <filesystem>
#include <map>

namespace chairs {

struct Audio::Impl {
    ma_engine engine{};
    bool engineUp = false;
    std::string dir;
    std::map<std::string, std::unique_ptr<ma_sound>> cached;
    std::map<std::string, std::unique_ptr<ma_sound>> tracks;

    ~Impl() {
        for (auto& [_, s] : tracks) ma_sound_uninit(s.get());
        for (auto& [_, s] : cached) ma_sound_uninit(s.get());
        if (engineUp) ma_engine_uninit(&engine);
    }

    std::string path(const std::string& name) const { return dir + "/" + name + ".wav"; }

    ma_sound* track(const std::string& name) {
        auto& s = tracks[name];
        if (!s) {
            s = std::make_unique<ma_sound>();
            if (ma_sound_init_from_file(&engine, path(name).c_str(), MA_SOUND_FLAG_DECODE | MA_SOUND_FLAG_NO_SPATIALIZATION, nullptr,
                                        nullptr, s.get()) != MA_SUCCESS) {
                s.reset();
                tracks.erase(name);
                return nullptr;
            }
            ma_sound_set_looping(s.get(), MA_TRUE);
        }
        return s.get();
    }
};

Audio::Audio() = default;

Audio::~Audio() = default;

bool Audio::init(const std::string& dir, std::string& error) {
    if (!std::filesystem::exists(dir + "/cancan-1.wav")) {
        error = "no sounds in " + dir + "; run: uv run --project ../supertune python tools/soundbank.py";
        return false;
    }
    impl_ = std::make_unique<Impl>();
    impl_->dir = dir;
    if (ma_engine_init(nullptr, &impl_->engine) != MA_SUCCESS) {
        error = "no audio device";
        impl_.reset();
        return false;
    }
    impl_->engineUp = true;
    ma_engine_set_volume(&impl_->engine, 0.85f);
    for (const auto& entry : std::filesystem::directory_iterator(dir)) {
        const std::string name = entry.path().stem().string();
        if (entry.path().extension() != ".wav" || name.rfind("cancan", 0) == 0 || name.rfind("hall", 0) == 0) continue;
        auto s = std::make_unique<ma_sound>();
        if (ma_sound_init_from_file(&impl_->engine, entry.path().string().c_str(), MA_SOUND_FLAG_DECODE, nullptr, nullptr, s.get()) ==
            MA_SUCCESS)
            impl_->cached[name] = std::move(s);
    }
    ready_ = true;
    return true;
}

void Audio::play(const std::string& name) {
    if (ready_ && impl_->cached.count(name)) ma_engine_play_sound(&impl_->engine, impl_->path(name).c_str(), nullptr);
}

void Audio::event(const Event& e, const Game& game) {
    if (!ready_) return;
    const std::string voice = std::to_string(e.a >= 0 ? static_cast<int>(static_cast<unsigned>(e.a * 5 + 3) % 8) : 0);
    lastShove_.resize(game.beans.size(), -1.0f);
    switch (e.kind) {
        case Event::MusicStart:
            if (game.round > 1) play("pop");
            break;
        case Event::Shove:
            if (e.a >= 0 && game.time - lastShove_[static_cast<size_t>(e.a)] > 0.25f) {
                lastShove_[static_cast<size_t>(e.a)] = game.time;
                play("whoosh-" + std::to_string(e.a % 4));
            }
            break;
        case Event::Hit: play("bonk-" + voice); break;
        case Event::Sit: play("plop-" + std::to_string(std::max(e.a, 0) % 4)); break;
        case Event::KnockedOff: play("knock-" + voice); break;
        case Event::Locked: play("ding"); break;
        case Event::Foul: play("whistle"); break;
        case Event::Eliminated: play("launch-" + voice); break;
        case Event::Winner: play("fanfare"); break;
        default: break;
    }
}

void Audio::jump(int bean, float time) {
    if (!ready_) return;
    lastJump_.resize(static_cast<size_t>(std::max(bean + 1, static_cast<int>(lastJump_.size()))), -1.0f);
    if (time - lastJump_[static_cast<size_t>(bean)] < 0.25f) return;
    lastJump_[static_cast<size_t>(bean)] = time;
    play("boing-" + std::to_string(static_cast<unsigned>(bean * 5 + 3) % 8));
}

void Audio::twinkle() { play("twinkle"); }

void Audio::music(bool playing, const Game& game) {
    if (!ready_) return;
    const std::string want = std::string(game.aliveCount() <= 2 ? "hall-" : "cancan-") + std::to_string(std::clamp(game.round, 1, 12));
    if (want != track_) {
        if (ma_sound* old = track_.empty() ? nullptr : impl_->track(track_)) ma_sound_stop(old);
        track_ = want;
        if (ma_sound* s = impl_->track(track_)) {
            ma_uint64 length = 0;
            ma_sound_get_length_in_pcm_frames(s, &length);
            const auto at = static_cast<ma_uint64>(game.phaseTime * 48000.0f);
            ma_sound_seek_to_pcm_frame(s, length ? at % length : 0);
        }
        playing_ = false;
    }
    if (playing == playing_) return;
    playing_ = playing;
    if (ma_sound* s = impl_->track(track_)) {
        if (playing) ma_sound_start(s);
        else ma_sound_stop(s);
    }
}

}  // namespace chairs
