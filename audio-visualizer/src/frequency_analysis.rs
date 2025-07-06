use std::sync::atomic::Ordering;

use crate::config::AudioVisualizerConfig;

pub trait FrequencyAnalyzer {
    fn compute_bands(
        &self,
        spectrum: &[f32],
        config: &AudioVisualizerConfig,
        freq_resolution: f32,
        min_bin: usize,
        max_bin: usize,
    ) -> Vec<f32>;
}

pub struct LogarithmicAnalyzer;
pub struct LinearAnalyzer;
pub struct BarkAnalyzer;
pub struct MelAnalyzer;
pub struct ThirdOctaveAnalyzer;
pub struct FullOctaveAnalyzer;

impl FrequencyAnalyzer for LogarithmicAnalyzer {
    fn compute_bands(
        &self,
        spectrum: &[f32],
        config: &AudioVisualizerConfig,
        freq_resolution: f32,
        min_bin: usize,
        max_bin: usize,
    ) -> Vec<f32> {
        let mut bands = vec![0.0; config.num_bands];
        // Track which bands were directly calculated vs need interpolation
        let mut calculated_bands = vec![false; config.num_bands];

        // Use logarithmic spacing for perceptually uniform frequency bands
        let log_min_freq = config.min_freq.ln();
        let log_max_freq = config.max_freq.ln();
        let log_freq_range = log_max_freq - log_min_freq;

        // First pass: Calculate bands where possible
        for i in 0..config.num_bands {
            // Calculate logarithmic frequency boundaries for this band
            let band_start_ratio = i as f32 / config.num_bands as f32;
            let band_end_ratio = (i + 1) as f32 / config.num_bands as f32;

            let band_start_log_freq = log_min_freq + band_start_ratio * log_freq_range;
            let band_end_log_freq = log_min_freq + band_end_ratio * log_freq_range;

            let band_start_freq = band_start_log_freq.exp();
            let band_end_freq = band_end_log_freq.exp();

            // Convert frequencies to bin indices
            let start_bin = ((band_start_freq / freq_resolution) as usize).max(min_bin);
            let end_bin = ((band_end_freq / freq_resolution) as usize).min(max_bin);

            if end_bin <= start_bin {
                // Mark this band for interpolation in the second pass
                calculated_bands[i] = false;
            } else {
                let mut band_energy = 0.0;
                for j in start_bin..end_bin {
                    band_energy += spectrum[j];
                }

                // Average the energy across the band
                band_energy /= (end_bin - start_bin) as f32;
                bands[i] = band_energy;
                calculated_bands[i] = true;
            }
        }

        // Second pass: Interpolate missing bands if enabled
        if config.interpolate_bands.load(Ordering::Relaxed) {
            // Find valid bands for interpolation
            let mut prev_valid_idx = None;
            for i in 0..config.num_bands {
                if !calculated_bands[i] {
                    // Find the next valid band
                    let mut next_valid_idx = None;
                    for j in (i + 1)..config.num_bands {
                        if calculated_bands[j] {
                            next_valid_idx = Some(j);
                            break;
                        }
                    }

                    // Interpolate based on available data
                    match (prev_valid_idx, next_valid_idx) {
                        (Some(prev), Some(next)) => {
                            // Interpolate between two valid points
                            let ratio = (i - prev) as f32 / (next - prev) as f32;
                            bands[i] = bands[prev] * (1.0 - ratio) + bands[next] * ratio;
                        }
                        (Some(prev), None) => {
                            // Only previous valid point available, use its value
                            bands[i] = bands[prev] * 0.9; // Slight decay
                        }
                        (None, Some(next)) => {
                            // Only next valid point available, use its value
                            bands[i] = bands[next] * 0.9; // Slight attenuation
                        }
                        (None, None) => {
                            // No valid points, use a small default value
                            bands[i] = 0.1;
                        }
                    }
                } else {
                    // Update prev_valid_idx for the next iteration
                    prev_valid_idx = Some(i);
                }
            }
        }

        bands
    }
}

