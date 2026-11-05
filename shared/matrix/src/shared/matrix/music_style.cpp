#include <shared/matrix/input_ids.h>
#include <shared/matrix/music_style.h>

#include <algorithm>
#include <cctype>
#include <cmath>

namespace MusicStyle {
namespace {
constexpr std::array<std::string_view, StyleCount> Names{"chill", "edm", "party", "rock", "hiphop", "acoustic"};

struct Keyword {
    std::string_view word;
    Style style;
    float weight;
};

// Substring matches against lower-cased genre names. Deezer genre names may be
// localised, so a few common non-English spellings are included.
constexpr Keyword Keywords[] = {
    // electronic
    {"electro", Style::Edm, 1.0f}, {"edm", Style::Edm, 1.0f}, {"house", Style::Edm, 0.9f}, {"techno", Style::Edm, 1.0f},
    {"trance", Style::Edm, 1.0f}, {"dubstep", Style::Edm, 1.0f}, {"drum and bass", Style::Edm, 1.0f}, {"drum & bass", Style::Edm, 1.0f},
    {"dnb", Style::Edm, 1.0f}, {"hardstyle", Style::Edm, 1.0f}, {"electronic", Style::Edm, 0.8f}, {"bass music", Style::Edm, 0.9f},
    {"future bass", Style::Edm, 1.0f}, {"big room", Style::Edm, 1.0f}, {"breakbeat", Style::Edm, 0.9f}, {"synthwave", Style::Edm, 0.7f},
    // party / pop
    {"dance", Style::Party, 1.0f}, {"pop", Style::Party, 0.8f}, {"disco", Style::Party, 1.0f}, {"latin", Style::Party, 0.9f},
    {"reggaeton", Style::Party, 1.0f}, {"funk", Style::Party, 0.8f}, {"schlager", Style::Party, 1.0f}, {"k-pop", Style::Party, 1.0f},
    {"eurodance", Style::Party, 1.0f}, {"afrobeat", Style::Party, 0.8f}, {"neue deutsche welle", Style::Party, 0.8f},
    {"new wave", Style::Party, 0.7f}, {"italo", Style::Party, 0.9f}, {"ndw", Style::Party, 0.8f},
    {"hands up", Style::Party, 0.8f}, {"hands up", Style::Edm, 0.7f}, {"deutschpop", Style::Party, 0.8f},
    // rock / metal
    {"rock", Style::Rock, 1.0f}, {"metal", Style::Rock, 1.0f}, {"punk", Style::Rock, 1.0f}, {"grunge", Style::Rock, 1.0f},
    {"alternative", Style::Rock, 0.7f}, {"emo", Style::Rock, 0.8f}, {"hardcore", Style::Rock, 0.8f}, {"indie", Style::Rock, 0.35f}, {"indie", Style::Party, 0.4f},
    // hip hop
    {"hip hop", Style::HipHop, 1.0f}, {"hip-hop", Style::HipHop, 1.0f}, {"rap", Style::HipHop, 1.0f}, {"trap", Style::HipHop, 0.9f},
    {"r&b", Style::HipHop, 0.8f}, {"rnb", Style::HipHop, 0.8f}, {"grime", Style::HipHop, 1.0f}, {"drill", Style::HipHop, 1.0f},
    // chill
    {"chill", Style::Chill, 1.0f}, {"lofi", Style::Chill, 1.0f}, {"lo-fi", Style::Chill, 1.0f}, {"ambient", Style::Chill, 1.0f},
    {"downtempo", Style::Chill, 1.0f}, {"lounge", Style::Chill, 1.0f}, {"new age", Style::Chill, 1.0f}, {"sleep", Style::Chill, 1.0f},
    {"meditation", Style::Chill, 1.0f}, {"relax", Style::Chill, 1.0f}, {"trip hop", Style::Chill, 0.8f},
    // acoustic / classical
    {"classical", Style::Acoustic, 1.0f}, {"klassik", Style::Acoustic, 1.0f}, {"classique", Style::Acoustic, 1.0f},
    {"jazz", Style::Acoustic, 1.0f}, {"folk", Style::Acoustic, 1.0f}, {"acoustic", Style::Acoustic, 1.0f},
    {"singer-songwriter", Style::Acoustic, 0.9f}, {"songwriter", Style::Acoustic, 0.9f}, {"blues", Style::Acoustic, 0.9f}, {"country", Style::Acoustic, 0.8f},
    {"soundtrack", Style::Acoustic, 0.6f}, {"film", Style::Acoustic, 0.5f}, {"piano", Style::Acoustic, 0.9f}, {"soul", Style::Acoustic, 0.6f},
    {"opera", Style::Acoustic, 1.0f}, {"orchestral", Style::Acoustic, 1.0f},
};

Weights normalised(Weights w)
{
    float sum = 0.0f;
    for (float v : w) sum += std::max(0.0f, v);
    if (sum <= 1e-6f)
        return Weights{};
    for (float& v : w) v = std::max(0.0f, v) / sum;
    return w;
}

float in_range(float x, float lo, float hi, float soft = 6.0f)
{
    if (x >= lo && x <= hi)
        return 1.0f;
    const float d = x < lo ? lo - x : x - hi;
    return std::clamp(1.0f - d / soft, 0.0f, 1.0f);
}

float ema(float prev, float value, float dt, float tau) { return prev + (value - prev) * (1.0f - std::exp(-dt / tau)); }

std::string lower(std::string_view s)
{
    std::string out(s);
    std::transform(out.begin(), out.end(), out.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return out;
}
}  // namespace

std::string_view name(Style style) { return Names[static_cast<size_t>(style)]; }
std::string tag(Style style) { return "style:" + std::string(name(style)); }
std::string audio_field(Style style) { return "style_" + std::string(name(style)); }

std::vector<std::string> split_genres(std::string_view list)
{
    std::vector<std::string> out;
    size_t start = 0;
    while (start <= list.size()) {
        const size_t end = std::min(list.find(',', start), list.size());
        std::string item(list.substr(start, end - start));
        item.erase(0, item.find_first_not_of(" \t"));
        item.erase(item.find_last_not_of(" \t") + 1);
        if (!item.empty())
            out.push_back(std::move(item));
        start = end + 1;
    }
    return out;
}

Weights from_genres(const std::vector<std::string>& genres)
{
    Weights w{};
    for (const auto& genre : genres) {
        const auto g = lower(genre);
        Weights local{};
        for (const auto& k : Keywords)
            if (g.find(k.word) != std::string::npos)
                local[static_cast<size_t>(k.style)] = std::max(local[static_cast<size_t>(k.style)], k.weight);
        // "pop rock" counts for both, "electro pop" leans electronic etc.
        for (int i = 0; i < StyleCount; ++i) w[static_cast<size_t>(i)] += local[static_cast<size_t>(i)];
    }
    return normalised(w);
}

void AudioEstimator::reset() { *this = AudioEstimator{}; }

void AudioEstimator::update(const AudioProtocol::Frame& f, float dt)
{
    using F = AudioProtocol::Feature;
    dt = std::clamp(dt, 0.0f, 0.25f);
    if (f.event(AudioProtocol::Silent) || f.feature(F::Silence) > 0.7f || dt <= 0.0f) {
        if (f.onset_counter > 0)
            lastOnset_ = f.onset_counter;
        return;
    }
    if (!primed_) {
        lastOnset_ = f.onset_counter;
        primed_ = true;
        return;
    }
    constexpr float Tau = 40.0f;  // track character, not the current bar
    seconds_ += dt;
    const float onsets = f.onset_counter >= lastOnset_ ? static_cast<float>(f.onset_counter - lastOnset_) : 0.0f;
    lastOnset_ = f.onset_counter;
    onsetRate_ = ema(onsetRate_, onsets / dt, dt, Tau);
    const float loudSlow = std::clamp(f.feature(F::LoudnessSlow), 0.0f, 1.0f);
    loud_ = ema(loud_, loudSlow, dt, Tau);
    dynamics_ = ema(dynamics_, std::fabs(std::clamp(f.feature(F::LoudnessFast), 0.0f, 1.0f) - loud_), dt, Tau);
    kick_ = ema(kick_, std::clamp(f.feature(F::Kick), 0.0f, 1.0f), dt, Tau);
    snare_ = ema(snare_, std::clamp(f.feature(F::Snare), 0.0f, 1.0f), dt, Tau);
    hihat_ = ema(hihat_, std::clamp(f.feature(F::Hihat), 0.0f, 1.0f), dt, Tau);
    centroid_ = ema(centroid_, std::clamp(f.feature(F::SpectralCentroid), 0.0f, 1.0f), dt, Tau);
    flatness_ = ema(flatness_, std::clamp(f.feature(F::SpectralFlatness), 0.0f, 1.0f), dt, Tau);
    const float confidence = std::clamp(f.feature(F::BeatConfidence), 0.0f, 1.0f);
    steady_ = ema(steady_, confidence * std::clamp(f.feature(F::TempoStability), 0.0f, 1.0f), dt, Tau);
    const float bpm = f.feature(F::Bpm);
    if (confidence > 0.15f && bpm > 50.0f && bpm < 220.0f)
        bpm_ = ema(bpm_, bpm, dt, Tau * 0.5f);
    if (!f.spectrum.empty()) {
        const size_t quarter = std::max<size_t>(1, f.spectrum.size() / 4);
        float low = 0.0f, all = 1e-4f;
        for (size_t i = 0; i < f.spectrum.size(); ++i) {
            all += f.spectrum[i];
            if (i < quarter)
                low += f.spectrum[i];
        }
        lowShare_ = ema(lowShare_, std::clamp(low / all * 2.0f, 0.0f, 1.0f), dt, Tau);
    }
}

Weights AudioEstimator::weights() const
{
    // Overall drive: loudness, percussive density and kick presence.
    const float energy = std::clamp(0.40f * loud_ + 0.30f * std::min(1.0f, onsetRate_ / 4.0f) + 0.30f * std::min(1.0f, kick_ * 2.5f), 0.0f, 1.0f);
    const float calm = 1.0f - energy;
    const float fourOnFloor = std::min(1.0f, kick_ * 2.5f) * steady_;
    Weights w{};
    w[static_cast<size_t>(Style::Chill)] = calm * calm * 1.4f + in_range(bpm_, 60.0f, 95.0f, 10.0f) * 0.2f * calm;
    w[static_cast<size_t>(Style::Acoustic)] =
        calm * (0.5f * (1.0f - std::min(1.0f, kick_ * 3.0f)) + 0.4f * std::min(1.0f, dynamics_ * 5.0f) + 0.2f * (1.0f - flatness_));
    w[static_cast<size_t>(Style::Edm)] =
        energy * (0.9f * fourOnFloor + 0.5f * in_range(bpm_, 118.0f, 152.0f) + 0.4f * lowShare_ + 0.2f * (1.0f - dynamics_ * 4.0f));
    w[static_cast<size_t>(Style::Party)] = energy * (0.6f * in_range(bpm_, 95.0f, 128.0f) + 0.6f * fourOnFloor + 0.4f * hihat_);
    w[static_cast<size_t>(Style::Rock)] =
        energy * (1.2f * std::min(1.0f, snare_ * 2.5f) + 0.7f * flatness_ + 0.3f * centroid_ + 0.3f * (1.0f - steady_));
    w[static_cast<size_t>(Style::HipHop)] =
        energy * (0.6f * std::max(in_range(bpm_, 70.0f, 100.0f), in_range(bpm_, 136.0f, 165.0f) * 0.7f) + 0.5f * lowShare_ +
                  0.4f * std::min(1.0f, hihat_ * 2.0f) - 0.4f * fourOnFloor);
    // Sharpen: the estimate is a soft vote, not a classifier.
    for (float& v : w) v = std::pow(std::max(0.0f, v), 1.6f);
    return normalised(w);
}

float AudioEstimator::confidence() const { return std::clamp(seconds_ / 30.0f, 0.0f, 1.0f) * 0.45f; }

Info describe(const RuntimeInputs::Snapshot& snapshot)
{
    Info info;
    Weights audio{};
    float audioConfidence = 0.0f;
    if (snapshot.available(RuntimeInputIds::Audio)) {
        audioConfidence = static_cast<float>(snapshot.number(RuntimeInputIds::Audio, AudioConfidenceField).value_or(0.0));
        for (int i = 0; i < StyleCount; ++i)
            audio[static_cast<size_t>(i)] =
                static_cast<float>(snapshot.number(RuntimeInputIds::Audio, audio_field(static_cast<Style>(i))).value_or(0.0));
        audio = normalised(audio);
    }

    Weights meta{};
    bool hasMeta = false;
    if (snapshot.available(RuntimeInputIds::MusicGenre)) {
        const auto genreTrack = snapshot.text(RuntimeInputIds::MusicGenre, "track_id").value_or("");
        const auto playingTrack = snapshot.text(RuntimeInputIds::SpotifyPlayback, "track_id");
        // Metadata describes one track; ignore it once a different one plays.
        if (!playingTrack.has_value() || genreTrack.empty() || *playingTrack == genreTrack) {
            info.genres = snapshot.text(RuntimeInputIds::MusicGenre, "genres").value_or("");
            meta = from_genres(split_genres(info.genres));
            float sum = 0.0f;
            for (float v : meta) sum += v;
            hasMeta = sum > 0.0f;
            if (hasMeta)
                info.source = "genre:" + snapshot.text(RuntimeInputIds::MusicGenre, "source").value_or("lookup");
        }
    }

    if (hasMeta) {
        const float a = audioConfidence > 0.0f ? 0.25f : 0.0f;
        for (int i = 0; i < StyleCount; ++i)
            info.weights[static_cast<size_t>(i)] = meta[static_cast<size_t>(i)] * (1.0f - a) + audio[static_cast<size_t>(i)] * a;
        info.confidence = 0.9f;
    }
    else if (audioConfidence > 0.0f) {
        info.weights = audio;
        info.confidence = audioConfidence;
        info.source = "audio";
    }
    info.weights = normalised(info.weights);
    int best = 0;
    for (int i = 1; i < StyleCount; ++i)
        if (info.weights[static_cast<size_t>(i)] > info.weights[static_cast<size_t>(best)])
            best = i;
    info.dominant = static_cast<Style>(best);
    return info;
}

float style_fit(const Info& info, const std::vector<std::string>& tags, float untagged)
{
    bool anyStyleTag = false;
    float fit = 0.0f;
    for (const auto& t : tags) {
        if (t.rfind("style:", 0) != 0)
            continue;
        anyStyleTag = true;
        for (int i = 0; i < StyleCount; ++i)
            if (t.compare(6, std::string::npos, Names[static_cast<size_t>(i)]) == 0)
                fit += info.weights[static_cast<size_t>(i)];
    }
    if (!anyStyleTag)
        return untagged;
    return std::clamp(fit, 0.0f, 1.0f);
}

}  // namespace MusicStyle
