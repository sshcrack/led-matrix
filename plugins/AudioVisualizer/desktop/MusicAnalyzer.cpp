#include "MusicAnalyzer.h"

#include <algorithm>
#include <cmath>
#include <numeric>

namespace {
constexpr float Pi = 3.14159265358979323846f;
constexpr std::array<std::pair<float, float>, 7> FeatureRanges{{
    {20.0f, 60.0f}, {60.0f, 140.0f}, {140.0f, 400.0f},
    {400.0f, 2000.0f}, {2000.0f, 5000.0f}, {5000.0f, 12000.0f},
    {12000.0f, 20000.0f}}};

float median_absolute_deviation(const std::deque<float> &values, float median) {
    if (values.empty()) return 0.0f;
    std::vector<float> deviations;
    deviations.reserve(values.size());
    for (float value : values) deviations.push_back(std::abs(value - median));
    const size_t middle = deviations.size() / 2;
    std::nth_element(deviations.begin(), deviations.begin() + middle, deviations.end());
    return deviations[middle];
}
}

MusicAnalyzer::MusicAnalyzer(AudioVisualizerConfig &config)
    : config_(config), fftInput_(new fftwf_complex[LongFftSize]),
      fftOutput_(new fftwf_complex[LongFftSize]), window_(LongFftSize),
      startTime_(std::chrono::steady_clock::now()), lastAnalyzeTime_(startTime_) {
    for (size_t i = 0; i < LongFftSize; ++i)
        window_[i] = 0.5f - 0.5f * std::cos(2.0f * Pi * static_cast<float>(i) /
                                           static_cast<float>(LongFftSize - 1));
    fftPlan_ = fftwf_plan_dft_1d(static_cast<int>(LongFftSize), fftInput_.get(),
                                 fftOutput_.get(), FFTW_FORWARD, FFTW_MEASURE);
    reset();
}

MusicAnalyzer::~MusicAnalyzer() {
    if (fftPlan_) fftwf_destroy_plan(fftPlan_);
}

void MusicAnalyzer::reset() {
    previousNormalizedSpectrum_.clear();
    previousBands_.fill(0.0f);
    smoothedBands_.fill(0.0f);
    sectionReference_.fill(0.0f);
    bandPeakDb_.fill(-35.0f);
    bandFloorDb_.fill(-90.0f);
    loudnessHistoryDb_.clear();
    fluxHistory_.clear();
    onsetTimes_.clear();
    onsetStrengths_.clear();
    tempoHistogram_.fill(0.0f);
    fastLoudness_ = slowLoudness_ = 0.0f;
    kickEnvelope_ = snareEnvelope_ = hihatEnvelope_ = 0.0f;
    dropEnvelope_ = sectionEnvelope_ = 0.0f;
    smoothedBpm_ = beatConfidence_ = tempoStability_ = 0.0f;
    quietSeconds_ = 0.0f;
    dropArmed_ = false;
    lastBeatTime_ = lastOnsetTime_ = lastDropTime_ = lastSectionTime_ = -1000.0;
    startTime_ = lastAnalyzeTime_ = std::chrono::steady_clock::now();
}

float MusicAnalyzer::clamp01(float value) { return std::clamp(value, 0.0f, 1.0f); }
float MusicAnalyzer::dbAmplitude(float amplitude) {
    return 20.0f * std::log10(std::max(amplitude, 1.0e-9f));
}

float MusicAnalyzer::smooth(float previous, float target, float attackSeconds,
                            float releaseSeconds, float dt) {
    const float time = target > previous ? attackSeconds : releaseSeconds;
    const float amount = 1.0f - std::exp(-dt / std::max(0.001f, time));
    return previous + (target - previous) * amount;
}

float MusicAnalyzer::normalizedFrequency(float hz) {
    constexpr float low = 20.0f;
    constexpr float high = 20000.0f;
    return clamp01(std::log(std::max(hz, low) / low) / std::log(high / low));
}

float MusicAnalyzer::percentile(std::deque<float> values, float p) {
    if (values.empty()) return 0.0f;
    p = std::clamp(p, 0.0f, 1.0f);
    const size_t index = static_cast<size_t>(p * static_cast<float>(values.size() - 1));
    std::nth_element(values.begin(), values.begin() + index, values.end());
    return values[index];
}

