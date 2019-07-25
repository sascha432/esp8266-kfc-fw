<?php
/**
 * Author: sascha_lammers@gmx.de
 */

// TODO add multi line support for header files
// TODO strip comments outside strings

namespace ESPWebFramework;

/**
 * Class PlatformIOParser
 */
class PlatformIOParser {

    /**
     * @var array
     */
    private $defines;

    /**
     * @var string
     */
    private $environment;

    /**
     * [platformio] env_default
     *
     * @var string
     */
    private $defaultEnvironment;

    /**
     * @var array
     */
    private $environments;

    /**
     * Configuration per environment
     *
     * @var array
     */
    private $envConfig;

    public function __construct()
    {
        $this->defines = array();
        $this->environments = array();
    }

    public function dump(): void
    {
        foreach($this->defines as $name => $value) {
            echo $name.'='.$value."\n";
        }
    }

    /**
     * @param string|null $environment
     */
    public function setEnvironment(?string $environment): void
    {
        $this->environment = $environment;
    }

    /**
     * @return array
     */
    public function getEnvironments(): array
    {
        return $this->environments;
    }

    /**
     * @return array
     */
    public function getDefinitions(): array
    {
        return $this->defines;
    }

    /**
     * @param string $name
     * @return mixed
     */
    public function getDefinition(string $name)
    {
        if (!$this->isDefined($name)) {
            return null;
        }
        $str = $this->defines[$name];
        $json = @json_decode($str);
        if ($json !== null) { // works for "string", int, float, bool
            return $json;
        } else {
            if ($str === 'NULL' || $str === 'nullptr') {
                return null;
            }
            if (preg_match('/^0x([\da-f]+)$/i', $str, $matches)) {
                return hexdec($matches[1]);
            } else if (preg_match('/^0(\d)+$/', $str)) {
                return octdec($matches[1]);
            } else if (preg_match('/^0b([01]+)$/i', $str, $matches)) {
                return bindec($matches[1]);
            }
        }
        return $str;
    }

    /**
     * @param string $name
     * @return bool
     */
    public function isDefined(string $name): bool
    {
        return isset($this->defines[$name]);
    }

    /**
     * @param string $name
     */
    public function defineTrue(string $name): void
    {
        $this->defines[$name] = 1;
    }

    /**
     * @param string $name
     */
    public function defineFalse(string $name): void
    {
        $this->defines[$name] = 0;
    }

    /**
     * Store keywords
     *
     * @param string $keyword
     * @param string $line
     */
    private function processIniLine(?string $environment, ?string $keyword, string $line): void
    {
        if ($environment !== null && $keyword !== null) {
            $this->envConfig[$environment][$keyword] = $line;
            // echo "[$environment],$keyword:$line\n";
        }
    }

    /**
     * Resolve variables from the environment in $line
     *
     * @param string $line
     * @return string
     */
    private function resolveVariables(string $line): string
    {
        if (preg_match_all('/\$\{(?:([a-zA-Z0-9_-]+)\.)([a-zA-Z0-9_-]+)\}/', $line, $out)) {
            foreach($out[0] as $num => $var) {
                $env = $out[1][$num];
                $keyword = $out[2][$num];

                if (!isset($this->envConfig[$env][$keyword])) {
                    throw new \RuntimeException("Variable '$var' does not exist");
                }
                $line = str_replace($var, $this->envConfig[$env][$keyword], $line);
            }
        }
        return $line;
    }

    /**
     * Merge devault environment "env" into active environment
     *
     * @return void
     */
    private function mergeDefaultEnv(string $environment): void
    {
        $selectedEnv = &$this->envConfig[$environment];
        foreach($this->envConfig['env'] as $keyword => $value) {
            if (isset($selectedEnv[$keyword])) {
                $selectedEnv[$keyword] .= ' '.$value;
            } else {
                $selectedEnv[$keyword] = $value;
            }
        }

        if (isset($selectedEnv['build_flags'])) { // get all preprocessor defines

            $line = $this->resolveVariables($selectedEnv['build_flags']);

            // TODO there might be quoted strings with space
            $token = strtok($line, "\t ");
            while($token) {
                if ($token === '-D') {
                    $token = strtok("\t ");
                    if (!$token) {
                        break;
                    }
                    @list($name, $value) = explode('=', $token, 2);
                    $this->defines[$name] = $value;
                }
                $token = strtok("\t ");
            }
        }
    }

