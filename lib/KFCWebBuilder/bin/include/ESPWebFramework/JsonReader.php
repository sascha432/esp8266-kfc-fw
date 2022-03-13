<?php
/**
 * Author: sascha_lammers@gmx.de
 */

namespace ESPWebFramework;

use ESPWebFramework\JsonConfiguration\Converter\ConverterInterface;
use ESPWebFramework\JsonConfiguration\JsonConfiguration;
use ESPWebFramework\JsonConfiguration\Type\TypeInterface;

/**
 * Class JsonReader
 * @package ESPWebFramework
 */
class JsonReader {

    /**
     * @var string
     */
    private $lastError;
    /**
     * @var array
     */
    private $includes;
    /**
     * @var array
     */
    private $path;

    /**
     * @param string $file
     * @param bool $isInclude
     * @return object|false
     */
    public function load(string $file, bool $isInclude = false) {

        error_clear_last();
        $this->lastError = null;
        if ($isInclude) {
            $this->includes = array();
        }

        if (($contents = @file_get_contents($file)) === false) {
            $this->lastError = error_get_last()['message'];
            return false;
        }
        $contents = preg_replace('/^[\s]*(\/\/|[#;]).*$/m', '', $contents);

        if (($json = @json_decode($contents)) === null) {
            $this->lastError = "File: $file\nJSON Error: ".json_last_error_msg();
            return false;
        }

        $currentDir = dirname($file);
        FileUtils::setBaseDir($currentDir);
        $json = self::normalizeConfiguration($json, $currentDir);

        if (isset($json->includes)) {

            foreach($json->includes as $filename) {
                $filename = self::absoluteDir($filename, $currentDir);

                if (($includeJson = $this->load($filename, true)) === false) {
                    return false;
                }
                $this->includes[] = $filename;

                $newCurrentDir = dirname($file);
                FileUtils::setBaseDir($newCurrentDir);
                $includeJson = self::normalizeConfiguration($includeJson, $newCurrentDir);

                $json = $this->mergeInclude($json, $includeJson);
            }
        }

        if (!$isInclude) {
            $json->includes = $this->includes;
        }
        FileUtils::setBaseDir($currentDir);

        return $json;
    }

    /**
     * @param object $json
     * @param array $includeJson
     * @return object
     */
    private function mergeInclude($json, $includeJson) {
        foreach($includeJson as $key => $val) {
            if (!isset($json->$key)) {
                $json->$key = $val;
            } else if (in_array($key, array('bin', 'commands', 'post_process', 'configuration', 'web_builder'), true)) {
                $section = $json->$key;
                foreach($val as $key2 => $val2) {
                    if ($key === 'configuration' && $key2 === 'set_env') {
                        $section->$key2 = (object)array_merge(isset($section->$key2) && is_array($section->$key2) ? $section->$key2 : array(), $val2);
                    } else
                        {
                        if (!isset($section->$key2)) {
                            $section->$key2 = $val2;
                        }
                    }
                }
                unset($section);
            }
        }
        return $json;
    }

    /**
     * @param string $str
     * @return string
     */
    private function camelCase2Snake(string $str) :string
    {
        return preg_replace_callback('/([A-Z])/', function($matches) {
            return '_'.strtolower($matches[1]);
        }, $str);
    }

    /**
     * @param string $parentName
     * @param string $name
     * @return string
     */
    private function buildPath(?string $parentName, string $name): string
    {
        $path = $this->path;
        if ($parentName) {
            $path[] = $parentName;
        }
        $path[] = $name;
        return implode('.', $path);
    }

