<?php

function error($msg) {
    global $is_text;
    if ($is_text) {
        die($msg);
    }
    else {
        die(json_encode(array("e" => $msg)));
    }
}

$is_text = @$_GET["text"] == 1;

$names = array();
$addresses = array();

$args = @$_GET["name"];
if (!is_array($args)) {
    $args = @explode(',', $args);
}
if (count($args)) {
    foreach($args as $value) {
        $value = trim(strval($value));
        if ($value) {
            $names[] = $value;
        }
    }
}

$args = @$_GET["addr"];
if (!is_array($args)) {
    $args = @explode(',', $args);
}
if (count($args)) {
    foreach($args as $value) {
        $value = trim(strval($value));
        if ($value) {
            $addresses[] = $value;
        }
    }
}

if (sizeof($names) == 0 && sizeof($addresses) == 0) {
    error("arguments missing: name or addr required");
}

if ($is_text) {
    $format = "{address}={name},{size},{section}";
    header("Content-Type: text/plain");
}
else {
    $format = "json";
    header("Content-Type: application/json");
}

$file = basename(str_replace("\\", "/", @$_GET["file"]));
if (!file_exists($file)) {
    error("$file: file not found");
}

$args = array("python", "dump_sections.py", "none", "--addr2line", "-f", $format, "--elf", $file);
if ($names) {
    array_push($args, "-n");
    array_walk($names, function($name, $key) {
        global $args;
        array_push($args, $name);
    });

}
if ($addresses) {
    array_push($args, "-a");
    array_walk($addresses, function($addr, $key) {
        global $args;
        array_push($args, $addr);
    });

}
foreach($args as $k => $arg) {
    $args[$k] = escapeshellarg($arg);
}

$output = array();
exec(implode(" ", $args), $output);

foreach($output as $line) {
    echo $line."\n";
}

