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

`GET /savecrash.json` exposes the crash report of the device. The endpoint
requires authentication. The response depends on the platform:

- **ESP8266** — crash reports are captured by the SaveCrash module into a
dedicated flash region, the endpoint returns the list of saved crash traces.
- **ESP32** — the ESP-IDF panic handler stores the core dump in the `coredump`
data partition, the endpoint returns its summary.

### ESP8266 crash log

`GET /savecrash.json` supports three modes:

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

### ESP32 core dump

On ESP32 builds the crash report is written by the ESP-IDF panic handler to the
`coredump` data partition, the SaveCrash storage is not used. The endpoint
supports two modes:

- `GET /savecrash.json` returns the summary of the stored core dump.
- `GET /savecrash.json?cmd=erase-coredump` removes the stored core dump.
- `GET /coredump.bin` downloads the stored core dump (HTTP 404 if there is
  none).

Summary response (the object is omitted if no core dump is stored, in that case
the response is an empty JSON object):

```json
{
	"coredump": {
		"size": 45312,
		"version": 1,
		"task": "loopTask",
		"pc": "0x400d1f4e",
		"cause": 28,
		"cause_name": "LoadProhibitedCause",
		"vaddr": "0x00000000",
		"backtrace": ["0x400d1f4e", "0x4019c9b8"],
		"corrupted": false,
		"sha256": "fafe260085d9f4c0"
	}
}
```

| Key | Type | Meaning |
| --- | --- | --- |
| `size` | number | Size of the stored core dump in bytes. |
| `version` | number | Core dump format version. |
| `task` | string | Name of the task that caused the exception. |
| `pc` | string | Program counter at the exception. |
| `cause` | number | Xtensa exception cause. |
| `cause_name` | string | Name of the exception cause, e.g. `StoreProhibitedCause` for a write to an invalid address. |
| `vaddr` | string | Virtual address of the exception. |
| `backtrace` | array | Application backtrace (array of program counters). |
| `corrupted` | boolean | `true` if the backtrace is corrupted. |
| `sha256` | string | First hex characters of the SHA256 of the firmware ELF that produced the dump. |

Clear response:

```json
{"result":"OK"}
```

The downloaded dump can be decoded with the ESP-IDF `esp-coredump` tool and the
matching `firmware.elf` of the build that crashed:

```
pip install esp-coredump
esp-coredump info_corefile -c coredump.bin -e .pio/build/<env>/firmware.elf
```

The dump is stored as an ELF core file (`CONFIG_ESP_COREDUMP_DATA_FORMAT_ELF`),
so it can also be loaded directly by `xtensa-esp32-elf-gdb`. Use `-t raw` if the
firmware was built with the raw core dump format and the tool does not detect it.

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
