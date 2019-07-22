<?php
/**
 * Author: sascha_lammers@gmx.de
 */

require_once __DIR__.'/argv.php';

$argParser = new ArgsParser();
$argParser->addBoolOption('is_verbose', array('-v', '--verbose'), false, 'Make the operation more talkative');
$argParser->addMultiOption('file_pattern', array('-f', '--file-pattern'), array('*'), '*.txt', 'Match found filenames to this pattern');
$argParser->addOption('percent', array('-p'), 85.0, null, 'Match any character if more than N percent mismatch. This allows to show results that have a close common match between all results');
$argParser->addOption('offset', array('-o', '--offset'), 0, 'start offset', 'Start offset before comparing any content');
$argParser->addOption('length', array('-l', '--length'), 16, null, 'Length of the content to compare');
$argParser->addOption('results', array('-r', '--results'), 10, null, 'Number of results to display');
$argParser->addOption('store', array('--store'), null, null, 'Store results in a file and exit');
$argParser->addOption('load', array('--load'), null, null, 'load results from a file and compare');
$argParser->addArgument('directory', 1, null, 'Directory to scan');

// avoid warnings "undefined variable"
$offset = $length = $is_verbose = $percent = $file_pattern = $directory = $results = $load = $store = null;
foreach($argParser->parse(true) as $key => $val) {
    $$key = $val;
}

/**
 * @param array $patterns
 * @return string
 */
function create_regex($patterns) {
    $out = array();
    foreach($patterns as $pattern) {
        $out[] = str_replace('\\*', '.*', preg_quote($pattern, '/'));
    }
    return '/^('.implode('|', $out).')$/U'.(strncasecmp(PHP_OS, 'WIN', 3) === 0 ? 'i' : '');

}

/**
 * @param string|array $val
 * @return string
 */
function hex_str($val)
{
    $out = '';
    for($i = 0, $len = is_array($val) ? count($val) : strlen($val); $i < $len; $i++) {
        if ($val[$i] === false) {
            $out .= '.';
        } else {
            $out .= sprintf('\\x%02x', ord($val[$i]));
        }
    }
    return $out;
}

/**
 * @param string $dir
 * @param string $pattern
 * @param string $excludePathPattern
 */
function recursive_scan($dir, $pattern, $excludePathPattern) {

    global $contents, $offset, $length, $is_verbose, $min_length;

    $dh = @dir($dir);
    if ($dh) {
        while(($entry = $dh->read()) !== false) {
            $path = rtrim($dir, '\\/').DIRECTORY_SEPARATOR.$entry;
            if (!in_array($entry, array('.', '..'), true) && !preg_match($excludePathPattern, $dir)) {
                if (is_readable($path)) {
                    if (is_dir($path)) {
                        recursive_scan($path, $pattern, $excludePathPattern);
                    } else if (preg_match($pattern, $entry)) {
                        if (($len = filesize($path)) >= $min_length) {
                            $fd = @fopen($path, 'rb');
                            if ($fd) {
                                fseek($fd, $offset);
                                $contents[] = $content = preg_split('//', fread($fd, $length), -1, PREG_SPLIT_NO_EMPTY);
                                if ($is_verbose) {
                                    echo $path.' '.hex_str($content)."\n";
                                }
                                fclose($fd);
                            } else if ($is_verbose) {
                                echo "$path: Cannot read file\n";
                            }
                        } else if ($is_verbose) {
                            echo "$path: File too small $len < $min_length\n";
                        }
                    }
                }
            }
        }
        $dh->close();
    }
}

if (!file_exists($directory) || !is_dir($directory)) {
    echo "$directory: Directory does not exist\n";
    exit(1);
}
$directory = realpath($directory);
if ($is_verbose) {
    echo "Directory $directory file pattern ".implode('|', $file_pattern)."...\n";
}

if ($load) {

    if (($contents = @file_get_contents($load)) === false) {
        echo "$load: Failed to read file\n";
        exit(1);
    }
    $contents = @unserialize($contents);
    if (!is_array($contents)) {
        echo "$load: File contains invalid data\n";
        exit(1);
    }

} else {

    $results = max(1, $results);
    $contents = array();
    $pattern = create_regex($file_pattern);
    $min_length = $offset + $length;
    recursive_scan($directory, $pattern, (strncasecmp(PHP_OS, 'WIN', 3) === 0) ? '/^[a-zA-Z]:[\\\\\/](\$RECYCLE\.BIN)(\\\\\/)?$/' : '/^\/(dev|proc|sys|lost\+found)(\/)?$/');

    if (count($contents) === 0) {
        echo "No files found\n";
        exit(127);
    }

    if ($store) {
        if (@file_put_contents($store, serialize($contents))) {
            echo "$store: Cannot write to file\n";
            exit(1);
        }
        echo "Results stored in $store\n";
        exit(0);
    }

}

