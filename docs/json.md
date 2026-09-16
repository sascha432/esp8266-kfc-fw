# JSON API

## File listing

`GET /file_manager/list` returns a chunked JSON object containing filesystem
information and the entries in the requested directory. The response can be
split at arbitrary chunk or network boundaries; parse the complete response as
JSON after it has been received.

The compact schema is intentionally a breaking change from the previous
verbose field names.

### Envelope

```json
{
	"t": "4.00 MB",
	"T": 4194304,
	"u": "128.00 KB",
	"U": 131072,
	"p": "3.13%",
	"d": "/data/",
	"f": []
}
```

| Key | Type | Meaning |
| --- | --- | --- |
| `t` | string | Human-readable total filesystem size. |
| `T` | number | Total filesystem size in bytes. |
| `u` | string | Human-readable used filesystem size. |
| `U` | number | Used filesystem size in bytes. |
| `p` | string | Used percentage, including the `%` suffix. |
| `d` | string | Current directory, with a trailing slash. |
| `f` | array | Files and directories in the current directory. |

### Entry

```json
{
	"f": "/data/config.json",
	"n": "config.json",
	"s": "2.00 KB",
	"b": 2048,
	"m": 0,
	"d": 0,
	"t": "2026-09-16 12:34"
}
```

| Key | Type | Meaning |
| --- | --- | --- |
| `f` | string | URL-encoded full path. |
| `n` | string | Entry name relative to the requested directory. |
| `s` | string | Human-readable file size. Files only. |
| `b` | number | File size in bytes. Files only. |
| `m` | number | Path type: `0` normal, `1` mapped, `2` temporary directory. |
| `d` | number | Directory flag: `1` directory, `0` file. |
| `t` | string | Optional modification time in `YYYY-MM-DD HH:MM` format. |

Directory entries omit `s` and `b`. Timestamp `t` is omitted when no file time
is available, except mapped entries, which include it when supplied by the
mapping.

The `m` value is also used by the file manager to select read-only rendering.
An empty directory is represented by an envelope whose `f` value is an empty
array.

## WiFi scan

`GET /scan-wifi` returns a chunked JSON response. Parse the complete response
before reading its fields.

The compact scan schema is a breaking change from the previous field names.

Pending scan:

```json
{"p":true,"m":"Network scan still running"}
```

No networks:

```json
{"m":"No WiFi networks in range"}
```

Successful scan:

```json
{
	"r": [
		{
			"t": "has-network-name",
			"d": "network-name",
			"s": "example",
			"c": 6,
			"r": -55,
			"b": "AA:BB:CC:DD:EE:FF",
			"e": "WPA2/PSK"
		}
	]
}
```

| Key | Type | Meaning |
| --- | --- | --- |
| `p` | boolean | `true` while the asynchronous scan is running. |
| `m` | string | Status or error message. |
| `r` | array | Scan results. |

Result keys:

| Key | Type | Meaning |
| --- | --- | --- |
| `t` | string | Table-row CSS class. |
| `d` | string | SSID-cell CSS class, omitted for hidden networks. |
| `s` | string | SSID, or `<i>HIDDEN</i>` for hidden networks. |
| `c` | number | WiFi channel. |
| `r` | number | RSSI in dBm. |
| `b` | string | BSSID/MAC address. |
| `e` | string | Encryption type. |

The key `r` means the result array at the envelope level and RSSI inside a
result item. The response may split JSON objects across HTTP chunks.

## Crash log

`GET /savecrash.json` exposes the saved crash logs on ESP8266 builds. The
endpoint requires authentication and returns JSON in three modes:

- `GET /savecrash.json` lists all saved crash traces.
- `GET /savecrash.json?id=<hex-id>` returns a single crash trace by id.
- `GET /savecrash.json?cmd=clear` clears the saved crash log storage.

List response:

```json
{
	"items": [
		{
			"id": "00000001",
			"ts": "2026-09-16 12:34",
			"t": 1726479246,
			"r": "Exception",
			"st": "...."
		}
	],
	"info": "87% free 512.00 KB/589.82 KB 🚀"
}
```

| Key | Type | Meaning |
| --- | --- | --- |
| `items` | array | Saved crash entries. |
| `info` | string | Storage usage summary, including free/total capacity. |

Entry keys:

| Key | Type | Meaning |
| --- | --- | --- |
| `id` | string | Crash entry id formatted as hexadecimal string, e.g. `"00000001"`. |
| `ts` | string | Timestamp string from the crash log header. |
| `t` | number | Unix timestamp stored with the crash entry. |
| `r` | string | Decoded crash reason string. |
| `st` | string | Captured stack trace / stack information. |

Single trace response:

```json
{
	"trace": "...."
}
```

The `trace` value contains the full crash log text for the selected id. If the
requested `id` does not exist, the server responds with HTTP 410.

Clear response:

```json
{"result":"OK"}
```

## Configuration import/export

`GET /export-settings` exports the current device configuration as a JSON file.
`POST /import-settings` imports a previously exported configuration JSON. Both
endpoints require authentication.

### Export settings

`GET /export-settings` returns the full configuration as JSON content with a
`Content-Disposition` header to suggest a filename like
`kfcfw_config_<hostname>_YYYYMMDD_HHMMSS.json`.

Example payload:

```json
{
	"firmware_version": "...",
	"device": {
		"name": "kfcfw"
	},
	"web_server": {
		"enabled": true
	}
}
```

The exact schema depends on the current firmware configuration. The endpoint
always returns the current configuration serialized to JSON and sets the
response type to `application/json`.

### Import settings

`POST /import-settings` accepts a form field named `config` containing the
serialized JSON config to import.

Request example (form-encoded):

```text
config={"device":{"name":"kfcfw"},...}
```

Successful response:

```json
{"status":200,"count":12,"message":"Success"}
```

Failure response:

```json
{"status":400,"count":-1,"message":"Failed to parse JSON data"}
```

| Key | Type | Meaning |
| --- | --- | --- |
| `status` | number | HTTP-like status code for the import operation. |
| `count` | number | Number of imported config handles on success; `-1` otherwise. |
| `message` | string | Success or parse failure message. |

If the request method is not `POST` or the `config` field is missing, the
server responds with HTTP 405.
