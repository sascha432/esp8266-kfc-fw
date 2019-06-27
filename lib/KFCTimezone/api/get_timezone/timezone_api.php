<?php
/**
 * Author: sascha_lammers@gmx.de
 */

/*

Usage:

api.php?key=_CHANGEME_&by=zone&format=json&zone=America/Vancouver
api.php?key=_CHANGEME_&by=zone&format=json&zone=PST

Result:

{"status":"OK","message":"","countryCode":"CA","zoneName":"America\/Vancouver","abbreviation":"PST","gmOffset":-28800,"dst":"0","zoneStart":1552212000,"zoneEnd":1583661599,"nextAbbreviation":"PDT","timestamp":1542119691,"formatted":"2018-11-13 14:34:51"}

*/
if (php_sapi_name() == "cli") {
    $_REQUEST['key']="_CHANGEME_";
    $_REQUEST['by']="zone";
    $_REQUEST['zone']="America/Vancouver";
}

if (@((String)$_REQUEST['key']) != '_CHANGEME_') {
	if (function_exists('http_response_code')) {
		http_response_code(403);
	} else {
		header("Connection: close", true, 403);
	}
	exit;
}

$ys = 31536000;
$errors = 0;
$message = array("Internal error");
$countryCode = null;
$zoneName = null;
$abbreviation = null;
$gmOffset = null;
$zoneStart = null;
$zoneEnd = null;
$nextAbbreviation = null;
$dst = null;
$timestamp = null;
$formatted = null;
$timestampFormat = "Y-m-d H:i:s";

switch((String)@$_REQUEST["by"]) {
    case "zone":
        break;
    default:
        $message[] = "Invalid 'by', only 'zone' is supported";
        $errors++;
        break;
}
switch((String)@$_REQUEST["format"]) {
    case "json":
        break;
    default:
        $message[] = "Invalid 'format', only 'json' is supported";
        break;
}

try {
    $now = new DateTime(null);
    $now->setTimezone(new DateTimeZone((String)@$_REQUEST["zone"]));
    $gmOffset = $now->getOffset();
    $zoneName = $now->getTimezone()->getName();
    $countryCode = $now->getTimezone()->getLocation()["country_code"];
    $timestamp = $now->getTimestamp() * 1;
    $trans = $now->getTimezone()->getTransitions($timestamp - $ys, $timestamp + $ys*2);

    // timestamp is between current and next transition
    foreach($trans as $current => $data) {
        var_dump($data);
        if ($timestamp > $data['ts'] && $timestamp <= $trans[$current + 1]['ts']) {
            break;
        }
    }
    $zoneStart = $trans[$current]["ts"];
    $current++;
    $abbreviation = $trans[$current]["abbr"];
    $dst = $trans[$current]["isdst"] ? "1" : "0";
    $zoneEnd = $trans[$current]["ts"] - 1;
    $nextAbbreviation = $trans[$current]["abbr"];

} catch(Exception $e) {
    $errors++;
    $message[] = "Invalid zone name";
}

if ($errors && sizeof($message) > 1) {
    array_shift($message);
}

$timestamp = new DateTime('@'.$timestamp);
$timestamp->setTimezone(new DateTimeZone($zoneName));

$response = array_filter(array(
    "status" => $errors == 0 ? "OK" : "ERROR",
    "message" => $errors == 0 ? "" : implode("\n", $message),
    "countryCode" => $countryCode,
    "zoneName" => $zoneName,
    "abbreviation" => $abbreviation,
    "gmOffset" => $gmOffset,
    "dst" => $dst,
    "zoneStart" => $zoneStart,
    "zoneEnd" => $zoneEnd,
    "nextAbbreviation" => $nextAbbreviation,
    "timestamp" => $timestamp->getTimestamp(),
    "formatted" => $timestamp->format($timestampFormat),
), function($value) { return $value !== null; });

$out = json_encode($response)."\n";

header("Content-type: application/json");
header("Content-length: ".strlen($out));
header("Connection: close");

echo $out;
