#pragma once
#include <vector>
#include <deque>
#include <chrono>
#include <mutex>

enum class BeatDetectionAlgorithm {
    Energy,
    SpectralFlux,
    HighFrequencyContent,
    ComplexDomain
};

struct BeatDetectionConfig {
    BeatDetectionAlgorithm algorithm = BeatDetectionAlgorithm::Energy;
    float energyThreshold = 1.5f;           // Energy multiplier for beat detection
    float minTimeBetweenBeats = 0.3f;       // Minimum time between beats in seconds
    int historySize = 43;                   // Number of frames to keep in history
    int focusBands = 16;                    // Number of lower frequency bands to focus on
    float spectralFluxThreshold = 0.02f;    // Threshold for spectral flux algorithm
    float hfcThreshold = 0.5f;              // Threshold for high frequency content algorithm
};

class BeatDetector {
public:
    BeatDetector();
    ~BeatDetector() = default;
    
    void configure(const BeatDetectionConfig& config);
    bool detect_beat(const std::vector<float>& audio_data);
    void reset();
    
private:
    BeatDetectionConfig config_;
    
    // Common state
    std::chrono::steady_clock::time_point last_beat_time_;
    bool beat_detected_;
    std::mutex beat_mutex_;
    
    // Energy-based detection state
    std::deque<float> energy_history_;
    
    // Spectral flux detection state
    std::vector<float> previous_spectrum_;
    std::deque<float> flux_history_;
    
    // High frequency content detection state
    std::deque<float> hfc_history_;
    
    // Algorithm implementations
    bool detect_beat_energy(const std::vector<float>& audio_data);
    bool detect_beat_spectral_flux(const std::vector<float>& audio_data);
    bool detect_beat_hfc(const std::vector<float>& audio_data);
    bool detect_beat_complex_domain(const std::vector<float>& audio_data);
    
    // Helper functions
    float calculate_energy(const std::vector<float>& audio_data);
    float calculate_spectral_flux(const std::vector<float>& current_spectrum);
    float calculate_hfc(const std::vector<float>& audio_data);
    bool check_beat_timing();
    void update_beat_flag(bool is_beat);
};