    /**
     * @param object $json
     * @param string $className
     * @param object|null $parent
     * @param string|null $parentName
     * @return mixed
     * @throws \ReflectionException
     */
    public function createClassFrom($json, string $className, ?object $parent = null, ?string $parentName = null) {

        $class = new $className();
        if ($class instanceof TypeInterface) {
            $class->setParent($parent, $parentName);
            $class->setValue($json);
        } else {
            if ($class instanceof ConverterInterface) {
                $json = $class->convertToValue($json);
            }
            $reflection = new \ReflectionClass($class);
            $properties = $reflection->getProperties();
            foreach($properties as $property) {
                $name = $property->getName();
                $_name = $this->camelCase2Snake($name);
                $value = $value = $json->$_name ?? null;

                if ($value !== null) {
                    if (preg_match_all('/@validator\s+([^\s]+)/', $property->getDocComment(), $matches)) {
                        foreach($matches[1] as $validator) {
                            $_validator = $validator;
                            if (strpos($validator, ':') !== false) {
                                $_validator = preg_split('/:+/', $validator);
                            }
                            if (!($result = call_user_func($_validator, $value))) {
                                throw new \RuntimeException(sprintf('%s: Validation(%s) of %s failed', $this->buildPath($parentName, $_name), $validator, $value));
                            }
                            if (is_string($result)) {
                                $value = $result;
                            }
                        }
                    }

                    $property->setAccessible(true);
                    // matches $1 class name, optional $2 [] after the name
                    if (preg_match('/@var[^@]+(\\\\'.__NAMESPACE__.'\\\\[^|\s\[*]+)\s*(\[\s*\])?/', $property->getDocComment(), $matches)) {
                        $className = $matches[1];
                        if (!@class_exists($className, true)) {
                            throw new \RuntimeException(sprintf("Cannot find class '%s'", $className));
                        }

                        if (isset($matches[2])) {
                            $array = array();
                            foreach($value as $key => $obj) {
                                if (isset($obj->name)) {
                                    $key = "'".$obj->name."'";
                                }
                                $this->path[] = '['.$key.']';
                                $array[] = $this->createClassFrom($obj, $className, $class, $_name);
                                array_pop($this->path);
                            }
                            $val = $array;
                        } else {
                            $val = $this->createClassFrom($value, $className, $class, $_name);
                        }
                        array_pop($this->path);
                        $property->setValue($class, $val);
                    } else {
                        $property->setValue($class, $value);
                    }
                }
            }
        }
        return $class;
    }

    /**
     * @param object $json
     * @return \ESPWebFramework\JsonConfiguration\JsonConfiguration
     * @throws \ReflectionException
     */
    public function createClass($json): JsonConfiguration
    {
        $this->path = array();
        try {
            return $this->createClassFrom($json, JsonConfiguration::class);
        } catch (\RuntimeException $e) {
            throw new \RuntimeException(implode('.', $this->path).":\n".$e->getMessage());
        }
    }

    /**
     * @return string
     */
    public function getLastError(): string
    {
        return $this->lastError;
    }

