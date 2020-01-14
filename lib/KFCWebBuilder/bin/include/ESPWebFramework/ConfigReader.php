<?php
/**
 * Author: sascha_lammers@gmx.de
 */

namespace ESPWebFramework;

use ESPWebFramework\JsonConfiguration\JsonConfiguration;

/**
 * Class ConfigReader
 */
class ConfigReader {

    /**
     * @var string
     */
    private $configFile;
    /**
     * @var string
     */
    private $lastError;
    /**
     * @var array
     */
    private $dependencies;
    /**
     * @var PlatformIOParser
     */
    private $platformIOParser;
    /**
     * @var bool
     */
    private $isWindows;
    /**
     * @var string
     */
    private $shellEnvVarsRegEx;
    /**
     * @var string
     */
    private $homePath = '~';
    /**
     * @var array
     */
    private $variables;
    /**
     * @var JsonConfiguration
     */
    private $config;
    /**
     * @var object
     */
    private $rawJson;
    /**
     * @var string
     */
    private $sysTempDir;
    /**
     * @var string
     */
    private $processTempDir;
    /**
     * @var string
     */
    private $sharedTempDir;

    /**
     * @var bool
     */
    private $isCleanUpOnExit;

    /**
     * @return object
     */
    public function getRawJson(): object
    {
        return $this->rawJson;
    }

    /**
     * ConfigReader constructor.
     * @param string $_configFile
     */
    public function __construct (string $_configFile) {
        $this->configFile = FileUtils::realpath($_configFile);
        $this->platformIOParser = new PlatformIOParser();
        $this->isCleanUpOnExit = true;
    }

    /**
     * @return PlatformIOParser
     */
    public function getPlatformIOParser(): PlatformIOParser
    {
        return $this->platformIOParser;
    }

    /**
     * @return string
     */
    public function getLastError(): string
    {
        return $this->lastError;
    }

    /**
     * @return bool
     */
    public function read(): bool
    {
//        $this->dependencies = get_included_files();
        try {
            if ($this->readFile($this->configFile) === false) {
                return false;
            }
        } catch (\Exception $e) {
            $this->lastError = $e->getMessage();
            return false;
        }

        return true;
    }

    /**
     * @param string $val
     * @param array $from
     * @param array $to
     * @param string $name
     * @return null|string
     */
    private function resolveVariable(string $val, array $from, array $to, string $name): ?string
    {
//        echo "$name=$val\n";

        if (strpos($val, '~') === 0) {
            $val = $this->homePath.substr($val, 1);
        }
        do {
            $prevVal = $val;

            $val = preg_replace_callback($this->shellEnvVarsRegEx, function($matches) {
                if (($env = getenv($matches[1])) === false) {
                    return $matches[0];
                }
                return $env;
            }, preg_replace_callback('/\${env\.([^}]+)?}/', function($matches) {
                if (($env = getenv($matches[1])) === false) {
                    return $matches[0];
                }
                return $env;
            }, str_replace($from, $to, $val)));

        } while($prevVal !== $val);

        if (preg_match_all('/\${(?!(arg|file)\.)[^}]+?}/', $val, $matches)) {
            throw new \RuntimeException("$name=$val\nCannot resolve variable(s): ".implode(', ', array_column($matches, 0)));
        }

        return $val;
    }

    /**
     * @param array $path
     * @param string $name
     * @return string
     */
    private function buildPath(array $path, string $name): string
    {
        $str = '';
        $path[] = $name;
        foreach($path as $part) {
            if (is_numeric($part)) {
                $part = "[$part]";
            } else {
                if ($str !== '') {
                    $str .= '.';
                }
            }
            $str .= $part;
        }
        return $str;
    }

    /**
     * @param object|array $json
     * @param array $from
     * @param array $to
     * @param array $path
     */
    private function resolveVariables(&$json, array $from, array $to, $path = array()): void
    {
        foreach($json as $key => &$val) {

//            echo $key." ".gettype($val)." ".$this->buildPath($path, $key)."\n";

            if (is_array($val) || is_object($val)) {
                $path[] = $key;
                $this->resolveVariables($val, $from, $to, $path);
                array_pop($path);
            } else {
                $val = $this->resolveVariable($val, $from, $to, $this->buildPath($path, $key));
            }
        }
        unset($val);
    }