impl FrequencyAnalyzer for LinearAnalyzer {
    fn compute_bands(
        &self,
        spectrum: &[f32],
        config: &AudioVisualizerConfig,
        _freq_resolution: f32,
        min_bin: usize,
        max_bin: usize,
    ) -> Vec<f32> {
        let mut bands = vec![0.0; config.num_bands];
        let bins_per_band = (max_bin - min_bin) / config.num_bands.max(1);

        for i in 0..config.num_bands {
            let start_bin = min_bin + i * bins_per_band;
            let end_bin = (start_bin + bins_per_band).min(max_bin);

            if end_bin <= start_bin {
                continue;
            }

            let mut band_energy = 0.0;
            for j in start_bin..end_bin {
                band_energy += spectrum[j];
            }

            band_energy /= (end_bin - start_bin) as f32;
            bands[i] = band_energy;
        }

        bands
    }
}

impl FrequencyAnalyzer for BarkAnalyzer {
    fn compute_bands(
        &self,
        spectrum: &[f32],
        config: &AudioVisualizerConfig,
        freq_resolution: f32,
        min_bin: usize,
        max_bin: usize,
    ) -> Vec<f32> {
        let mut bands = vec![0.0; config.num_bands];

        // Implementation based on audio_analyzer.js
        // Bark scale formula: bark = (26.81 * freq) / (1960 + freq) - 0.53
        
        // Function to convert frequency to bark scale (from audio_analyzer.js)
        let hz_to_bark = |freq: f32| -> f32 {
            (26.81 * freq) / (1960.0 + freq) - 0.53
        };
        
        // Function to convert bark scale back to frequency (from audio_analyzer.js)
        let bark_to_hz = |bark: f32| -> f32 {
            1960.0 / (26.81 / (bark + 0.53) - 1.0)
        };
        
        // Calculate the min and max bark values
        let min_bark = hz_to_bark(config.min_freq);
        let max_bark = hz_to_bark(config.max_freq);
        
        // Calculate the bark scale width for each band
        let bark_width = (max_bark - min_bark) / config.num_bands as f32;
        
        // Compute each band
        for i in 0..config.num_bands {
            // Calculate the bark range for this band
            let bark_start = min_bark + bark_width * i as f32;
            let bark_end = min_bark + bark_width * (i + 1) as f32;
            
            // Convert bark values back to frequencies
            let freq_start = bark_to_hz(bark_start);
            let freq_end = bark_to_hz(bark_end);
            
            // Calculate the bin range for this band
            let bin_start = ((freq_start / freq_resolution) as usize).max(min_bin);
            let bin_end = ((freq_end / freq_resolution) as usize).min(max_bin);
            
            // Skip if we don't have a valid bin range
            if bin_end <= bin_start {
                continue;
            }
            
            // Calculate the energy for this band
            let mut band_energy = 0.0;
            for j in bin_start..bin_end {
                band_energy += spectrum[j];
            }
            
            // Normalize by the number of bins
            band_energy /= (bin_end - bin_start) as f32;
            bands[i] = band_energy;
        }

        bands
    }
}

impl FrequencyAnalyzer for MelAnalyzer {
    fn compute_bands(
        &self,
        spectrum: &[f32],
        config: &AudioVisualizerConfig,
        freq_resolution: f32,
        min_bin: usize,
        max_bin: usize,
    ) -> Vec<f32> {
        let mut bands = vec![0.0; config.num_bands];

        // Convert Hz to Mel: mel = 2595 * log10(1 + freq/700)
        let min_mel = 2595.0 * (1.0 + config.min_freq / 700.0).log10();
        let max_mel = 2595.0 * (1.0 + config.max_freq / 700.0).log10();

        for i in 0..config.num_bands {
            let band_ratio = i as f32 / (config.num_bands - 1) as f32;
            let target_mel = min_mel + band_ratio * (max_mel - min_mel);

            // Convert Mel back to Hz: freq = 700 * (10^(mel/2595) - 1)
            let target_freq = 700.0 * (10.0_f32.powf(target_mel / 2595.0) - 1.0);

            let target_bin = (target_freq / freq_resolution) as usize;
            let start_bin = if i == 0 {
                min_bin
            } else {
                target_bin.saturating_sub(3)
            };
            let end_bin = if i == config.num_bands - 1 {
                max_bin
            } else {
                (target_bin + 3).min(max_bin)
            };

            if end_bin <= start_bin {
                continue;
            }

            let mut band_energy = 0.0;
            for j in start_bin..end_bin {
                band_energy += spectrum[j];
            }

            band_energy /= (end_bin - start_bin) as f32;
            bands[i] = band_energy;
        }

        bands
    }
}

