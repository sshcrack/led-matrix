use tauri::WebviewWindow;

/// Toggle the visibility of the application window
#[tauri::command]
pub fn toggle_window_visibility(window: WebviewWindow) -> Result<(), String> {
    if window.is_visible().unwrap_or(true) {
        window
            .hide()
            .map_err(|e| format!("Failed to hide window: {}", e))
    } else {
        window
            .show()
            .map_err(|e| format!("Failed to show window: {}", e))?;
        window
            .set_focus()
            .map_err(|e| format!("Failed to focus window: {}", e))
    }
}
