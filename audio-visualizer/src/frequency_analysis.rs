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
        // Standard Bark critical band edge frequencies in Hz
        // These are the upper limits of each band from bark 1 to 24
        let bark_band_edges: [f32; 25] = [
            0.0, 100.0, 200.0, 300.0, 400.0, 510.0, 630.0, 770.0, 920.0, 1080.0, 
            1270.0, 1480.0, 1720.0, 2000.0, 2320.0, 2700.0, 3150.0, 3700.0, 4400.0, 
            5300.0, 6400.0, 7700.0, 9500.0, 12000.0, 15500.0
        ];
        
        // Standard Bark critical band centers in Hz
        let bark_band_centers: [f32; 24] = [
            50.0, 150.0, 250.0, 350.0, 450.0, 570.0, 700.0, 840.0, 1000.0, 1170.0,
            1370.0, 1600.0, 1850.0, 2150.0, 2500.0, 2900.0, 3400.0, 4000.0, 4800.0,
            5800.0, 7000.0, 8500.0, 10500.0, 13500.0
        ];
        
        // Determine the number of output bands based on config
        let mut bands = vec![0.0; config.num_bands];
        
        // Check if we have sufficient bands for full Bark scale representation
        let use_mapping = config.num_bands < 24;
        
        if use_mapping {
            // Using mapping approach (for fewer output bands)
            // Convert min/max freq to Bark scale for interpolation
            let hz_to_bark = |freq: f32| -> f32 {
                13.0 * (0.00076 * freq).atan() + 3.5 * ((freq / 7500.0).powi(2)).atan()
            };
            
            let min_bark = hz_to_bark(config.min_freq);
            let max_bark = hz_to_bark(config.max_freq);
            
            for i in 0..config.num_bands {
                let band_ratio = i as f32 / (config.num_bands - 1) as f32;
                let target_bark = min_bark + band_ratio * (max_bark - min_bark);
                
                // Find the corresponding frequency for this Bark value
                // Using more accurate inverse transform
                let target_freq = if target_bark < 1.0 {
                    target_bark * 100.0 // Linear approximation for very low barks
                } else {
                    // Find nearest bark band
                    let bark_index = target_bark as usize;
                    if bark_index < 24 {
                        // Interpolate between band centers
                        let bark_frac = target_bark - bark_index as f32;
                        let freq1 = bark_band_centers[bark_index.min(23)];
                        let freq2 = bark_band_centers[(bark_index + 1).min(23)];
                        freq1 + bark_frac * (freq2 - freq1)
                    } else {
                        // Beyond the defined range, use exponential approximation
                        let z = target_bark;
                        600.0 * (z / 6.0).sinh()
                    }
                };
                
                // Calculate critical bandwidth at this frequency (in Hz)
                // Using formula: bandwidth = 25 + 75 * (1 + 1.4 * (freq/1000)^2)^0.69
                let critical_bandwidth = 25.0 + 75.0 * (1.0 + 1.4 * (target_freq / 1000.0).powi(2)).powf(0.69);
                
                // Convert to bin indices
                let target_bin = (target_freq / freq_resolution) as usize;
                let half_bandwidth_bins = (critical_bandwidth / (2.0 * freq_resolution)) as usize;
                
                let start_bin = if target_bin > half_bandwidth_bins {
                    (target_bin - half_bandwidth_bins).max(min_bin)
                } else {
                    min_bin
                };
                
                let end_bin = (target_bin + half_bandwidth_bins).min(max_bin);
                
                if end_bin <= start_bin {
                    continue;
                }
                
                // Calculate band energy
                let mut band_energy = 0.0;
                for j in start_bin..end_bin {
                    band_energy += spectrum[j];
                }
                
                band_energy /= (end_bin - start_bin) as f32;
                bands[i] = band_energy;
            }
        } else {
            // Direct mapping to critical bands when we have enough output bands
            // We'll map each critical band directly to an output band
            let num_critical_bands = bark_band_centers.len().min(config.num_bands);
            
            for i in 0..num_critical_bands {
                let center_freq = bark_band_centers[i];
                let lower_edge = if i == 0 { 0.0 } else { bark_band_edges[i] };
                let upper_edge = bark_band_edges[i+1];
                
                // Skip bands outside of our frequency range
                if upper_edge < config.min_freq || lower_edge > config.max_freq {
                    continue;
                }
                
                // Convert to bin indices
                let start_bin = (lower_edge / freq_resolution) as usize;
                let end_bin = (upper_edge / freq_resolution) as usize;
                
                // Ensure we're within the valid range
                let start_bin = start_bin.max(min_bin);
                let end_bin = end_bin.min(max_bin);
                
                if end_bin <= start_bin {
                    continue;
                }
                
                // Calculate band energy
                let mut band_energy = 0.0;
                for j in start_bin..end_bin {
                    band_energy += spectrum[j];
                }
                
                band_energy /= (end_bin - start_bin) as f32;
                bands[i] = band_energy;
            }
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
