use std::sync::{Arc, RwLock, atomic::AtomicBool};

use anyhow::Result;
use serde::{Deserialize, Serialize};

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct AudioVisualizerConfig {
    pub hostname: String,
    pub port: u16,
    pub num_bands: usize,
    pub gain: f32,
    pub smoothing: f32,
    pub min_freq: f32,
    pub max_freq: f32,
    pub mode: u8, // 0 = discrete frequencies, 1 = 1/3 octave bands, 2 = full octave bands
    pub linear_amplitude: bool, // true = linear amplitude, false = logarithmic (dB) amplitude
    #[serde(with = "rwlock_serde")]
    pub frequency_scale: Arc<RwLock<String>>, // "log", "linear", "bark", "mel"

    #[serde(with = "atomic_bool_serde")]
    pub interpolate_bands: Arc<AtomicBool>, // Whether to interpolate frequency bands that cannot be directly calculated
}

mod rwlock_serde {
    use serde::de::Deserializer;
    use serde::ser::Serializer;
    use serde::{Deserialize, Serialize};
    use std::sync::{Arc, RwLock};

    pub fn serialize<S, T>(val: &Arc<RwLock<T>>, s: S) -> Result<S::Ok, S::Error>
    where
        S: Serializer,
        T: Serialize,
    {
        T::serialize(&*val.read().unwrap(), s)
    }

    pub fn deserialize<'de, D, T>(d: D) -> Result<Arc<RwLock<T>>, D::Error>
    where
        D: Deserializer<'de>,
        T: Deserialize<'de>,
    {
        Ok(Arc::new(RwLock::new(T::deserialize(d)?)))
    }
}

mod atomic_bool_serde {
    use serde::de::Deserializer;
    use serde::ser::Serializer;
    use serde::{Deserialize, Serialize};
    use std::sync::Arc;
    use std::sync::atomic::{AtomicBool, Ordering};

    pub fn serialize<S>(val: &Arc<AtomicBool>, s: S) -> Result<S::Ok, S::Error>
    where
        S: Serializer,
    {
        bool::serialize(&val.load(Ordering::Acquire), s)
    }

    pub fn deserialize<'de, D>(d: D) -> Result<Arc<AtomicBool>, D::Error>
    where
        D: Deserializer<'de>,
    {
        Ok(Arc::new(AtomicBool::new(bool::deserialize(d)?)))
    }
}

impl Default for AudioVisualizerConfig {
    fn default() -> Self {
        Self {
            hostname: "".to_string(),
            port: 0,
            num_bands: 64,
            gain: 2.0, // Increased default gain for the new scaling
            smoothing: 0.8,
            min_freq: 20.0,
            max_freq: 20000.0,
            mode: 0,                 // 0 = discrete frequencies (log scale)
            linear_amplitude: false, // Use logarithmic (dB) amplitude by default
            frequency_scale: Arc::new(RwLock::new("log".to_string())), // Logarithmic frequency scale by default
            interpolate_bands: Arc::new(AtomicBool::new(true)), // Enable band interpolation by default
        }
    }
}

impl AudioVisualizerConfig {
    pub fn load() -> Result<Self> {
        let config_dir = dirs::config_dir().unwrap().join("audio-visualizer");
        let config_file = config_dir.join("config.toml");

        if config_file.exists() {
            let content = std::fs::read_to_string(config_file)?;
            let config: AudioVisualizerConfig = toml::from_str(&content)?;
            Ok(config)
        } else {
            Ok(AudioVisualizerConfig::default())
        }
    }

    pub fn save(&self) -> Result<()> {
        let config_dir = dirs::config_dir().unwrap().join("audio-visualizer");
        std::fs::create_dir_all(&config_dir)?;
        let config_file = config_dir.join("config.toml");

        let content = toml::to_string_pretty(self)?;
        std::fs::write(config_file, content)?;
        Ok(())
    }
}
