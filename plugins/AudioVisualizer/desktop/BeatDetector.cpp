#include "BeatDetector.h"
#include <algorithm>
#include <numeric>
#include <cmath>
#include <iostream>

BeatDetector::BeatDetector() 
    : beat_detected_(false)
    , last_beat_time_(std::chrono::steady_clock::now()) {
}

void BeatDetector::configure(const BeatDetectionConfig& config) {
    std::lock_guard<std::mutex> lock(beat_mutex_);
    config_ = config;
    
    // Clear histories when algorithm changes
    energy_history_.clear();
    flux_history_.clear();
    hfc_history_.clear();
    previous_spectrum_.clear();
}

bool BeatDetector::detect_beat(const std::vector<float>& audio_data) {
    if (audio_data.empty()) {
        return false;
    }
    
    switch (config_.algorithm) {
        case BeatDetectionAlgorithm::Energy:
            return detect_beat_energy(audio_data);
        case BeatDetectionAlgorithm::SpectralFlux:
            return detect_beat_spectral_flux(audio_data);
        case BeatDetectionAlgorithm::HighFrequencyContent:
            return detect_beat_hfc(audio_data);
        case BeatDetectionAlgorithm::ComplexDomain:
            return detect_beat_complex_domain(audio_data);
        default:
            return detect_beat_energy(audio_data);
    }
}

void BeatDetector::reset() {
    std::lock_guard<std::mutex> lock(beat_mutex_);
    energy_history_.clear();
    flux_history_.clear();
    hfc_history_.clear();
    previous_spectrum_.clear();
    beat_detected_ = false;
    last_beat_time_ = std::chrono::steady_clock::now();
}

bool BeatDetector::detect_beat_energy(const std::vector<float>& audio_data) {
    // Calculate current energy
    float current_energy = calculate_energy(audio_data);
    
    // Add to energy history
    energy_history_.push_back(current_energy);
    if (energy_history_.size() > config_.historySize) {
        energy_history_.pop_front();
    }
    
    // Need sufficient history to detect beats
    if (energy_history_.size() < 20) {
        return false;
    }
    
    // Calculate average energy over history
    float avg_energy = std::accumulate(energy_history_.begin(), energy_history_.end(), 0.0f) / energy_history_.size();
    
    // Check for beat: current energy significantly higher than average
    bool is_beat = current_energy > (avg_energy * config_.energyThreshold);
    
    if (is_beat && check_beat_timing()) {
        update_beat_flag(true);
        std::cout << "Beat detected (Energy)! Energy: " << current_energy 
                  << ", Avg: " << avg_energy << ", Ratio: " << current_energy / avg_energy << std::endl;
        return true;
    }
    
    return false;
}

bool BeatDetector::detect_beat_spectral_flux(const std::vector<float>& audio_data) {
    // Calculate spectral flux
    float current_flux = calculate_spectral_flux(audio_data);
    
    // Add to flux history
    flux_history_.push_back(current_flux);
    if (flux_history_.size() > config_.historySize) {
        flux_history_.pop_front();
    }
    
    // Need sufficient history
    if (flux_history_.size() < 20) {
        return false;
    }
    
    // Calculate average flux
    float avg_flux = std::accumulate(flux_history_.begin(), flux_history_.end(), 0.0f) / flux_history_.size();
    
    // Check for beat: current flux significantly higher than average
    bool is_beat = current_flux > (avg_flux + config_.spectralFluxThreshold);
    
    if (is_beat && check_beat_timing()) {
        update_beat_flag(true);
        std::cout << "Beat detected (Spectral Flux)! Flux: " << current_flux 
                  << ", Avg: " << avg_flux << std::endl;
        return true;
    }
    
    return false;
}

bool BeatDetector::detect_beat_hfc(const std::vector<float>& audio_data) {
    // Calculate high frequency content
    float current_hfc = calculate_hfc(audio_data);
    
    // Add to HFC history
    hfc_history_.push_back(current_hfc);
    if (hfc_history_.size() > config_.historySize) {
        hfc_history_.pop_front();
    }
    
    // Need sufficient history
    if (hfc_history_.size() < 20) {
        return false;
    }
    
    // Calculate average HFC
    float avg_hfc = std::accumulate(hfc_history_.begin(), hfc_history_.end(), 0.0f) / hfc_history_.size();
    
    // Check for beat: current HFC significantly higher than average
    bool is_beat = current_hfc > (avg_hfc * config_.hfcThreshold);
    
    if (is_beat && check_beat_timing()) {
        update_beat_flag(true);
        std::cout << "Beat detected (HFC)! HFC: " << current_hfc 
                  << ", Avg: " << avg_hfc << std::endl;
        return true;
    }
    
    return false;
}

