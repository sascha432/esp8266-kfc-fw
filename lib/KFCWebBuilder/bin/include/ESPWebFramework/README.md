# ESPWebFramework

## Introduction

The ESP Web Framework consists of C++ classes to improve building web sites with the ESP8266 and ESP32. It also includes a tool to create a package of optimized html templates (with #ifdef support), minified JS and CSS code and support for long filenames on SPIFFS/LittleFS

## Requirements

  - PHP 7.4.28
  - Python 3.9.10
  - Java 8 (JRE)
  - NodeJS 16.14.2
  - uglify-js 3.15.3
  - gzip 1.11

Other version might work as well, but have not been tested

## Classes

### Mappings

include file: `fs_mapping.h`

#### File format `.listings`

`.listings.txt` is a human readable form of it

```
byte                            Number of entries (type defined as FS_MAPPING_COUNTER_TYPE)

struct header
    word                        Offset of the real filename
    word                        Offset of the path on SPIFFS
    byte                        Flags bitset (0x01 = Gzipped content)
    dword                       Last modified unixtime
    byte[20]                    sha1 hash of the file
[repeated # of entries]

byte[size]
    filename\0                  After the headers a block with the filenames starts.
    SPIFFS filename\0           Each filename is NUL terminated. The total size is
    [repeated # of entries]     <i>file size - (1 + sizeof(header) * #entries)</i>

```

**NOTE:** To support more than 255 files, FS_MAPPING_COUNTER_TYPE can be set to uint16_t.

## Web Builder

### Configuration

#### Platform IO

#### SPIFFS output and long filenames with _Mapper_

The _Mapper_ plugin provides support for long filenames on SPIFFS. It renames all files to a 2 or 4* digit hexadecimal number and creates a mapping file for translating the SPIFFS filenames. It also stores a hash of the file's content to support incremental updates.

*If <i>FS_MAPPING_COUNTER_TYPE</i> is uint8_t, the length of the filenames is 2

##### Configuration:

**NOTE:** All files in _web_target_dir_ that match `[0-9a-f]{2,8}` and `.listings*` get deleted by the <i>Mapper</i> plugin

<pre><code>
{
    "spiffs": {
        // Points to your <i>SPIFFS data</i> directory of PlatformIO
        "data_dir": "./data",

        // Points to your data directory or a sub directory
        "web_target_dir": "./data/www",

        // default location
        "listings_file": "${spiffs.web_target_dir}/.listings",
    }
}
</code></pre>

##### Usage:

The <i>Mapper</i> plugin runs as processor and can either be added to each group's processors, or added to several groups like shown in the example below.

<pre><code>
{
    "web_builder": {
        "groups":
            "<i>GROUP NAME</i>": {
                <i>// list all groups where to append this processor</i>
                "append": [ "group a", "group b", "/.*/" ],
                "processors":  {
                    "map": {
                        "command": {
                            "type": "php_class",
                            "class_name": "\\ESPWebFramework\\Mapper"
                        },
                        "publish": true
                    }
                }
            }
        }
    }
}
</code></pre>

#### Groups

The builder supports different groups, which can run different actions on the defined source files.

#### Processors

Each processor gets executed if all conditions match. If a processor publishes a file, it gets removed from the queue and the won't be passed to any following processor. An exception is the internal command @remove-file, which removes the file from the queue without publishing it.

<pre><code>
"groups":
"processors": {
    "type": "command",
    "command": "/path/to/application argument1 argument2"
}

or

"command": "/path/to/application argument1 argument2"

</code></pre>


#### Commands

##### Launch external application

<pre><code>
"command": {
    "type": "command",
    "command": "/path/to/application argument1 argument2"
}

or

"command": "/path/to/application argument1 argument2"

</code></pre>

##### Execute php code

<pre><code>
"command": {
    "type": "php_eval",
    "code": "echo 'execute some php code'; return 0;"
}
</code></pre>

##### Execute php plugin

Execute a php plugin. If include_file is specified, the builder includes this file before initializing the class. The class must implement the interface \ESPWebFramework\PluginInterface.

<pre><code>
"command": {
    "type": "php_class",
    "inlude_file": "optional_include.php",
    "class": "\\Namespace\\ClassName"
}
</code></pre>

##### Execute an internal command

<pre><code>
"command": {
    "type": "internal_command",
    "command": "@<name>,
}

or

"command": "@<name>"

</code></pre>

#### Internal commands

#### @copy

Copy file from source to target.

#### @copy-combined

Copy all files into a single file. After that operation the source file contains a list of all files copied.

#### @remove-file

The matching files will be removed from the queue and not processed anymore.

#### @create-html-template

The source file is preprocessed (all #if defs resolved and files included) and copied to the target.