    /**
     * @param string $filename
     * @TODO add support for variables
     */
    public function readPlatformIOIni(string $filename): void
    {
        if (!file_exists($filename)) {
            throw new \RuntimeException("Cannot read $filename");
        }

        $section = $this->environment;
        $sectionFound = false;
        $environment = null;

        $keyword = null;
        $skip = $section !== null;
        $fullLine = '';

        foreach(file($filename, FILE_IGNORE_NEW_LINES) as $line) {

            $ch = substr(ltrim($line), 0, 1);
            if ($ch === '#' || $ch === ';') {
                continue; // line starts with a comment
            }
            //TODO trim comments

            if (preg_match('/^\s*\[([^\]]+)\]\s*$/', $line, $out)) {
                if ($keyword !== null) {
                    $this->processIniLine($environment, $keyword, $fullLine);
                }
                $keyword = null;
                $fullLine = '';

                $this->environments[] = $out[1];
                $environment = $out[1];
                if ($section !== null) {
                    $skip = ($out[1] !== $section);
                    if (!$skip) {
                        $sectionFound = true;
                    }
                }
            } else {
                if (preg_match('/^([a-zA-Z0-9_-]+)\s*=\s*(.*)/', $line, $out)) {
                    list(, $keyword, $fullLine) = $out;
                } else if ($keyword !== null) {
                    if (preg_match('/^\s/', $line)) { // line starts with white space
                        $fullLine .= $line;
                    } else { // new configuration keyword, process full line
                        $this->processIniLine($environment, $keyword, $fullLine);
                        $keyword = null;
                        $fullLine = '';
                    }
                }
            }

        }

        if ($keyword !== null) {
            $this->processIniLine($environment, $keyword, $fullLine);
        }

        if (isset($this->envConfig['platformio']) && isset($this->envConfig['platformio']['env_default'])) { // is a default environment configured?
            $this->defaultEnvironment = $this->envConfig['platformio']['env_default'];
        }


        if ($this->environment !== null) {
            if (!$sectionFound) {
                throw new \RuntimeException(sprintf('Environment %s not found in %s', $this->environment, $filename));
            }
            $this->mergeDefaultEnv($this->environment);
        }
    }

    /**
     * @param string $expr
     * @return bool
     */
    public function evaluateExpression(string $expr): bool
    {
        if (preg_match_all('/defined[ (]+?([^ )]*?)[ )]+/', $expr, $out)) {
            foreach($out[0] as $key => $value) {
                $expr = str_replace($value, isset($this->defines[$out[1][$key]]) ? 'true' : 'false', $expr);
            }
        }
        $expr = strtr($expr, $this->defines);

        $____result = false;
        eval('$____result = '.$expr.';');
        return $____result;
    }

    /**
     * @param string $filename
     */
    public function readDefinitionHeader(string $filename): void
    {
        if (!file_exists($filename)) {
            throw new \RuntimeException("Cannot read $filename");
        }

        $activeBlocks = array(true);
        $level = 0;
        $lineNo = 0;

        foreach(file($filename, FILE_IGNORE_NEW_LINES) as $line) {

            $lineNo++;
            $line = trim($line);

            $idx = strrpos($line, '//');
            if ($idx !== false) {
                $line = rtrim(substr($line, 0, $idx - 1)); //TODO doesn't check if it is inside a string
            }

            $line = trim($line);

            if (preg_match('/^#\s*if\s+(.*)/', $line, $out)) {
                if ($activeBlocks[$level]) {
                    $activeBlocks[++$level] = $activeBlocks[$level - 1] && $this->evaluateExpression($out[1]);
                } else {
                    $activeBlocks[++$level] = false;
                }
            } else if (preg_match('/^#\s*ifdef\s+(.*)/', $line, $out)) {
                if ($activeBlocks[$level]) {
                    $activeBlocks[++$level] = isset($this->defines[$out[1]]);
                } else {
                    $activeBlocks[++$level] = false;
                }
            } else if (preg_match('/^#\s*ifndef\s+(.*)/', $line, $out)) {
                if ($activeBlocks[$level]) {
                    $activeBlocks[++$level] = !isset($this->defines[$out[1]]);
                } else {
                    $activeBlocks[++$level] = false;
                }
            } else if (preg_match('/^#\s*elif\s+(.*)/', $line, $out)) {
                if ($activeBlocks[$level]) {
                    $activeBlocks[++$level] = $this->evaluateExpression($out[1]);
                } else {
                    $activeBlocks[++$level] = false;
                }
            } else if (preg_match('/^#\s*else$/', $line, $out)) {
                if ($activeBlocks[$level - 1]) {
                    $activeBlocks[$level] = !$activeBlocks[$level];
                }
            } else if (preg_match('/^#\s*endif$/', $line, $out)) {
                $activeBlocks[$level] = false;
                $level--;
                if ($level < 0) {
                    throw new RuntimeException(sprintf('ERROR: %s:%d: #endif without #if', basename($filename), $lineNo));
                }
            }
            if ($activeBlocks[$level]) {
                if (preg_match('/^#\s*define\s+([^ ]+)\s+(.*)/', $line, $out)) {
                    if (isset($this->defines[$out[1]])) {
                        echo sprintf("WARNING: %s:%d: $out[1] already defined\n", basename($filename), $lineNo);
                    } else {
                        $this->defines[$out[1]] = trim($out[2]);
                    }
                } else if (preg_match('/^#\s*undef\s*([\S]*)/', $line, $out)) {
                    if (isset($this->defines[$out[1]])) {
                        unset($this->defines[$out[1]]);
                    } else {
                        echo sprintf("WARNING: %s:%d: $out[1] is not defined\n", basename($filename), $lineNo);
                    }
                }
            }
        }
    }

}
