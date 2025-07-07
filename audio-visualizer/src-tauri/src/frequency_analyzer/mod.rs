use crate::config::AudioVisualizerConfig;

pub mod logarithmic;
pub mod linear;
pub mod bark;
pub mod mel;
pub mod third_octave;
pub mod full_octave;
pub mod utils;

pub use logarithmic::LogarithmicAnalyzer;
pub use linear::LinearAnalyzer;
pub use bark::BarkAnalyzer;
pub use mel::MelAnalyzer;
pub use third_octave::ThirdOctaveAnalyzer;
pub use full_octave::FullOctaveAnalyzer;

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