if ($is_verbose) {
    echo 'Comparing results '.count($contents)."...\n";
}
$tested = array();
$n = 0;
$maxMisMatches = max(1, floor($percent * (count($contents) - 1) / 100));
$filteredMatches = array();

$updateFrequency = max(1, round(count($contents) / 1000));
$maxMatch = null;

foreach($contents as $key => $content) {
    if ($n % $updateFrequency === 0) {
        echo sprintf("\r%.1f%%", ($n / count($contents)) * 100);
    }
    $n++;

    if (!isset($tested[$c_str = implode($content)])) {
//        $len = strlen($content);
        $len = count($content);
        $arr = array_fill(0, $length, 0);
        // amazing how slow php is... using array is ~25-30% faster on a core i7 with php 7.2 but 10% slower on a raspberry pi with php 7.0
        foreach($contents as $key2 => $content2) {
            if ($key !== $key2/* && $content !== $content2*/) {
                foreach(array_diff_assoc($content, $content2) as $i => $val) {
                    if ($arr[$i] !== false && ++$arr[$i] > $maxMisMatches) {
                        $arr[$i] = false;
                    }
                }
//                if ($arr[0]>0) {
//                    var_dump(hex_str($content));
//                    var_dump(hex_str($content2));
//                    exit;
//                }
//                for($i = 0; $i < $len; $i++) {
//                    if ($arr[$i] !== false && $content[$i] !== $content2[$i] && ++$arr[$i] > $maxMisMatches) {
//                        $arr[$i] = false;
//                    }
//                }
            }
        }
        $tested[$c_str] = true;

        $partial_array = array();
        for($i = 0, $count = count($arr); $i < $count; $i++) {
            if ($arr[$i] === false) {
                $partial_array[$i] = false;
                $content[$i] = '.';
            } else {
                $partial_array[$i] = $content[$i];
            }
        }
        if (array_filter($partial_array) && !isset($filteredMatches[$hex_str = hex_str($partial_array)])) {
            $matches = 0;
            $len = count($partial_array);
            foreach($contents as $key2 => $content2) {
                if ($key !== $key2 && !array_filter(array_diff_assoc($partial_array, $content2))) {
                    $matches++;
                }
//                $is_match = true;
//                for($i = 0; $i < $len && $is_match; $i++) {
//                    if ($partial_array[$i] !== false && $partial_array[$i] !== $content2[$i]) {
//                        $is_match = false;
//                    }
//                }
//                if ($is_match) {
//                    $matches++;
//                }
            }
            $filteredMatches[$hex_str] = $matches;
            if ($matches >= max($filteredMatches)) {
                $maxMatch = $partial_array;
            }
        }
    }
}


echo "        \r";

arsort($filteredMatches);

$count = count($contents) - 1;
$maxCount = reset($filteredMatches);
if ($maxCount < $count) {

    for(;;) {
        $len = count($maxMatch);
        for($i = $len - 1; $i >= 0; $i--) {
            if ($maxMatch[$i] !== false) {
                $maxMatch[$i] = false;
                break;
            }
        }
        if ($i === 0) {
            break;
        }

        $matches = 0;
        foreach($contents as $key => $content) {
            if (!array_filter(array_diff_assoc($maxMatch, $content))) {
                $matches++;
            }
        }
        if ($matches > $maxCount) {
            $maxCount  = $matches;
            $filteredMatches[hex_str($maxMatch)] = $matches;
            if ($matches === $count) {
                break;
            }
        }
    }

    arsort($filteredMatches);
}

$n = 1;
foreach(array_slice($filteredMatches, 0, $results) as $hex_str => $counter) {
    echo "$n $hex_str $counter / ".$count."\n";
    $n++;
}
if (count($filteredMatches) > $results) {
    $backwardCount = min($results, count($filteredMatches) - $results);
    echo "...\n";
    $n = count($filteredMatches) - $backwardCount;
    foreach(array_slice($filteredMatches, -$backwardCount) as $hex_str => $counter) {
        $n++;
        echo "$n $hex_str $counter / ".$count."\n";
    }
}
