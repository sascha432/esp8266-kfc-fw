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