std::vector<float> MusicAnalyzer::computeLongPowerSpectrum(const std::vector<float> &mono) {
    if (mono.size() < LongFftSize) return {};
    auto input = mono.end() - static_cast<std::ptrdiff_t>(LongFftSize);
    for (size_t i = 0; i < LongFftSize; ++i, ++input) {
        fftInput_[i][0] = *input * window_[i];
        fftInput_[i][1] = 0.0f;
    }
    fftwf_execute(fftPlan_);

    std::vector<float> power(LongFftSize / 2);
    constexpr float normalization = 4.0f /
        static_cast<float>(LongFftSize * LongFftSize);
    for (size_t i = 0; i < power.size(); ++i) {
        const float real = fftOutput_[i][0];
        const float imag = fftOutput_[i][1];
        power[i] = (real * real + imag * imag) * normalization;
    }
    return power;
}

std::array<float, MusicAnalyzer::FeatureBandCount> MusicAnalyzer::computeRawFeatureBands(
    const std::vector<float> &powerSpectrum, double sampleRate) const {
    std::array<float, FeatureBandCount> result{};
    const float resolution = static_cast<float>(sampleRate) /
                             static_cast<float>(LongFftSize);
    for (size_t band = 0; band < FeatureRanges.size(); ++band) {
        const auto [low, high] = FeatureRanges[band];
        const size_t begin = std::clamp(static_cast<size_t>(low / resolution),
                                        size_t{1}, powerSpectrum.size() - 1);
        const size_t end = std::clamp(static_cast<size_t>(high / resolution) + 1,
                                      begin + 1, powerSpectrum.size());
        float sum = 0.0f;
        for (size_t i = begin; i < end; ++i) sum += powerSpectrum[i];
        result[band] = std::sqrt(sum / static_cast<float>(end - begin));
    }
    return result;
}

void MusicAnalyzer::updateTempo(double nowSeconds, float onsetStrength) {
    const float decay = std::exp(-1.0f / 650.0f);
    for (float &bin : tempoHistogram_) bin *= decay;

    while (!onsetTimes_.empty() && nowSeconds - onsetTimes_.front() > 10.0) {
        onsetTimes_.pop_front();
        onsetStrengths_.pop_front();
    }

    for (size_t i = 0; i < onsetTimes_.size(); ++i) {
        const double interval = nowSeconds - onsetTimes_[i];
        if (interval < 0.24 || interval > 2.0) continue;
        float bpm = static_cast<float>(60.0 / interval);
        while (bpm < 60.0f) bpm *= 2.0f;
        while (bpm > 170.0f) bpm *= 0.5f;
        const float ageWeight = std::exp(-static_cast<float>(interval) * 0.18f);
        const float weight = onsetStrength * onsetStrengths_[i] * ageWeight;
        const int index = std::clamp(static_cast<int>(std::round((bpm - 60.0f) * 2.0f)),
                                     0, static_cast<int>(tempoHistogram_.size() - 1));
        for (int offset = -2; offset <= 2; ++offset) {
            const int target = index + offset;
            if (target >= 0 && target < static_cast<int>(tempoHistogram_.size()))
                tempoHistogram_[target] += weight * std::exp(-0.55f * offset * offset);
        }
    }

    onsetTimes_.push_back(nowSeconds);
    onsetStrengths_.push_back(onsetStrength);

    const auto peakIt = std::max_element(tempoHistogram_.begin(), tempoHistogram_.end());
    const float peak = *peakIt;
    const float total = std::accumulate(tempoHistogram_.begin(), tempoHistogram_.end(), 0.0f);
    if (peak > 0.001f) {
        const size_t index = static_cast<size_t>(std::distance(tempoHistogram_.begin(), peakIt));
        const float bpm = 60.0f + static_cast<float>(index) * 0.5f;
        smoothedBpm_ = smoothedBpm_ <= 1.0f ? bpm : smoothedBpm_ * 0.88f + bpm * 0.12f;
        beatConfidence_ = clamp01((peak / std::max(total, 0.001f)) * 6.0f);

        float weightedVariance = 0.0f;
        float localWeight = 0.0f;
        for (size_t i = 0; i < tempoHistogram_.size(); ++i) {
            const float candidate = 60.0f + static_cast<float>(i) * 0.5f;
            const float distance = candidate - bpm;
            const float weight = tempoHistogram_[i] * std::exp(-distance * distance / 72.0f);
            weightedVariance += weight * distance * distance;
            localWeight += weight;
        }
        tempoStability_ = clamp01(1.0f - std::sqrt(weightedVariance /
            std::max(localWeight, 0.001f)) / 12.0f);
    }
}