    /**
     * @param string $dirName
     * @param bool $self
     * @param string|null $filePattern
     * @return array
     */
    public static function recursiveDelete(string $dirName, bool $self = true, ?string $filePattern = null): array
    {
        $dirCount = 0;
        $fileCount = 0;
        $dir = dir($dirName);
        while(false !== ($entry = $dir->read())) {
            if ($entry !== '.' && $entry !== '..') {
                $fullPath = $dirName.DIRECTORY_SEPARATOR.$entry;
                if (is_dir($fullPath)) {
                    list($a, $b, ) = self::recursiveDelete($fullPath, true, $filePattern);
                    $dirCount += $a;
                    $fileCount += $b;
                    if (@rmdir($fullPath)) {
                        $dirCount++;
                    }
                } else if ($filePattern === null || preg_match($filePattern, $entry)) {
                    if (@unlink($fullPath)) {
                        $fileCount++;
                    }
                }
            }
        }
        $dir->close();
        $success = false;
        if ($self && @rmdir($dirName)) {
            $dirCount++;
            $success = true;
        }
        return array($dirCount, $fileCount, $success);
    }

    /**
     * @param string $filename
     * @return mixed
     * @throws \ReflectionException
     */
    private function readFile(string $filename) {

        mt_srand();

        $this->sysTempDir = sys_get_temp_dir();

        $this->sharedTempDir = $this->sysTempDir.DIRECTORY_SEPARATOR.'WebBuilderCli';
        if (@!mkdir($concurrentDirectory = $this->sharedTempDir, 0777, true) && !is_dir($concurrentDirectory)) {
            throw new \RuntimeException(sprintf("Cannot create temporary directory: '%s'", $this->sharedTempDir));
        }
        $this->processTempDir = $this->sharedTempDir.DIRECTORY_SEPARATOR.getmypid();

        do {
            $tmpDir = $this->processTempDir.'_'.sprintf('%08x', (mt_rand(0, 0xffff) << 16) | mt_rand(0, 0xffff));
        } while(file_exists($tmpDir));
        /** @noinspection MkdirRaceConditionInspection */
        if (!@mkdir($tmpDir, 0777, true) || !file_exists($tmpDir)) {
            throw new \RuntimeException(sprintf("Cannot create temporary directory: '%s'", $tmpDir));
        }
        $this->processTempDir = $tmpDir;

        $pioEnv = $this->getPlatformIOParser()->getEnvironment();
        if (false !== ($tmp = strchr($pioEnv, ':'))) {
            $pioEnv = substr($tmp, 1);
        }

        $vars = array(
            '${pio.env}' => $pioEnv,
            '${path.main_config}' => dirname($this->configFile),
            '${path.sys_temp_dir}' => $this->sysTempDir,
            '${path.shared_temp_dir}' => $this->sharedTempDir,
            '${path.process_temp_dir}' => $this->processTempDir,
            '${date.date}' => strftime('%D'),
            '${date.time}' => strftime('%T'),
            '${date.datetime}' => strftime('%DT%T'),
            '${date.unixtime}' => (string)time(),
        );

        register_shutdown_function(function() {
            if ($this->cleanUpOnExit()) {
                $result = self::recursiveDelete($this->getProcessTempDir());
                if ($result[1]) {
                    echo "Cleanup temp. dir, files: $result[1] dirs $result[0] success ".($result[2] ? 'true' : 'false')."\n";
                }
            }
        });

        $this->dependencies[] = $filename;

        $jsonReader = new JsonReader();

        if (($json = $jsonReader->load($filename)) === false) {
            $this->lastError = $jsonReader->getLastError();
            return false;
        }

        if (is_array($json->includes)) {
            foreach($json->includes as $dependency) {
                $this->dependencies[] = $dependency;
            }
        }

        $this->platformIOParser->readPlatformIOIni($json->platformio_ini);
        $this->platformIOParser->readDefinitionHeader($json->global_defines_header);

        foreach(array('OS_WIN', 'OS_OSX', 'OS_UNIX') as $name) {
            if ($this->platformIOParser->isDefined($name)) {
                throw new \RuntimeException(sprintf('%s already defined', $name));
            }
            $this->platformIOParser->defineFalse($name);
        }
        if (strncasecmp(PHP_OS, 'WIN', 3) === 0) {
            $this->isWindows = true;
            $this->platformIOParser->defineTrue('OS_WIN');
        } else if (strcasecmp(PHP_OS, 'DARWIN') === 0) {
            $this->platformIOParser->defineTrue('OS_OSX');
        } else {
            $this->platformIOParser->defineTrue('OS_UNIX');
        }

        if ($this->isWindows) {
            $this->shellEnvVarsRegEx = '/%([a-z#$\'()*+,\-.?@[\]_`{}~][a-z0-9#$\'()*+,\-.?@[\]_`{}~]*)%/i';
            $this->homePath = getenv('HOMEDRIVE').getenv('HOMEPATH');
        } else {
            $this->shellEnvVarsRegEx = '/\$(?:{)?([a-z0-9_]+)('.'?(1)\})/i';
            $this->homePath = getenv('HOME');
        }

        if (isset($json->web_builder->conditional_excludes)) {
            $json->web_builder->conditional_excludes = $this->applyConditions($json->web_builder->conditional_excludes);
        }

        $this->filterIfStatements($json);
        $this->variables = $vars;
        $this->escapeBin($json);
        $this->updateVariables($json);
        $this->resolveVariables($json, array_keys($this->variables), array_values($this->variables));

        if (isset($json->global_defines_header)) {
            $this->dependencies[] = $json->global_defines_header;
        }
        if (isset($json->platformio_ini)) {
            $this->dependencies[] = $json->platformio_ini;
        }

        $this->rawJson = $json;
        $this->config = $jsonReader->createClass($json);

        $this->dependencies = array_unique($this->dependencies);

        $this->isCleanUpOnExit = $this->config->getConfiguration()->getCleanupTempDirOnFailure();

        return true;
    }

