use crate::config::AudioVisualizerConfig;
use super::FrequencyAnalyzer;

pub struct MelAnalyzer;

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
