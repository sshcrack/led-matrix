use super::FrequencyAnalyzer;
use crate::config::AudioVisualizerConfig;

pub struct BarkAnalyzer;

impl FrequencyAnalyzer for BarkAnalyzer {
    fn compute_bands(
        &self,
        spectrum: &[f32],
        config: &AudioVisualizerConfig,
        freq_resolution: f32,
        min_bin: usize,
        max_bin: usize,
    ) -> Vec<f32> {
        let mut bands = Vec::with_capacity(config.num_bands);

        // Implementation based on audio_analyzer.js
        // Bark scale formula: bark = (26.81 * freq) / (1960 + freq) - 0.53

        // Function to convert frequency to bark scale (from audio_analyzer.js)
        let hz_to_bark = |freq: f32| -> f32 { (26.81 * freq) / (1960.0 + freq) - 0.53 };

        // Function to convert bark scale back to frequency (from audio_analyzer.js)
        let bark_to_hz = |bark: f32| -> f32 { 1960.0 / (26.81 / (bark + 0.53) - 1.0) };

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
                if !config.skip_non_processed {
                    bands.push(0.0); // Use a small default value for skipped bands
                }

                continue;
            }

            // Calculate the energy for this band
            let mut band_energy = 0.0;
            for j in bin_start..bin_end {
                band_energy += spectrum[j];
            }

            // Normalize by the number of bins
            band_energy /= (bin_end - bin_start) as f32;
            bands.push(band_energy);
        }

        bands
    }
}