AudioProtocol::Frame MusicAnalyzer::analyze(
    const AudioRecorder::CapturedAudioFrame &audio,
    const std::vector<float> &displayBands) {
    AudioProtocol::Frame frame;
    if (audio.mono.size() < LongFftSize) return frame;

    const auto now = std::chrono::steady_clock::now();
    const float dt = std::clamp(std::chrono::duration<float>(now - lastAnalyzeTime_).count(),
                                0.001f, 0.10f);
    const double nowSeconds = std::chrono::duration<double>(now - startTime_).count();
    lastAnalyzeTime_ = now;

    frame.sequence = ++sequence_;
    frame.timestamp_ms = static_cast<uint32_t>(nowSeconds * 1000.0);
    frame.spectrum = displayBands;
    for (float &value : frame.spectrum) value = clamp01(value);

    float sumSquares = 0.0f;
    float peak = 0.0f;
    for (float sample : audio.mono) {
        sumSquares += sample * sample;
        peak = std::max(peak, std::abs(sample));
    }
    const float rms = std::sqrt(sumSquares / static_cast<float>(audio.mono.size()));
    const float loudnessDb = dbAmplitude(rms * static_cast<float>(config_.musicAnalysisGain));
    loudnessHistoryDb_.push_back(loudnessDb);
    while (loudnessHistoryDb_.size() > 900) loudnessHistoryDb_.pop_front();
    const float floorDb = percentile(loudnessHistoryDb_, 0.12f);
    const float ceilingDb = std::max(floorDb + 18.0f, percentile(loudnessHistoryDb_, 0.94f));
    const float loudness = clamp01((loudnessDb - floorDb) / (ceilingDb - floorDb));
    fastLoudness_ = smooth(fastLoudness_, loudness, 0.025f, 0.22f, dt);
    slowLoudness_ = smooth(slowLoudness_, loudness, 1.8f, 3.5f, dt);

    const auto power = computeLongPowerSpectrum(audio.mono);
    if (power.empty()) return frame;
    const auto rawBands = computeRawFeatureBands(power, audio.sampleRate);

    std::array<float, FeatureBandCount> bands{};
    for (size_t i = 0; i < FeatureBandCount; ++i) {
        const float rawDb = dbAmplitude(rawBands[i] * static_cast<float>(config_.musicAnalysisGain));
        if (rawDb < bandFloorDb_[i]) bandFloorDb_[i] = rawDb;
        else bandFloorDb_[i] += std::min(dt * 0.55f, rawDb - bandFloorDb_[i]);
        if (rawDb > bandPeakDb_[i]) bandPeakDb_[i] = rawDb;
        else bandPeakDb_[i] -= dt * 2.2f;
        bandPeakDb_[i] = std::max(bandPeakDb_[i], bandFloorDb_[i] + 20.0f);
        const float normalized = clamp01((rawDb - bandFloorDb_[i]) /
                                         (bandPeakDb_[i] - bandFloorDb_[i]));
        smoothedBands_[i] = smooth(smoothedBands_[i], normalized, 0.018f, 0.18f, dt);
        bands[i] = smoothedBands_[i];
    }

    float totalPower = 0.0f;
    float weightedFrequency = 0.0f;
    float cumulative = 0.0f;
    float rolloffHz = 0.0f;
    double logPower = 0.0;
    const float resolution = static_cast<float>(audio.sampleRate) /
                             static_cast<float>(LongFftSize);
    for (size_t i = 1; i < power.size(); ++i) {
        const float value = std::max(power[i], 1.0e-14f);
        const float frequency = static_cast<float>(i) * resolution;
        totalPower += value;
        weightedFrequency += value * frequency;
        logPower += std::log(value);
    }
    const float centroidHz = totalPower > 0.0f ? weightedFrequency / totalPower : 0.0f;
    const float rolloffTarget = totalPower * 0.85f;
    for (size_t i = 1; i < power.size(); ++i) {
        cumulative += power[i];
        if (cumulative >= rolloffTarget) {
            rolloffHz = static_cast<float>(i) * resolution;
            break;
        }
    }
    const float arithmeticMean = totalPower / std::max<size_t>(1, power.size() - 1);
    const float geometricMean = std::exp(static_cast<float>(logPower /
        static_cast<double>(std::max<size_t>(1, power.size() - 1))));
    const float flatness = clamp01(geometricMean / std::max(arithmeticMean, 1.0e-12f));

    std::vector<float> normalizedSpectrum(power.size());
    const float normalization = std::max(totalPower, 1.0e-12f);
    for (size_t i = 0; i < power.size(); ++i)
        normalizedSpectrum[i] = std::sqrt(power[i] / normalization);
    float flux = 0.0f;
    if (previousNormalizedSpectrum_.size() == normalizedSpectrum.size()) {
        for (size_t i = 1; i < normalizedSpectrum.size(); ++i)
            flux += std::max(0.0f, normalizedSpectrum[i] - previousNormalizedSpectrum_[i]);
        flux /= static_cast<float>(normalizedSpectrum.size());
    }
    previousNormalizedSpectrum_ = std::move(normalizedSpectrum);
    fluxHistory_.push_back(flux);
    while (fluxHistory_.size() > 180) fluxHistory_.pop_front();
    const float fluxMedian = percentile(fluxHistory_, 0.50f);
    const float fluxMad = median_absolute_deviation(fluxHistory_, fluxMedian);
    const float fluxThreshold = fluxMedian + (1.8f / std::max(0.25f,
        static_cast<float>(config_.transientSensitivity))) * std::max(fluxMad, 0.00002f);
    const float onsetStrength = clamp01((flux - fluxThreshold) /
                                        std::max(fluxThreshold * 2.0f, 0.00005f));
    const bool onsetEvent = onsetStrength > 0.16f && nowSeconds - lastOnsetTime_ > 0.065;

    const float kickTarget = clamp01(onsetStrength * 1.7f *
        (std::max(0.0f, bands[0] - previousBands_[0]) * 1.6f +
         std::max(0.0f, bands[1] - previousBands_[1]) * 1.2f + bands[1] * 0.35f));
    const float snareTarget = clamp01(onsetStrength * 1.5f *
        (std::max(0.0f, bands[2] - previousBands_[2]) * 0.8f +
         std::max(0.0f, bands[3] - previousBands_[3]) * 1.0f +
         std::max(0.0f, bands[4] - previousBands_[4]) * 0.9f));
    const float hihatTarget = clamp01(onsetStrength * 2.0f *
        (std::max(0.0f, bands[5] - previousBands_[5]) * 1.15f +
         std::max(0.0f, bands[6] - previousBands_[6]) * 1.35f));
    kickEnvelope_ = smooth(kickEnvelope_, kickTarget, 0.006f, 0.16f, dt);
    snareEnvelope_ = smooth(snareEnvelope_, snareTarget, 0.006f, 0.13f, dt);
    hihatEnvelope_ = smooth(hihatEnvelope_, hihatTarget, 0.004f, 0.085f, dt);

    if (onsetEvent) {
        lastOnsetTime_ = nowSeconds;
        ++onsetCounter_;
        frame.flags |= AudioProtocol::OnsetEvent;
        updateTempo(nowSeconds, std::max(onsetStrength, 0.2f));
    }

    const float period = smoothedBpm_ > 1.0f ? 60.0f / smoothedBpm_ : 0.5f;
    const double sinceBeat = nowSeconds - lastBeatTime_;
    const float predictedPhase = period > 0.0f ?
        std::fmod(static_cast<float>(std::max(0.0, sinceBeat)) / period, 1.0f) : 0.0f;
    const bool strongPercussiveOnset = onsetEvent &&
        std::max(kickTarget, snareTarget * 0.72f) >
        0.14f / std::max(0.3f, static_cast<float>(config_.beatSensitivity));
    const bool alignedOnset = strongPercussiveOnset && sinceBeat > period * 0.48 &&
        (sinceBeat < period * 1.35 || beatConfidence_ < 0.35f);
    const bool predictedBeat = beatConfidence_ > 0.72f && tempoStability_ > 0.55f &&
        loudness > 0.08f && sinceBeat >= period * 0.98 && sinceBeat < period * 1.18;
    const bool beatEvent = alignedOnset || predictedBeat;
    const float beatStrength = clamp01(std::max({kickEnvelope_, onsetStrength * 0.72f,
                                                 predictedBeat ? beatConfidence_ * 0.5f : 0.0f}));
    if (beatEvent) {
        lastBeatTime_ = nowSeconds;
        ++beatCounter_;
        frame.flags |= AudioProtocol::BeatEvent;
    }
    if (kickTarget > 0.22f) frame.flags |= AudioProtocol::KickEvent;
    if (snareTarget > 0.20f) frame.flags |= AudioProtocol::SnareEvent;
    if (hihatTarget > 0.16f) frame.flags |= AudioProtocol::HihatEvent;

    const float energyTrend = std::clamp((fastLoudness_ - slowLoudness_) * 2.6f, -1.0f, 1.0f);
    if (loudness < 0.20f && bands[1] < 0.25f) quietSeconds_ += dt;
    else quietSeconds_ = std::max(0.0f, quietSeconds_ - dt * 0.8f);
    if (quietSeconds_ > 0.65f) dropArmed_ = true;
    const bool dropEvent = dropArmed_ && nowSeconds - lastDropTime_ > 2.0 &&
        onsetStrength > 0.28f && bands[1] > 0.62f && fastLoudness_ > 0.62f;
    if (dropEvent) {
        dropArmed_ = false;
        quietSeconds_ = 0.0f;
        lastDropTime_ = nowSeconds;
        ++dropCounter_;
        frame.flags |= AudioProtocol::DropEvent;
        dropEnvelope_ = 1.0f;
    }
    dropEnvelope_ = smooth(dropEnvelope_, 0.0f, 0.01f, 1.25f, dt);

    float sectionDistance = 0.0f;
    for (size_t i = 0; i < FeatureBandCount; ++i) {
        const float difference = bands[i] - sectionReference_[i];
        sectionDistance += difference * difference;
        sectionReference_[i] = smooth(sectionReference_[i], bands[i], 4.0f, 4.0f, dt);
    }
    sectionDistance = std::sqrt(sectionDistance / static_cast<float>(FeatureBandCount));
    const bool sectionEvent = nowSeconds > 5.0 && nowSeconds - lastSectionTime_ > 3.0 &&
        sectionDistance > 0.30f && onsetStrength > 0.12f;
    if (sectionEvent) {
        lastSectionTime_ = nowSeconds;
        ++sectionCounter_;
        frame.flags |= AudioProtocol::SectionEvent;
        sectionEnvelope_ = 1.0f;
    }
    sectionEnvelope_ = smooth(sectionEnvelope_, 0.0f, 0.01f, 1.8f, dt);

    float leftSq = 0.0f, rightSq = 0.0f, midSq = 0.0f, sideSq = 0.0f, cross = 0.0f;
    if (audio.stereo()) {
        for (size_t i = 0; i < audio.left.size(); ++i) {
            const float left = audio.left[i];
            const float right = audio.right[i];
            const float mid = 0.5f * (left + right);
            const float side = 0.5f * (left - right);
            leftSq += left * left; rightSq += right * right;
            midSq += mid * mid; sideSq += side * side; cross += left * right;
        }
    }
    const float leftRms = std::sqrt(leftSq / std::max<size_t>(1, audio.left.size()));
    const float rightRms = std::sqrt(rightSq / std::max<size_t>(1, audio.right.size()));
    const float midRms = std::sqrt(midSq / std::max<size_t>(1, audio.left.size()));
    const float sideRms = std::sqrt(sideSq / std::max<size_t>(1, audio.left.size()));
    const float stereoWidth = clamp01(sideRms / std::max(midRms + sideRms, 1.0e-7f) * 1.7f);
    const float stereoBalance = std::clamp((rightRms - leftRms) /
        std::max(leftRms + rightRms, 1.0e-7f), -1.0f, 1.0f);
    const float stereoCorrelation = std::clamp(cross /
        std::sqrt(std::max(leftSq * rightSq, 1.0e-12f)), -1.0f, 1.0f);

    const bool silence = rms < 0.00035f;
    if (silence) frame.flags |= AudioProtocol::Silent;

    frame.beat_counter = beatCounter_;
    frame.onset_counter = onsetCounter_;
    frame.drop_counter = dropCounter_;
    frame.section_counter = sectionCounter_;

    frame.set(AudioProtocol::Feature::Rms, clamp01(rms * 8.0f));
    frame.set(AudioProtocol::Feature::Peak, clamp01(peak));
    frame.set(AudioProtocol::Feature::Loudness, loudness);
    frame.set(AudioProtocol::Feature::LoudnessFast, fastLoudness_);
    frame.set(AudioProtocol::Feature::LoudnessSlow, slowLoudness_);
    frame.set(AudioProtocol::Feature::SubBass, bands[0]);
    frame.set(AudioProtocol::Feature::Bass, bands[1]);
    frame.set(AudioProtocol::Feature::LowMid, bands[2]);
    frame.set(AudioProtocol::Feature::Mid, bands[3]);
    frame.set(AudioProtocol::Feature::HighMid, bands[4]);
    frame.set(AudioProtocol::Feature::Treble, bands[5]);
    frame.set(AudioProtocol::Feature::Air, bands[6]);
    frame.set(AudioProtocol::Feature::SpectralCentroid, normalizedFrequency(centroidHz));
    frame.set(AudioProtocol::Feature::SpectralRolloff, normalizedFrequency(rolloffHz));
    frame.set(AudioProtocol::Feature::SpectralFlatness, flatness);
    frame.set(AudioProtocol::Feature::SpectralFlux, clamp01(flux / std::max(fluxThreshold * 3.0f, 0.00005f)));
    frame.set(AudioProtocol::Feature::OnsetStrength, onsetStrength);
    frame.set(AudioProtocol::Feature::Kick, kickEnvelope_);
    frame.set(AudioProtocol::Feature::Snare, snareEnvelope_);
    frame.set(AudioProtocol::Feature::Hihat, hihatEnvelope_);
    frame.set(AudioProtocol::Feature::StereoWidth, stereoWidth);
    frame.set(AudioProtocol::Feature::StereoBalance, stereoBalance);
    frame.set(AudioProtocol::Feature::StereoCorrelation, stereoCorrelation);
    frame.set(AudioProtocol::Feature::EnergyTrend, energyTrend);
    frame.set(AudioProtocol::Feature::SectionChange, sectionEnvelope_);
    frame.set(AudioProtocol::Feature::Drop, dropEnvelope_);
    frame.set(AudioProtocol::Feature::Bpm, smoothedBpm_);
    frame.set(AudioProtocol::Feature::BeatPhase,
              lastBeatTime_ > -100.0 && period > 0.0f ?
              std::fmod(static_cast<float>(nowSeconds - lastBeatTime_) / period, 1.0f) : predictedPhase);
    frame.set(AudioProtocol::Feature::BeatConfidence, beatConfidence_);
    frame.set(AudioProtocol::Feature::BeatStrength, beatStrength);
    frame.set(AudioProtocol::Feature::TempoStability, tempoStability_);
    frame.set(AudioProtocol::Feature::Silence, silence ? 1.0f : 0.0f);

    frame.waveform.resize(WaveformPoints);
    const size_t block = audio.mono.size() / WaveformPoints;
    for (size_t i = 0; i < WaveformPoints; ++i) {
        const size_t begin = i * block;
        const size_t end = i + 1 == WaveformPoints ? audio.mono.size() : (i + 1) * block;
        float selected = 0.0f;
        for (size_t sample = begin; sample < end; ++sample)
            if (std::abs(audio.mono[sample]) > std::abs(selected)) selected = audio.mono[sample];
        frame.waveform[i] = std::clamp(selected * static_cast<float>(config_.waveformGain), -1.0f, 1.0f);
    }

    previousBands_ = bands;
    return frame;
}