    /**
     * @param object $json
     * @param string|null $baseDir
     * @return object
     */
    public static function normalizeConfiguration($json, ?string $baseDir = null) {
        if (isset($json->web_builder->groups) && is_object($json->web_builder->groups)) {
            $groupArray = array();
            foreach($json->web_builder->groups as $name => $val) {
                $groupArray[] =  (object)(array(
                        'name' => $name,
                    ) + (array)$val);
            }
            if (count($groupArray)) {
                foreach($groupArray as &$group) {
                    if (isset($group->processors) && (is_object($group->processors) || is_array($group->processors))) {
                        $processorsArray= array();
                        foreach($group->processors as $name => $val) {
                            $processorsArray[] = (object)(array(
                                    'name' => $name,
                                ) + (array)$val);
                        }
                        $group->processors = $processorsArray;
                    }
                    if (isset($group->filters) && (is_object($group->filters) || is_array($group->filters))) {
                        $filtersArray = array();
                        foreach($group->filters as $name => $val) {
                            $filtersArray[] = (object)(array(
                                    'name' => $name,
                                ) + (array)$val);
                        }
                        $group->filters = $filtersArray;
                    }
                    if (isset($group->target_dirs)) {
                        $targetDirsArray = array();
                        foreach($group->target_dirs as $key => $val) {
                            $array = array(
                                'target_dir' => $key,
                                'source_files' => array(),
                                'source_dirs' => array(),
                            );
                            if (is_string($val)) {
                                $val = array($val);
                            }
                            if (is_array($val)) {
                                foreach($val as $key2 => $val2) {
                                    $array['source_files'][] = (object)array(
                                        'source_file' => $val2,
                                    );
                                }
                            }
                            if (is_object($val)) {
                                foreach($val as $key2 => $val2) {
                                    $array['source_dirs'][] = (object)array(
                                        'source_dir' => $key2,
                                        'pattern' => $val2,
                                    );
                                }
                            }
                            $array = @array_filter($array, 'count');
                            // $array = @array_filter($array);

                            $targetDirsArray[] = (object)$array;
                        }
                        $group->target_dirs = $targetDirsArray;
                    }
                }
                unset($group);
            }
            $json->web_builder->groups = $groupArray;

        }
        if ($baseDir !== null) {
            self::replaceStaticVars($json, $baseDir);

            if (isset($json->platformio_ini)) {
                self::applyBaseDir($json->platformio_ini, $baseDir);
            }
            if (isset($json->global_defines_header)) {
                self::applyBaseDir($json->global_defines_header, $baseDir);
            }
            if (isset($json->web_builder->conditional_excludes)) {
                self::recursiveCallback($json->web_builder->conditional_excludes, function($key, $val) use($baseDir) {
                    return self::absoluteDir($val, $baseDir);
                });
            }
            if (isset($json->configuration->set_env) && is_object($json->configuration->set_env)) {
                $array = array();
                foreach($json->configuration->set_env as $key => $val) {
                    $array[] = (object)array("name" => $key, "value" => $val);
                }
                $json->configuration->set_env = $array;
            }
            if (isset($json->web_builder->groups)) {
                foreach($json->web_builder->groups as &$group) {
                    if (isset($group->target_dirs)) {
                        foreach($group->target_dirs as &$targetDirs) {
                            if (isset($targetDirs->target_dir)) {
                                self::applyBaseDir($targetDirs->target_dir, $baseDir);
                            }
                            if (isset($targetDirs->source_dirs)) {
                                foreach($targetDirs->source_dirs as &$sourceDirs) {
                                    if (isset($sourceDirs->source_dir)) {
                                        self::applyBaseDir($sourceDirs->source_dir, $baseDir);
                                    }
                                }
                                unset($sourceDirs);
                            }
                            if (isset($targetDirs->source_files)) {
                                foreach($targetDirs->source_files as &$sourceFiles) {
                                    if (isset($sourceFiles->source_file)) {
                                        self::applyBaseDir($sourceFiles->source_file, $baseDir);
                                    }
                                }
                                unset($sourceFiles);
                            }
                        }
                        unset($targetDirs);
                    }
                }
            }
            unset($group);
        }
        return json_decode(json_encode((object)$json));
    }

    /**
     * @param object|array $json
     * @param string $baseDir
     */
    private static function replaceStaticVars(&$json, string $baseDir): void
    {
        foreach($json as $key => &$val) {
//            echo "replaceStaticVars $key\n";
            if (is_array($val) || is_object($val)) {
                self::replaceStaticVars($val, $baseDir);
            } else if (false !== strpos($val, '${path.current_config}')) {
                $val = FileUtils::realpath(str_replace('${path.current_config}', $baseDir, $val));
            }
        }
        unset($val);
    }

    /**
     * @param array|object $items
     * @param callable $callback
     */
    private static function recursiveCallback(&$items, callable $callback): void
    {
        foreach($items as $key => &$val) {
            if (is_array($val) || is_object($val)) {
                self::recursiveCallback($val, $callback);
            } else {
                $val = $callback($key, $val);
            }
        }
        unset($val);
    }

    /**
     * @param string $dir
     * @param string $baseDir
     */
    private static function applyBaseDir(string &$dir, string $baseDir): void
    {
        $dir = self::absoluteDir($dir, $baseDir);
    }

    /**
     * @param string $dir
     * @param string $baseDir
     * @return string
     */
    private static function absoluteDir(string $dir, string $baseDir): string
    {
        if (!preg_match('/^(\\/\\\\|[a-z]:|\${[^}]+})|%[^%]+%/i', $dir)) {
            return FileUtils::realpath($baseDir.'/'.$dir);
        }
        return $dir;
    }

}