impl FrequencyAnalyzer for ThirdOctaveAnalyzer {
    fn compute_bands(
        &self,
        spectrum: &[f32],
        config: &AudioVisualizerConfig,
        freq_resolution: f32,
        _min_bin: usize,
        _max_bin: usize,
    ) -> Vec<f32> {
        let third_octave_centers = generate_third_octave_centers(config);
        let mut bands = vec![0.0; third_octave_centers.len().min(config.num_bands)];

        for (i, &center_freq) in third_octave_centers
            .iter()
            .enumerate()
            .take(config.num_bands)
        {
            if center_freq < config.min_freq || center_freq > config.max_freq {
                continue;
            }

            // 1/3 octave bandwidth
            let lower_freq = center_freq / 2.0_f32.powf(1.0 / 6.0);
            let upper_freq = center_freq * 2.0_f32.powf(1.0 / 6.0);

            let lower_bin = (lower_freq / freq_resolution) as usize;
            let upper_bin = (upper_freq / freq_resolution) as usize;

            if upper_bin <= lower_bin {
                continue;
            }

            let mut band_energy = 0.0;
            for j in lower_bin..upper_bin.min(spectrum.len()) {
                band_energy += spectrum[j];
            }

            band_energy /= (upper_bin - lower_bin) as f32;
            bands[i] = band_energy;
        }

        bands
    }
}

impl FrequencyAnalyzer for FullOctaveAnalyzer {
    fn compute_bands(
        &self,
        spectrum: &[f32],
        config: &AudioVisualizerConfig,
        freq_resolution: f32,
        _min_bin: usize,
        _max_bin: usize,
    ) -> Vec<f32> {
        let octave_centers = generate_octave_centers(config);
        let mut bands = vec![0.0; octave_centers.len().min(config.num_bands)];

        for (i, &center_freq) in octave_centers
            .iter()
            .enumerate()
            .take(config.num_bands)
        {
            if center_freq < config.min_freq || center_freq > config.max_freq {
                continue;
            }

            // Full octave bandwidth
            let lower_freq = center_freq / 2.0_f32.sqrt();
            let upper_freq = center_freq * 2.0_f32.sqrt();

            let lower_bin = (lower_freq / freq_resolution) as usize;
            let upper_bin = (upper_freq / freq_resolution) as usize;

            if upper_bin <= lower_bin {
                continue;
            }

            let mut band_energy = 0.0;
            for j in lower_bin..upper_bin.min(spectrum.len()) {
                band_energy += spectrum[j];
            }

            band_energy /= (upper_bin - lower_bin) as f32;
            bands[i] = band_energy;
        }

        bands
    }
}

fn generate_third_octave_centers(config: &AudioVisualizerConfig) -> Vec<f32> {
    // Standard 1/3 octave center frequencies
    let base_frequencies = [
        31.5, 40.0, 50.0, 63.0, 80.0, 100.0, 125.0, 160.0, 200.0, 250.0, 315.0, 400.0, 500.0,
        630.0, 800.0, 1000.0, 1250.0, 1600.0, 2000.0, 2500.0, 3150.0, 4000.0, 5000.0, 6300.0,
        8000.0, 10000.0, 12500.0, 16000.0, 20000.0,
    ];

    base_frequencies
        .iter()
        .filter(|&&freq| freq >= config.min_freq && freq <= config.max_freq)
        .copied()
        .collect()
}

fn generate_octave_centers(config: &AudioVisualizerConfig) -> Vec<f32> {
    // Standard octave center frequencies
    let base_frequencies = [
        31.5, 63.0, 125.0, 250.0, 500.0, 1000.0, 2000.0, 4000.0, 8000.0, 16000.0,
    ];

    base_frequencies
        .iter()
        .filter(|&&freq| freq >= config.min_freq && freq <= config.max_freq)
        .copied()
        .collect()
}

pub fn get_analyzer(mode: u8, frequency_scale: &str) -> Box<dyn FrequencyAnalyzer> {
    match mode {
        1 => Box::new(ThirdOctaveAnalyzer),
        2 => Box::new(FullOctaveAnalyzer),
        _ => match frequency_scale {
            "linear" => Box::new(LinearAnalyzer),
            "bark" => Box::new(BarkAnalyzer),
            "mel" => Box::new(MelAnalyzer),
            _ => Box::new(LogarithmicAnalyzer),
        },
    }
}
