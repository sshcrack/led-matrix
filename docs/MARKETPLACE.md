# LED Matrix Plugin Marketplace

The LED Matrix Plugin Marketplace is a comprehensive system for discovering, installing, and managing plugins for your LED matrix display. It provides a secure and user-friendly way to extend the functionality of your matrix with community-developed plugins.

## Overview

The marketplace consists of three main components:

1. **Backend API** - RESTful endpoints for plugin management on the matrix server
2. **React Native Frontend** - Beautiful mobile/web interface for browsing and installing plugins
3. **Plugin Repository** - GitHub-based plugin hosting with security verification

## Features

### 🔒 Security First
- **SHA512 Verification**: All plugin binaries are verified with cryptographic hashes
- **Memory-Safe Loading**: Proper cleanup and memory management for plugin loading/unloading
- **Sandboxed Execution**: Plugins run in isolated environments
- **Secure Downloads**: HTTPS-only downloads with integrity checking

### 🎨 Beautiful UI
- **Responsive Design**: Works seamlessly on mobile, tablet, and web
- **Search & Filtering**: Find plugins by name, description, or tags
- **Real-time Status**: Live updates on installation progress
- **Plugin Cards**: Rich previews with images, descriptions, and scene listings

### ⚡ Dynamic Management
- **Hot Loading**: Install/uninstall plugins without restarting the matrix
- **Progress Tracking**: Real-time installation progress and status updates
- **Dependency Resolution**: Automatic handling of plugin dependencies
- **Version Management**: Update plugins to newer versions seamlessly

## Architecture

### Marketplace Index Structure

The marketplace uses a centralized JSON index to catalog available plugins:

```json
{
  "version": "1.0",
  "plugins": [
    {
      "id": "example-scenes",
      "name": "Example Scenes",
      "description": "Basic example scenes for LED matrix",
      "version": "1.0.0",
      "author": "LED Matrix Team",
      "tags": ["examples", "basic", "demo"],
      "image": "https://example.com/preview.png",
      "scenes": [
        {
          "name": "Color Pulse",
          "description": "Smooth color pulsing effect"
        }
      ],
      "releases": {
        "1.0.0": {
          "matrix": {
            "url": "https://github.com/repo/releases/download/v1.0.0/plugin.so",
            "sha512": "abc123...",
            "size": 65536
          }
        }
      },
      "compatibility": {
        "matrix_version": ">=1.0.0"
      },
      "dependencies": []
    }
  ]
}
```

### API Endpoints

#### Marketplace Management
- `GET /marketplace/index` - Get the marketplace index
- `POST /marketplace/refresh` - Refresh the index from remote repository
- `GET /marketplace/installed` - List installed plugins
- `GET /marketplace/status/{plugin_id}` - Get plugin installation status

#### Plugin Operations
- `POST /marketplace/install` - Install a plugin
- `POST /marketplace/uninstall` - Uninstall a plugin
- `POST /marketplace/enable` - Enable a plugin
- `POST /marketplace/disable` - Disable a plugin

#### Dynamic Loading (Advanced)
- `POST /marketplace/load` - Dynamically load a plugin
- `POST /marketplace/unload` - Dynamically unload a plugin

### Installation Process

1. **Discovery**: User browses plugins in the React Native marketplace interface
2. **Selection**: User selects a plugin and version to install
3. **Download**: Backend downloads the plugin binary from the release URL
4. **Verification**: SHA512 hash is calculated and verified against expected value
5. **Installation**: Plugin is placed in the appropriate directory structure
6. **Loading**: Plugin is dynamically loaded into the matrix runtime
7. **Registration**: Plugin scenes and providers are registered with the system

## Usage

### For Users

1. **Browse Marketplace**: Open the marketplace screen in the LED Matrix app
2. **Search & Filter**: Use the search bar or category filters to find plugins
3. **Install Plugins**: Tap the install button on any plugin card
4. **Monitor Progress**: Watch real-time installation progress
5. **Manage Plugins**: Enable/disable or uninstall plugins as needed

### For Plugin Developers

1. **Create Plugin**: Develop your plugin using the LED Matrix plugin API
2. **Build Releases**: Create release binaries for your plugin
3. **Calculate Hashes**: Generate SHA512 hashes for all binaries
4. **Host on GitHub**: Upload releases to GitHub with proper versioning
5. **Submit to Index**: Add your plugin to the marketplace index JSON

## Security Considerations

### Hash Verification
All plugin binaries must include SHA512 hashes in the marketplace index. The system will:
- Download the binary from the specified URL
- Calculate the SHA512 hash of the downloaded file
- Compare against the expected hash from the index
- Reject installation if hashes don't match

### Memory Safety
The plugin loading system includes comprehensive memory management:
- Proper cleanup of plugin resources on unload
- Exception handling during plugin initialization
- Safe destruction of plugin objects
- Handle management for dynamic libraries

### Sandboxing
Plugins run within the matrix process but with controlled access:
- Limited file system access
- Network access through controlled APIs
- Resource limits to prevent system abuse
- Monitoring for suspicious behavior

## Configuration

### Environment Variables
- `PLUGIN_DIR` - Directory for plugin installation (default: `./plugins`)
- `MARKETPLACE_INDEX_URL` - URL for the marketplace index JSON

### Plugin Directory Structure
```
plugins/
├── plugin-name/
│   ├── libplugin-name.so      # Matrix plugin binary
│   └── metadata.json          # Plugin metadata
└── .marketplace_cache/
    ├── index.json             # Cached marketplace index
    └── installed.json         # Installed plugins list
```

## API Reference

### Install Plugin
```http
POST /marketplace/install
Content-Type: application/json

{
  "plugin_id": "example-scenes",
  "version": "1.0.0"
}
```

### Get Plugin Status
```http
GET /marketplace/status/example-scenes
```

Response:
```json
{
  "plugin_id": "example-scenes",
  "status": 1,
  "status_string": "installed"
}
```

### Status Values
- `not_installed` (0) - Plugin is not installed
- `installed` (1) - Plugin is installed and up to date
- `update_available` (2) - Newer version available
- `downloading` (3) - Currently downloading
- `installing` (4) - Currently installing
- `error` (5) - Installation error occurred

## Troubleshooting

### Common Issues

**Plugin Not Loading**
- Check that the plugin binary is compatible with your system architecture
- Verify the SHA512 hash matches the expected value
- Ensure all dependencies are installed

**Installation Fails**
- Check network connectivity to the plugin repository
- Verify disk space is available for plugin installation
- Check server logs for detailed error messages

**Memory Issues**
- Monitor system memory usage during plugin operations
- Restart the matrix server if memory leaks are suspected
- Check for zombie plugin processes

### Debug Mode
Enable verbose logging for marketplace operations:
```bash
export SPDLOG_LEVEL=debug
./matrix-server
```

## Contributing

To contribute to the marketplace system:

1. **Backend Changes**: Modify the C++ marketplace client and server code
2. **Frontend Changes**: Update the React Native marketplace components
3. **Plugin Index**: Submit plugins through the marketplace repository
4. **Documentation**: Help improve this documentation

## Future Enhancements

- [ ] Plugin ratings and reviews system
- [ ] Automatic updates for installed plugins
- [ ] Plugin categories and featured plugins
- [ ] Desktop application marketplace integration
- [ ] Plugin sandboxing with containerization
- [ ] Marketplace analytics and usage tracking