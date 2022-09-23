<?php

$autoloadFile = __DIR__ . '/vendor/autoload.php';
if (file_exists($autoloadFile)) {
    require $autoloadFile;
} else {
    require dirname(dirname(dirname(__DIR__))) . '/vendor/autoload.php';
}
