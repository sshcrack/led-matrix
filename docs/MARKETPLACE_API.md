# Marketplace API Documentation

This document describes the REST API endpoints for the LED Matrix Plugin Marketplace.

## Base URL

All API endpoints are relative to the matrix server base URL:
```
http://<matrix-ip>:<port>
```

Default port is typically 8080.

## Authentication

Currently, no authentication is required for marketplace endpoints. This may change in future versions.

## Endpoints

### Get Marketplace Index

Retrieves the current marketplace index with all available plugins.

```http
GET /marketplace/index
```

**Response:**
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

**Error Responses:**
- `404 Not Found` - No marketplace index available

---

### Refresh Marketplace Index

Fetches the latest marketplace index from the remote repository.

```http
POST /marketplace/refresh?url=<optional-custom-url>
```

**Query Parameters:**
- `url` (optional) - Custom URL for marketplace index

**Response:**
```json
{
  "message": "Index refresh started"
}
```

**Status Codes:**
- `202 Accepted` - Refresh started successfully
- `500 Internal Server Error` - Failed to start refresh

---

### Get Installed Plugins

Returns a list of all currently installed plugins.

```http
GET /marketplace/installed
```

**Response:**
```json
[
  {
    "id": "example-scenes",
    "version": "1.0.0",
    "install_path": "/path/to/plugins/example-scenes",
    "enabled": true
  }
]
```

---

### Get Plugin Status

Retrieves the installation status of a specific plugin.

```http
GET /marketplace/status/{plugin_id}
```

**Path Parameters:**
- `plugin_id` - The unique identifier of the plugin

**Response:**
```json
{
  "plugin_id": "example-scenes",
  "status": 1,
  "status_string": "installed"
}
```

**Status Values:**
| Value | Status String | Description |
|-------|---------------|-------------|
| 0 | `not_installed` | Plugin is not installed |
| 1 | `installed` | Plugin is installed and up to date |
| 2 | `update_available` | Newer version is available |
| 3 | `downloading` | Currently downloading |
| 4 | `installing` | Currently installing |
| 5 | `error` | Installation error occurred |

---

### Install Plugin

Installs a plugin from the marketplace.

```http
POST /marketplace/install
Content-Type: application/json
```

**Request Body:**
```json
{
  "plugin_id": "example-scenes",
  "version": "1.0.0"
}
```

**Response:**
```json
{
  "message": "Installation started",
  "plugin_id": "example-scenes",
  "version": "1.0.0"
}
```

**Status Codes:**
- `202 Accepted` - Installation started successfully
- `400 Bad Request` - Missing required parameters
- `404 Not Found` - Plugin or version not found
- `500 Internal Server Error` - Installation failed

**Error Response:**
```json
{
  "error": "Plugin not found"
}
```

---

### Uninstall Plugin

Removes an installed plugin from the system.

```http
POST /marketplace/uninstall
Content-Type: application/json
```

**Request Body:**
```json
{
  "plugin_id": "example-scenes"
}
```

**Response:**
```json
{
  "message": "Uninstallation started",
  "plugin_id": "example-scenes"
}
```

**Status Codes:**
- `202 Accepted` - Uninstallation started successfully
- `400 Bad Request` - Missing plugin_id
- `500 Internal Server Error` - Uninstallation failed

---

### Enable Plugin

Enables a previously disabled plugin.

```http
POST /marketplace/enable
Content-Type: application/json
```

**Request Body:**
```json
{
  "plugin_id": "example-scenes"
}
```

**Response:**
```json
{
  "message": "Plugin enabled",
  "plugin_id": "example-scenes"
}
```

**Status Codes:**
- `200 OK` - Plugin enabled successfully
- `404 Not Found` - Plugin not found
- `400 Bad Request` - Missing plugin_id

---

### Disable Plugin

Disables an active plugin without uninstalling it.

```http
POST /marketplace/disable
Content-Type: application/json
```

**Request Body:**
```json
{
  "plugin_id": "example-scenes"
}
```

**Response:**
```json
{
  "message": "Plugin disabled",
  "plugin_id": "example-scenes"
}
```

**Status Codes:**
- `200 OK` - Plugin disabled successfully
- `404 Not Found` - Plugin not found
- `400 Bad Request` - Missing plugin_id

---

### Load Plugin (Advanced)

Dynamically loads a plugin from a file path. This is an advanced endpoint typically used internally.

```http
POST /marketplace/load
Content-Type: application/json
```

**Request Body:**
```json
{
  "plugin_path": "/path/to/plugin/libplugin.so"
}
```

**Response:**
```json
{
  "message": "Plugin loaded successfully",
  "plugin_path": "/path/to/plugin/libplugin.so"
}
```

**Status Codes:**
- `200 OK` - Plugin loaded successfully
- `400 Bad Request` - Missing plugin_path
- `500 Internal Server Error` - Failed to load plugin

---

### Unload Plugin (Advanced)

Dynamically unloads a plugin from memory. This is an advanced endpoint typically used internally.

```http
POST /marketplace/unload
Content-Type: application/json
```

**Request Body:**
```json
{
  "plugin_id": "example-scenes"
}
```

**Response:**
```json
{
  "message": "Plugin unloaded successfully",
  "plugin_id": "example-scenes"
}
```

**Status Codes:**
- `200 OK` - Plugin unloaded successfully
- `404 Not Found` - Plugin not found or not loaded
- `400 Bad Request` - Missing plugin_id
- `500 Internal Server Error` - Failed to unload plugin

## Error Handling

All endpoints return consistent error responses in JSON format:

```json
{
  "error": "Error message describing what went wrong"
}
```

Common HTTP status codes:
- `200 OK` - Request succeeded
- `202 Accepted` - Request accepted, processing asynchronously
- `400 Bad Request` - Invalid request parameters
- `404 Not Found` - Resource not found
- `500 Internal Server Error` - Server error occurred

## CORS Support

All marketplace endpoints include CORS headers for cross-origin requests:
- `Access-Control-Allow-Origin: *`
- `Access-Control-Allow-Methods: GET, POST, PUT, DELETE, OPTIONS`
- `Access-Control-Allow-Headers: Content-Type, Authorization, X-Requested-With`

## Rate Limiting

Currently, no rate limiting is implemented. Future versions may include rate limiting to prevent abuse.

## Examples

### Installing a Plugin with cURL

```bash
curl -X POST http://localhost:8080/marketplace/install \
  -H "Content-Type: application/json" \
  -d '{"plugin_id": "example-scenes", "version": "1.0.0"}'
```

### Checking Plugin Status

```bash
curl http://localhost:8080/marketplace/status/example-scenes
```

### Getting Marketplace Index

```bash
curl http://localhost:8080/marketplace/index | jq .
```

## WebSocket Events (Future)

Future versions may include WebSocket support for real-time updates:
- Installation progress events
- Plugin status changes
- Marketplace index updates

## SDK Integration

The marketplace API is designed to be easily integrated with:
- React Native mobile applications
- Web frontend applications
- Desktop applications
- Command-line tools
- Third-party management systems