use crate::config::AudioVisualizerConfig;
use super::FrequencyAnalyzer;

pub struct LinearAnalyzer;

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
