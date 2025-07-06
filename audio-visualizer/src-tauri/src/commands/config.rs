use crate::config::AudioVisualizerConfig;

/// Save the application configuration to disk
#[tauri::command]
pub fn save_config(config: serde_json::Value) -> Result<(), String> {
    println!("[DEBUG] save_config: Saving configuration to disk");
    let config_obj: AudioVisualizerConfig = serde_json::from_value(config).map_err(|e| {
        println!("[ERROR] save_config: Failed to parse config: {}", e);
        format!("Failed to parse config: {}", e)
    })?;

    match config_obj.save() {
        Ok(_) => {
            println!("[DEBUG] save_config: Configuration saved successfully");
            Ok(())
        }
        Err(e) => {
            println!("[ERROR] save_config: Failed to save config: {}", e);
            Err(format!("Failed to save config: {}", e))
        }
    }
}

/// Load the application configuration from disk
#[tauri::command]
pub fn load_config() -> Result<AudioVisualizerConfig, String> {
    println!("[DEBUG] load_config: Loading configuration from disk");
    match AudioVisualizerConfig::load() {
        Ok(config) => {
            println!("[DEBUG] load_config: Configuration loaded successfully");
            Ok(config)
        }
        Err(e) => {
            println!("[ERROR] load_config: Failed to load config: {}", e);
            Err(format!("Failed to load config: {}", e))
        }
    }
}
