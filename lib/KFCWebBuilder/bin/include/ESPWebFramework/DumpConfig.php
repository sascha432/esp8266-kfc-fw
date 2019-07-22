<?php
/**
 * Author: sascha_lammers@gmx.de
 */

namespace ESPWebFramework;

/**
 * Class DumpConfig
 * @package ESPWebFramework
 */
class DumpConfig {
    /**
     * @param object
     */
    public static function dump($json): void
    {
        static::unescapeJson($json);
        echo json_encode($json, JSON_PRETTY_PRINT|JSON_UNESCAPED_SLASHES)."\n";
    }

    /**
     * @param array|object $json
     */
    private static function unescapeJson(&$json): void
    {
        if (is_object($json)) {
            $json = json_decode(json_encode($json), true);
        }
        foreach($json as $key => &$val) {
            if (is_array($val) || is_object($val)) {
                static::unescapeJson($val);
            } else {
                if (strncmp($key, ConfigReader::UNESCAPED_STRING_MARKED, strlen(ConfigReader::UNESCAPED_STRING_MARKED)) === 0) {
                    $rkey = substr($key, strlen(ConfigReader::UNESCAPED_STRING_MARKED));
                    $copiedValRef = $val;
                    if (isset($json[$rkey])) {
                        $json[$rkey] = $copiedValRef;
                        unset($json[$key]);
                    }
                }
            }
        }
    }
}
