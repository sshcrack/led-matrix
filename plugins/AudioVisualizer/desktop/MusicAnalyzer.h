#pragma once

#include "config.h"
#include "record.h"
#include <shared/common/audio_protocol.h>

#include <array>
#include <chrono>
#include <cstdint>
#include <deque>
#include <fftw3.h>
#include <memory>
#include <vector>

class MusicAnalyzer {
public:
    explicit MusicAnalyzer(AudioVisualizerConfig &config);
    ~MusicAnalyzer();

    AudioProtocol::Frame analyze(const AudioRecorder::CapturedAudioFrame &audio,
                                 const std::vector<float> &displayBands);
    void reset();

private:
    static constexpr size_t LongFftSize = MUSIC_ANALYSIS_WINDOW_SIZE;
    static constexpr size_t FeatureBandCount = 7;
    static constexpr size_t WaveformPoints = 64;

    AudioVisualizerConfig &config_;
    std::unique_ptr<fftwf_complex[]> fftInput_;
    std::unique_ptr<fftwf_complex[]> fftOutput_;
    fftwf_plan fftPlan_{};
    std::vector<float> window_;
    std::vector<float> previousNormalizedSpectrum_;

    std::array<float, FeatureBandCount> previousBands_{};
    std::array<float, FeatureBandCount> smoothedBands_{};
    std::array<float, FeatureBandCount> bandPeakDb_{};
    std::array<float, FeatureBandCount> bandFloorDb_{};
    std::array<float, FeatureBandCount> sectionReference_{};

    std::deque<float> loudnessHistoryDb_;
    std::deque<float> fluxHistory_;
    std::deque<double> onsetTimes_;
    std::deque<float> onsetStrengths_;
    std::deque<float> recentTempoEstimates_;
    std::array<float, 241> tempoHistogram_{}; // 60..180 BPM, 0.5 BPM bins.

    float fastLoudness_ = 0.0f;
    float slowLoudness_ = 0.0f;
    float kickEnvelope_ = 0.0f;
    float snareEnvelope_ = 0.0f;
    float hihatEnvelope_ = 0.0f;
    float dropEnvelope_ = 0.0f;
    float sectionEnvelope_ = 0.0f;
    float smoothedBpm_ = 0.0f;
    float beatConfidence_ = 0.0f;
    float tempoStability_ = 0.0f;
    float quietSeconds_ = 0.0f;
    int octaveCorrectionStreak_ = 0;
    int tempoChangeStreak_ = 0;
    bool dropArmed_ = false;

    double lastBeatTime_ = -1000.0;
    double lastOnsetTime_ = -1000.0;
    double lastDropTime_ = -1000.0;
    double lastSectionTime_ = -1000.0;
    uint32_t sequence_ = 0;
    uint64_t beatCounter_ = 0;
    uint64_t onsetCounter_ = 0;
    uint64_t dropCounter_ = 0;
    uint64_t sectionCounter_ = 0;

    // Prefer the capture sample clock over render-loop wall time. The desktop
    // loop may jitter or skip frames while the audio callback remains stable;
    // using sample positions keeps onset intervals and BPM estimation tied to
    // the actual audio timeline.
    uint64_t audioClockOriginSequence_ = 0;
    uint64_t lastAudioSequence_ = 0;
    bool hasAudioClock_ = false;

    std::chrono::steady_clock::time_point startTime_;
    std::chrono::steady_clock::time_point lastAnalyzeTime_;

    std::vector<float> computeLongPowerSpectrum(const std::vector<float> &mono);
    std::array<float, FeatureBandCount> computeRawFeatureBands(
        const std::vector<float> &powerSpectrum, double sampleRate) const;
    void updateTempo(double nowSeconds, float onsetStrength);

    static float percentile(std::deque<float> values, float p);
    static float clamp01(float value);
    static float dbAmplitude(float amplitude);
    static float smooth(float previous, float target, float attackSeconds,
                        float releaseSeconds, float dt);
    static float normalizedFrequency(float hz);
};
