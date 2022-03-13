<?php
/**
 * Author: sascha_lammers@gmx.de
 */

namespace ESPWebFramework;

/**
 * Class FileUtils
 * @package ESPWebFramework
 */
class FileUtils {

    /**
     * @var string
     */
    private static $baseDir;

    /**
     * @param string $baseDir
     */
    public static function setBaseDir(string $baseDir): void
    {
        static::$baseDir = $baseDir;
    }

    public static function findFiles(string $filename): array
    {
        if (file_exists($filename)) {
            return array(realpath($filename));
        }
        $path = dirname($filename);
        $basename = basename($filename);
        $wildcard = sha1('fibonacci').md5('fibonacci');
        $pattern = '#^'.str_replace($wildcard, '.*?', preg_quote(str_replace('*', $wildcard, $basename))).'$#';
        $dir = Dir($path);
        $files = array();
        while(false !== ($entry = $dir->read())) {
            if ($entry[0] != '.') {
                if (preg_match($pattern, $entry)) {
                    $files[] = realpath($path.'/'.$entry);
                }
            }
        }
        return $files;
    }

    /**
     * @param string $path
     * @return string
     */
    public static function realpath(string $path): string
    {
        if (file_exists($path)) {
            return realpath($path);
        }
        return self::_realpath($path);
    }

    /**
     * @param string $cmd
     * @param string|null $pathEnv
     * @return string|false
     */
    public static function findInPath(string $cmd, ?string &$pathEnv = null)
    {
        if (self::isAbsoluteDir($cmd)) {
            if (file_exists($cmd)) {
                return realpath($cmd);
            }
        } else {
            $pathEnv = getenv('PATH');
            if (strncasecmp(PHP_OS, 'WIN', 3) === 0) {
                $paths = explode(';', $pathEnv);
            } else {
                $paths = explode(':', $pathEnv);
            }
            foreach($paths as $path) {
                if (file_exists($filename = $path.DIRECTORY_SEPARATOR.$cmd)) {
                    return realpath($filename);
                }
            }
        }
        return false;
    }

    /**
     * @param string $dir
     * @return bool
     */
    public static function isAbsoluteDir(string $dir): bool
    {
        return preg_match('#^([\\\\/]|[a-z]:)#i', $dir);
    }

    /**
     * @param string $dir
     * @param null|string $baseDir
     * @return string
     */
    public static function _realpath(string $dir, ?string $baseDir = null): string
    {
        if (!self::isAbsoluteDir($dir)) {
            $baseDir = $baseDir ?? static::$baseDir ?? getcwd();
            $dir = $baseDir.DIRECTORY_SEPARATOR.$dir;
        }
        $parts = preg_split('#[\\\\/]+#', $dir);
        $low = @$parts[0][1] === ':' || @$parts[0] === '' ? 1 : 0;

        $part = reset($parts);
        while($part !== false && key($parts) < $low) {
            $part = next($parts);
        }

        while($part !== false) { // false and '' mean end of the list
            if ($part === '..') {
                $key = key($parts);
                if ($key === $low) {
                    $parts[$key] = '.';
                    $part = next($parts);
                } else {
                    prev($parts);
                    $key2 = key($parts);
                    next($parts);
                    $part = next($parts);
                    if ($key2 === $low) {
                        $parts[$key2] = '.';
                    } else {
                        unset($parts[$key2]);
                    }
                    unset($parts[$key]);
                }
            } else {
                $part = next($parts);
            }
        }

        while(($key = array_search('.', $parts, true)) !== false) {
            unset($parts[$key]);
        }

        $dir = implode(DIRECTORY_SEPARATOR, $parts);
        if ($dir === '') {
            return DIRECTORY_SEPARATOR;
        }
        return $dir;
    }


}