bool BeatDetector::detect_beat_complex_domain(const std::vector<float>& audio_data) {
    // Complex domain onset detection combines multiple features
    float energy = calculate_energy(audio_data);
    float flux = calculate_spectral_flux(audio_data);
    float hfc = calculate_hfc(audio_data);
    
    // Add to histories
    energy_history_.push_back(energy);
    flux_history_.push_back(flux);
    hfc_history_.push_back(hfc);
    
    if (energy_history_.size() > config_.historySize) {
        energy_history_.pop_front();
        flux_history_.pop_front();
        hfc_history_.pop_front();
    }
    
    // Need sufficient history
    if (energy_history_.size() < 20) {
        return false;
    }
    
    // Calculate averages
    float avg_energy = std::accumulate(energy_history_.begin(), energy_history_.end(), 0.0f) / energy_history_.size();
    float avg_flux = std::accumulate(flux_history_.begin(), flux_history_.end(), 0.0f) / flux_history_.size();
    float avg_hfc = std::accumulate(hfc_history_.begin(), hfc_history_.end(), 0.0f) / hfc_history_.size();
    
    // Combined detection using weighted features
    bool energy_beat = energy > (avg_energy * config_.energyThreshold);
    bool flux_beat = flux > (avg_flux + config_.spectralFluxThreshold);
    bool hfc_beat = hfc > (avg_hfc * config_.hfcThreshold);
    
    // Beat detected if at least 2 out of 3 features indicate a beat
    int beat_votes = (energy_beat ? 1 : 0) + (flux_beat ? 1 : 0) + (hfc_beat ? 1 : 0);
    bool is_beat = beat_votes >= 2;
    
    if (is_beat && check_beat_timing()) {
        update_beat_flag(true);
        std::cout << "Beat detected (Complex)! Votes: " << beat_votes 
                  << " (E:" << energy/avg_energy << ", F:" << flux-avg_flux 
                  << ", H:" << hfc/avg_hfc << ")" << std::endl;
        return true;
    }
    
    return false;
}

float BeatDetector::calculate_energy(const std::vector<float>& audio_data) {
    float total_energy = 0.0f;
    
    // Focus on lower frequency bands for beat detection (typically where bass is)
    int focus_bands = std::min(static_cast<int>(audio_data.size()), config_.focusBands);
    
    for (int i = 0; i < focus_bands; i++) {
        float normalized = audio_data[i];
        total_energy += normalized * normalized; // Square for energy
    }
    
    return total_energy / focus_bands;
}

float BeatDetector::calculate_spectral_flux(const std::vector<float>& current_spectrum) {
    if (previous_spectrum_.empty()) {
        previous_spectrum_ = current_spectrum;
        return 0.0f;
    }
    
    float flux = 0.0f;
    size_t min_size = std::min(current_spectrum.size(), previous_spectrum_.size());
    
    for (size_t i = 0; i < min_size; i++) {
        float diff = current_spectrum[i] - previous_spectrum_[i];
        if (diff > 0) {
            flux += diff;
        }
    }
    
    // Update previous spectrum
    previous_spectrum_ = current_spectrum;
    
    return flux / min_size;
}

float BeatDetector::calculate_hfc(const std::vector<float>& audio_data) {
    float hfc = 0.0f;
    
    for (size_t i = 1; i < audio_data.size(); i++) {
        hfc += i * audio_data[i] * audio_data[i];
    }
    
    return hfc / audio_data.size();
}

bool BeatDetector::check_beat_timing() {
    auto now = std::chrono::steady_clock::now();
    auto time_since_last_beat = std::chrono::duration_cast<std::chrono::duration<float>>(now - last_beat_time_).count();
    
    if (time_since_last_beat >= config_.minTimeBetweenBeats) {
        last_beat_time_ = now;
        return true;
    }
    
    return false;
}

void BeatDetector::update_beat_flag(bool is_beat) {
    std::lock_guard<std::mutex> lock(beat_mutex_);
    beat_detected_ = is_beat;
}