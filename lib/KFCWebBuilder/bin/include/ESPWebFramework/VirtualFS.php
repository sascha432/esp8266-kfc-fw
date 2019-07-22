<?php

$fd = @fopen("..\..\..\data\webui\.mappings", 'rb');
if (!$fd) {
    die("read error\n");
}

$i = unpack("C", fread($fd, 1));
fseekd($fd, $items);
$ds = fread($fd, 0xffff);
fseek($fd, 1);
$h = array();
var_dump($i);
while($i--) {
    $h[] = unpack("S/S/c/L/L/S*", fread($fd, 2 + 2+ 1 + 4 + 4 + 20));
    var_dump($h);
    sleep(10);
    fread($fd,20);

}
$d = fread($fd, 0xffff);

var_dumpo($i);

var_dump($h);
exit;

foreach($h as list($ofs, $ofs2, $flags, $mtime, $file_size)) {
    $o = strtok(substr($d, $ofs), "\0");
    $mh = strtok(substr($d, $ofs), "\0");

    echo $m.":".$p.":".$flags.":".$mtime.":".$file_size."\n";
}