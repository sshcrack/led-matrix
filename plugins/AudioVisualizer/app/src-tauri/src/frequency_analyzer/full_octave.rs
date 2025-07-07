use crate::config::AudioVisualizerConfig;
use super::FrequencyAnalyzer;
use super::utils::generate_octave_centers;

pub struct FullOctaveAnalyzer;

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
