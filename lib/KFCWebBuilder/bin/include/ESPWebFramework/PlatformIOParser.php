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
     * Lines of the configuration file
     *
     * @var array
     */
    private $lines;

    /**
     * Directory of the .ini file
     *
     * @var array
     */
    private $ini_base_dir;

    /**
     * Configuration per environment
     *
     * @var array
     */
    private $envConfig;

    /**
     * stores last $line, $keyword on the end of thze line
     *
     * @var array
     */
    private $lastLine = [];

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
     * @return string|null
     */
    public function getEnvironment(): string
    {
        return $this->environment;
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
     * Add all lines from the file to $this->lines
     *
     * @param string $filename
     */
    public function append_ini_file(string $filename): void
    {
        if (!file_exists($filename)) {
            throw new \RuntimeException("Cannot load extra config '".$filename."'");
        }
        foreach(file($filename, FILE_IGNORE_NEW_LINES) as $line) {
            $this->lines[] = $line;
        }
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
            if ($keyword == 'extra_configs') {

                // load extra configs and append at the end
                $extra_files = preg_split("/\s+/", $line, -1, PREG_SPLIT_NO_EMPTY);
                foreach($extra_files as $file) {
                    if ($file{0} != '/') {
                        $file = $this->ini_base_dir.'/'.$file;
                        if (strchr($file, '*')) {
                            foreach(glob($file) as $filename) {
                                $this->append_ini_file($filename);
                            }
                        }
                        else {
                            $this->append_ini_file($file);
                        }
                    }
                }

            }
            else if ($keyword == 'extends') {
                $line = trim($line);
                if (!isset($this->envConfig[$line])) {
                    throw new \RuntimeException("extends missing environment '".$line."'");
                }
                $this->mergeEnvironments($environment, $line, false);
            }
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
    private function resolveVariables(string $lineIn): string
    {
        $line = $lineIn;
        while (preg_match_all('/\$\{(?:([a-zA-Z0-9_:-]+)\.)([a-zA-Z0-9_-]+)\}/', $line, $out)) {

            foreach($out[0] as $num => $var) {
                $env = $out[1][$num];
                $keyword = $out[2][$num];
                $value = null;

                if (!isset($this->envConfig[$env][$keyword])) {
                    if ($env == "sysenv") {
                        $value = getenv($keyword);
                    }
                }
                else {
                    $value = $this->envConfig[$env][$keyword];
                }

                if ($value === null) {
                    throw new \RuntimeException("Variable '$var' does not exist");
                }
                $line = str_replace($var, $value, $line);
            }
        }
        return $line;
    }

    /**
     * Strip comments from the line
     *
     * @param string $line
     * @param boolean $cStyle
     * @return string
     */
    private function stripComments(string $line, bool $cStyle): string
    {
        if ($cStyle) { // "//" and "/* */"

            $line = preg_replace_callback('/(".*?")|\/\*[\s\S]*?\*\/|([^:]|^)\/\/.*$/', function($matches) {
                if ($matches[0]{0} == '"') {
                    return $matches[0];
                }
                return @$matches[2];
            }, $line);

        } else { // "#", ";"

            $line = preg_replace_callback('/(".*?")|[#;].*$/', function($matches) {
                if ($matches[0]{0} == '"') {
                    return $matches[0];
                }
                return '';
            }, $line);

        }
        return $line;
    }

    /**
     * Merge 2 environments
     * *
     * @param string $environment
     * @param string $sourceEnv
     * @param bool $parseBuildFlags
     *
     * @return void
     */
    private function mergeEnvironments(string $environment, string $sourceEnv, bool $parseBuildFlags): void
    {
        //echo "mergeEnvironments: $sourceEnv => $environment\n";
        $selectedEnv = &$this->envConfig[$environment];
        foreach($this->envConfig[$sourceEnv] as $keyword => $value) { // copy config from default environment if it does not exist
            if (!isset($selectedEnv[$keyword])) {
                $selectedEnv[$keyword] = $value;
            }
        }

        if ($parseBuildFlags && isset($selectedEnv['build_flags'])) { // get all preprocessor defines

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

        $this->ini_base_dir = dirname($filename);
        $this->lines = [];
        foreach(file($filename, FILE_IGNORE_NEW_LINES) as $line) {
            $this->lines[] = $line;
        }

        // use a for loop since lines will be added in the loop
        for($line_num = 0; $line_num < count($this->lines); $line_num++) {

            $line = $this->lines[$line_num];

            $ch = substr(ltrim($line), 0, 1);
            if ($ch === '#' || $ch === ';') {
                continue; // line starts with a comment
            }
            $line = $this->stripComments($line, false);

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
                if (preg_match('/^([a-zA-Z0-9_.-]+)\s*=\s*(.*)/', $line, $out)) {

                    if ($keyword !== null) {
                        $this->processIniLine($environment, $keyword, $fullLine);
                    }
                    // new keyword
                    list(, $keyword, $fullLine) = $out;

                } else if ($keyword !== null) {
                    if (preg_match('/^\s/', $line)) { // line starts with white space
                        $fullLine .= $line;
                    } else { // new configuration keyword, process full line

                        $this->processIniLine($environment, $keyword, $fullLine);
                        $keyword = null;
                        $fullLine = '';
                        $this->lastLine = [$line_num, $keyword, $environment];
                    }
                }
                else {
                    $line = trim($line);
                    if ($line != "") {
                        echo "WARNING! '$line' skipped\n";
                        echo "line=".$this->lastLine[0]." keyword=".$this->lastLine[1]." env=".$this->lastLine[2]."\n";
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
            $this->mergeEnvironments($this->environment, 'env', true);
        }
    }

    // function resolveExtends($env) {
    //     echo "resolveExtends\n";
    //     var_dump($this->envConfig[$env]);
    //     if (isset($this->envConfig[$env]['extends'])) {
    //         $extends = $this->envConfig[$env]['extends'];
    //         echo "resolveExtends $extends\n";
    //         if (!isset($this->envConfig[$extends])) {
    //             throw new \RuntimeException("Cannot find extends=$extends");
    //         }
    //         //$this->resolveExtends($extends);

    //     }
    //     exit;
    // }

    // error handler to collect undefined constants
    private $undefinedConstants;

    function evaluateExpressionErrorHandler($errno, $errstr, $errfile, $errline)
    {
        if ($errno == E_WARNING) {
            if (strstr($errstr, 'Use of undefined constant')) {
                if (preg_match('/- assumed \'(.*?)\' \(this will throw an Error in a future version of PHP\)/', $errstr, $matches)) {
                    $this->undefinedConstants[] = $matches[1];
                }
            }
        }
        return true;
    }

    /**
     * Resolve preprocessor macros with eval()
     *
     * @param string $expr
     * @return bool
     */
    public function evaluateExpression(string $exprIn): bool
    {
        $expr = $exprIn;
        if (preg_match_all('/defined[ (]+?([^ )]*?)[ )]+/', $expr, $out)) {
            foreach($out[0] as $key => $value) {
                $expr = str_replace($value, isset($this->defines[$out[1][$key]]) ? 'true' : 'false', $expr);
            }
        }
        // foreach($this->defines as $define => $value) {
        //     $define = preg_quote($define);
        //     $expr = preg_replace("/(?<![a-zA-Z0-9_-])$define(?![a-zA-Z0-9_-])/", $value, $expr);
        // }

        $expr = $this->stripComments($expr, true);

        // run expression until all undefined constants have been resolved
        // $resolved = array();
        $____old_error_handler = set_error_handler(array($this, "evaluateExpressionErrorHandler"));
        while(true) {
            $this->undefinedConstants = array();
            $____result = false;
            @eval('$____result = '.$expr.';');
            if (sizeof($this->undefinedConstants) == 0) {
                break;
            }

            // sort defines by length of the name
            usort($this->undefinedConstants, function($a, $b) {
                return strlen($b) - strlen($a);
            });

            foreach($this->undefinedConstants as $name) {
                $expr = str_replace($name, isset($this->defines[$name]) ? $this->defines[$name] : 'false', $expr);
                // $resolved[] = $name.'='.(isset($this->defines[$name]) ? $this->defines[$name] : '*UNDEF*');
            }
        }
        set_error_handler($____old_error_handler);

        // echo "$exprIn = $expr = ".($____result ? "true" : "false")." RESOLVED ".implode(',', $resolved)."\n";

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
