use crate::config::AudioVisualizerConfig;

pub fn generate_third_octave_centers(config: &AudioVisualizerConfig) -> Vec<f32> {
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

pub fn generate_octave_centers(config: &AudioVisualizerConfig) -> Vec<f32> {
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
