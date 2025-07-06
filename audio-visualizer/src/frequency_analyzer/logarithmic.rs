use std::sync::atomic::Ordering;

use crate::config::AudioVisualizerConfig;
use super::FrequencyAnalyzer;

pub struct LogarithmicAnalyzer;

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
