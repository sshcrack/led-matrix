use crate::{config::AudioVisualizerConfig, frequency_analyzer};
use rustfft::{Fft, FftPlanner};
use std::collections::VecDeque;
use std::sync::Arc;

pub const BUFFER_SIZE: usize = 2048;
pub const FFT_SIZE: usize = 1024;

pub trait AudioProcessorTrait {
    fn process_audio(&mut self, samples: &[f32]);
    fn get_bands(&self) -> Vec<f32>;
    fn update_config(&mut self, config: AudioVisualizerConfig);
}

pub struct AudioProcessor {
    fft: Arc<dyn Fft<f32>>,
    window: Vec<f32>,
    buffer: VecDeque<f32>,
    spectrum: Vec<f32>,
    bands: Vec<f32>,
    config: AudioVisualizerConfig,
    sample_rate: u32,
}

impl AudioProcessor {
    pub fn new(config: AudioVisualizerConfig, sample_rate: u32) -> Self {
        let mut planner = FftPlanner::new();
        let fft = planner.plan_fft_forward(FFT_SIZE);

        // Create Hann window
        let window: Vec<f32> = (0..FFT_SIZE)
            .map(|i| {
                let angle = 2.0 * std::f32::consts::PI * i as f32 / (FFT_SIZE - 1) as f32;
                0.5 * (1.0 - angle.cos())
            })
            .collect();

        Self {
            fft,
            window,
            buffer: VecDeque::new(),
            spectrum: vec![0.0; FFT_SIZE / 2],
            bands: vec![0.0; config.num_bands],
            config,
            sample_rate,
        }
    }

    fn compute_fft(&mut self) {
        let mut input: Vec<rustfft::num_complex::Complex<f32>> = self
            .buffer
            .iter()
            .take(FFT_SIZE)
            .enumerate()
            .map(|(i, &sample)| rustfft::num_complex::Complex::new(sample * self.window[i], 0.0))
            .collect();

        self.fft.process(&mut input);

        // Compute magnitude spectrum with proper normalization
        let normalization_factor = 1.0 / (FFT_SIZE as f32);
        let window_correction = 2.0; // Compensation for Hann window power loss

        for (i, complex) in input.iter().enumerate().take(FFT_SIZE / 2) {
            self.spectrum[i] = complex.norm() * normalization_factor * window_correction;
        }
    }

    fn compute_bands(&mut self) {
        let freq_resolution = self.sample_rate as f32 / FFT_SIZE as f32;

        let min_bin = (self.config.min_freq / freq_resolution) as usize;
        let max_bin =
            ((self.config.max_freq / freq_resolution) as usize).min(self.spectrum.len() - 1);

        // Ensure we have at least some bins to work with
        if max_bin <= min_bin {
            return;
        }

        let freq_scale = self.config.frequency_scale.read().unwrap().clone();
        let analyzer = frequency_analyzer::get_analyzer(self.config.mode, &freq_scale);
        let bands = analyzer.compute_bands(
            &self.spectrum,
            &self.config,
            freq_resolution,
            min_bin,
            max_bin,
        );

        self.apply_amplitude_processing(bands);
    }

    fn apply_amplitude_processing(&mut self, bands: Vec<f32>) {
        for i in 0..bands.len().min(self.config.num_bands) {
            let mut band_energy = bands[i];

            // Apply gain
            band_energy *= self.config.gain;

            // Apply amplitude scaling based on linear_amplitude setting
            let processed_energy = if self.config.linear_amplitude {
                // Linear amplitude: just normalize to 0-1 range
                band_energy.min(1.0)
            } else {
                // Logarithmic amplitude: convert to dB scale
                if band_energy > 0.0 {
                    // Convert to dB scale: 20 * log10(amplitude)
                    // Add a small offset to avoid log(0) and normalize to 0-1 range
                    let db = 20.0 * (band_energy + 1e-10).log10();
                    // Map from typical range (-60dB to 0dB) to 0-1
                    ((db + 60.0) / 60.0).max(0.0).min(1.0)
                } else {
                    0.0
                }
            };

            // Apply smoothing
            self.bands[i] = self.bands[i] * self.config.smoothing
                + processed_energy * (1.0 - self.config.smoothing);
        }
    }

    // Add this new method to get the interpolated_log flag
    pub fn get_interpolated_log(&self) -> bool {
        let interpolated = self
            .config
            .interpolate_bands
            .load(std::sync::atomic::Ordering::Relaxed);
        let frequency_scale = *self.config.frequency_scale.read().unwrap() == "log";
        let result = interpolated && frequency_scale;

        result
    }
}

impl AudioProcessorTrait for AudioProcessor {
    fn process_audio(&mut self, samples: &[f32]) {
        // Add samples to buffer
        for &sample in samples {
            self.buffer.push_back(sample);
            if self.buffer.len() > BUFFER_SIZE {
                self.buffer.pop_front();
            }
        }

        if self.buffer.len() >= FFT_SIZE {
            self.compute_fft();
            self.compute_bands();
        }
    }

    fn get_bands(&self) -> Vec<f32> {
        self.bands.clone()
    }

    fn update_config(&mut self, config: AudioVisualizerConfig) {
        println!("[DEBUG] AudioProcessor.update_config: Updating config");
        println!(
            "[DEBUG] AudioProcessor.update_config: Old num_bands={}, New num_bands={}",
            self.config.num_bands, config.num_bands
        );

        if config.num_bands != self.config.num_bands {
            println!("[DEBUG] AudioProcessor.update_config: Resizing bands array");
            self.bands = vec![0.0; config.num_bands];
        }

        // Log important settings changes
        println!(
            "[DEBUG] AudioProcessor.update_config: Old frequency_scale={}, New frequency_scale={}",
            *self.config.frequency_scale.read().unwrap(),
            *config.frequency_scale.read().unwrap()
        );
        println!("[DEBUG] AudioProcessor.update_config: Old interpolate_bands={}, New interpolate_bands={}", 
                 self.config.interpolate_bands.load(std::sync::atomic::Ordering::Relaxed),
                 config.interpolate_bands.load(std::sync::atomic::Ordering::Relaxed));

        self.config = config;
        println!("[DEBUG] AudioProcessor.update_config: Config updated successfully");
    }
}