    /**
     * @return JsonConfiguration
     */
    public function getConfiguration(): JsonConfiguration
    {
        return $this->config;
    }

    /**
     * @return array
     */
    public function getDependencies(): array
    {
        return $this->dependencies;
    }

    const UNESCAPED_STRING_MARKED = "\u0075__";

    /**
     * @throws \ReflectionException
     */
    private function escapeBin(&$json): void
    {
        foreach($json->bin as $key => &$bin) {
            if (strncmp($key, self::UNESCAPED_STRING_MARKED, strlen(self::UNESCAPED_STRING_MARKED)) === 0) {
                continue;
            }
            $file = $this->resolveVariable($bin, array(), array(), "bin.$key");
            if (!FileUtils::isAbsoluteDir($file)) {
                if (($result = FileUtils::findInPath($file, $pathEnv)) === false) {
                    throw new \RuntimeException(sprintf('Cannot find %s in current PATH (%s)', $file, $pathEnv));
                }
                $file = $result;
            }
            if (!file_exists($file)) {
                throw new \RuntimeException(sprintf('Cannot find %s', $file));
            }
            $path = FileUtils::realpath($file);
            $key = self::UNESCAPED_STRING_MARKED.$key;
            $json->bin->$key = $path;
            $bin = escapeshellarg($path);
        }
    }

    /**
     * @param object|array $json
     * @param int $level
     * @param array $path
     */
    private function updateVariables($json, int $level = 0, array $path = array()): void
    {
        $level++;
        if (is_array($json)) {
            foreach($json as $key => $item) {
                $this->updateVariables($item, $level, $path);
            }
        } else if (is_object($json)) {
            $isFlat = true;
            foreach($json as $key => $val) {
                if ($key === 'web_builder') {
                    // skip
                } else if (is_array($val) || is_object($val)) {
                    $path[] = $key;
                    $this->updateVariables($val, $level, $path);
                    array_pop($path);
                    $isFlat = false;
                }
            }
            if ($isFlat) {
                foreach($json as $key => $val) {
                    $this->variables['${'.implode('.', $path).'.'.$key.'}'] = $val;
                }
            }
        }
    }

    /**
     * @param array|object $items
     * @return array|mixed|null
     */
    private function evalIfStatements($items) {
        $results = array();
        $ifMatch = false;
        $default = null;
        $ifs = 0;
        foreach($items as $key => $val) {
            $line = trim($key);
            if (preg_match('/^#\s*if\s+(.*)/', $line, $out)) {
                $ifs++;
                if ($this->platformIOParser->evaluateExpression($out[1])) {
                    $results[] = $val;
                    $ifMatch = true;
                }
            } else if (preg_match('/^#\s*ifdef\s+(.*)/', $line, $out)) {
                $ifs++;
                if ($this->platformIOParser->isDefined($out[1])) {
                    $results[] = $val;
                    $ifMatch = true;
                }
            } else if (preg_match('/^#\s*ifndef\s+(.*)/', $line, $out)) {
                $ifs++;
                if (!$this->platformIOParser->isDefined($out[1])) {
                    $results[] = $val;
                    $ifMatch = true;
                }
            } else if (preg_match('/^#\s*elif\s+(.*)/', $line, $out)) {
                if (!$ifMatch && $this->platformIOParser->evaluateExpression($out[1])) {
                    $results[] = $val;
                    $ifMatch = true;
                }
            } else if (preg_match('/^#\s*else$/', $line, $out)) {
                if (!$ifMatch) {
                    $results[] = $val;
                }
            } else if (preg_match('/^#\s*default$/', $line, $out)) {
                $default = $val;
            }
            if ($default !== null && count($results) === 0) {
                $results[] = $default;
            }
        }
        if ($ifs > 1) {
            return $results;
        }
        if (count($results)) {
            return array_pop($results);
        }
        return null;
    }

    /**
     * @param array|object $json
     */
    private function filterIfStatements(&$json): void
    {
        $isFlat = true;
        foreach($json as $key => &$item) {
            if (is_array($item) || is_object($item)) {
                $this->filterIfStatements($item);
                $isFlat = false;
            }
        }
        unset($item);

        if ($isFlat) {
            $ifCounter = 0;
            $invalidIfs = array();
            foreach($json as $key => $item) {
                if (preg_match("/^#\s*(if|elif|else|default)/", trim($key))) {
                    $ifCounter++;
                } else {
                    $invalidIfs[] = $key;
                }
            }
            if ($ifCounter && count($invalidIfs)) {
                throw new \RuntimeException('Invalid #if statement(s): '.implode(',', $invalidIfs));
            }
            if ($ifCounter) {
                $json = $this->evalIfStatements($json);
            }
        }
        unset($item);
    }


    /**
     * @param array $list
     * @return array
     */
    private function applyConditions($list): array
    {
        $array = array();
        foreach($list as $condition => $val) {
            if (is_numeric($condition)) {
                $array[] = $val;
            } else if (preg_match('/^#\s*if\s+(.*)/', $condition, $matches)) {
                $result = $this->getPlatformioParser()->evaluateExpression($matches[1]);
                if ($result) {
                    if (is_array($val) || is_object($val)) {
                        /** @noinspection SlowArrayOperationsInLoopInspection */
                        $array = array_merge($array, $this->applyConditions($val));
                    } else {
                        $array[] = $val;
                    }
                }
            }
        }
        return $array;
    }

    /**
     * @return string
     */
    public function getSysTempDir(): string
    {
        return $this->sysTempDir;
    }

    /**
     * @return string
     */
    public function getProcessTempDir(): string
    {
        return $this->processTempDir;
    }

    /**
     * @return string
     */
    public function getSharedTempDir(): string
    {
        return $this->sharedTempDir;
    }

    /**
     * @return bool
     */
    public function cleanUpOnExit(): bool
    {
        return $this->isCleanUpOnExit;
    }

    /**
     * @param bool $isCleanUpOnExit
     */
    public function setCleanUpOnExit(bool $isCleanUpOnExit): void
    {
        $this->isCleanUpOnExit = $isCleanUpOnExit;
    }
}
