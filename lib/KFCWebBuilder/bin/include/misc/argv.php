<?php
/**
 * Author: sascha_lammers@gmx.de
 */

class ArgsParser {

    private $argv;
    private $help;
    private $options;
    private $allOptions;
    private $showUsageOnError;
    private $results;
    private $arguments;
    private $command;
    private $exitCode;

    /**
     * ArgsParser constructor.
     * @param string $argv
     */
    public function __construct($argv = null)
    {
        if (func_get_args() !== 1) {
            $argv = $GLOBALS['argv'];
            $this->command = basename($argv[0]);
            unset($argv[0]);
        }
        $this->argv = array_values($argv);
        $this->help = array(array('-h', '--help'), 'Display this help and exit');
        $this->allOptions = array();
        $this->options = array();
        $this->arguments = array();
        $this->showUsageOnError = false;
        $this->exitCode = 255;
    }

    /**
     * @param string $name
     */
    public function setCommand($name)
    {
        $this->command = $name;
    }

    /**
     * @param int $exitCode
     */
    public function setExitCode($exitCode)
    {
        $this->exitCode = $exitCode;
    }

    /**
     * @param array $options
     * @param array $all
     * @param array $help
     */
    private function checkIfExists($options, $all, $help = array())
    {
        $all[] = $help;
        $all = array_merge(...$all);
        foreach($options as $opt) {
            if (in_array($opt, $all, true)) {
                throw new \RuntimeException(sprintf('Option %s has been defined already. (%s)', $opt, implode('|', $all)));
            }
        }
    }

    /**
     * @param array $help
     * @param bool $showUsageOnError
     * @param null|string $long_help
     */
    public function setHelp($help, $showUsageOnError = false, $long_help = null)
    {
        $this->checkIfExists($help, $this->allOptions);
        $this->help = array($help, $long_help);
        $this->showUsageOnError = $showUsageOnError;
    }

    /**
     * @param string $name
     * @param array $options
     * @param null|bool $default
     * @param null|string $long_help
     */
    public function addBoolOption($name, $options, $default = false, $long_help = null)
    {
        $this->addOption($name, $options, $default, null, $long_help, 'bool');
    }

    /**
     * @param string $name
     * @param array $options
     * @param array $default
     * @param null|string $help
     * @param null|string $long_help
     */
    public function addMultiOption($name, $options, $default = null, $help = null, $long_help = null)
    {
        $this->addOption($name, $options, $default, $help, $long_help, true);
    }

    /**
     * @param string $name
     * @param array $options
     * @param string $default
     * @param null|string $help
     * @param null|string $long_help
     * @param bool|string $type
     */
    public function addOption($name, $options, $default = null, $help = null, $long_help = null, $type = false)
    {
        $this->checkIfExists($options, $this->allOptions, $this->help[0]);
        $this->allOptions[] = $options;
        $this->options[] = array($name, $options, $type, $default, $help, $long_help);
    }

    /**
     * @param string $name
     * @param int $count
     * @param null|string $help
     * @param null|string $long_help
     */
    public function addArgument($name, $count = 1, $help = null, $long_help = null)
    {
        $this->arguments[] = array($name, $count, $help, $long_help);
    }

    /**
     * @param string $message
     */
    private function failure($message)
    {
        echo $message."\n";
        if ($this->showUsageOnError) {
            $this->showUsage();
        } else if ($this->help[0]) {
            echo 'Use '.implode('|', $this->help[0])." to display usage.\n";
        }
        exit($this->exitCode);
    }

    private function showUsage()
    {
        $usage = array(
            $this->command
        );
        $longHelp = array();
        foreach($this->arguments as list($name, $count, $help, $long_help)) {
            if ($long_help) {
                $longHelp[] = array($name, null, null, null, $help, $long_help);
            }
            if ($count === -1) {
                $usage[] = '<'.($help ?? $name).'>...';
            } else {
                while($count--) {
                    $usage[] = '<'.($help ?? $name).'>';
                }
            }
        }

        foreach($this->options as list($name, $options, $type, $default, $help, $long_help)) {
            if ($long_help) {
                $longHelp[] = array($name, $options, $type, $default, $help, $long_help);
            }
            $str = '['.implode('|', $options);
            if (is_bool($type)) {
                $str .= ' <'.($help ?? $name).'>';
            }
            $str .= ']';
            if ($type === true) {
                $str .= '...';
            }
            $usage[] = $str;
        }

        echo 'Usage: '.implode(' ', $usage)."\n";
        if ($longHelp) {
            echo "\n";
            foreach($longHelp as list($name, $options, $type, $default, $help, $long_help)) {
                if ($long_help) {
                    $help = $help ?? $name;
                    if ($options === null) {
                        $options = array('<'.$help.'>');
                    } else {
                        if ($type !== 'bool') {
                            $options[count($options) - 1] = $options[count($options) - 1] .= ' <'.$help.(is_scalar($default) ? '='.$default : '').'>';
                        }
                    }
                    $opts = '  '.implode(', ', $options);
                    if (strlen($opts) > 23) {
                        echo $opts."\n";
                        $opts = '';
                    }
                    foreach(explode("\n", wordwrap($long_help, 78 - 24, "\n", true)) as $line) {
                        echo str_pad($opts, 24).$line."\n";
                        $opts = '';
                    }
                }
            }
            echo "\n";
        }
        exit($this->exitCode);
    }

    /**
     * @param string $name
     * @return bool
     */
    public function hasOption($name)
    {
        return isset($this->results[$name]);
    }

    /**
     * @param string $name
     * @return bool
     */
    public function isTrue($name)
    {
        return isset($this->results[$name]) && $this->results[$name] === true;
    }

    /**
     * @param string $name
     * @return bool
     */
    public function isFalse($name)
    {
        return !$this->isTrue($name);
    }

    /**
     * @param string $name
     * @return mixed
     */
    public function getOption($name)
    {
        if (isset($this->results[$name])) {
            return $this->results[$name];
        }
        return null;
    }

    /**
     * @param string $name
     * @return string|array|null
     */
    public function getArgument($name)
    {
        return $this->getOption($name);
    }

    /**
     * @return array
     */
    public function getResults()
    {
        return $this->results;
    }

    /**
     * @param bool|null $complainAboutUnknown false ignores unknown options and returns them as arguments, null removes them silently
     * @return array
     */
    public function parse($complainAboutUnknown = true)
    {
        $this->results = array();

        $stopParsing = null;
        if (($key = array_search('--', $this->argv, true)) !== false) {
            $stopParsing = array_slice($this->argv, $key + 1);
            $this->argv = array_slice($this->argv, 0, $key);
        }

        array_unshift($this->options, array('show_usage', $this->help[0], 'bool', false, null, $this->help[1]));

        foreach($this->options as list($name, $options, $type, $default, )) {
            $result = null;
            if ($type === 'bool') {
                $old_count = count($this->argv);
                $this->argv = array_diff($this->argv, $options);
                if (count($this->argv) !== $old_count) {
                    $result = true;
                    $this->argv = array_values($this->argv);
                }
            } else {
                foreach($options as $val) {
                    while (($key = array_search($val, $this->argv, true)) !== false) {
                        unset($this->argv[$key]);
                        if (!isset($this->argv[$key + 1])) {
                            $this->failure(implode('|', $options).' requires a value');
                        }
                        if ($type === true) {
                            $result[] = $this->argv[$key + 1];
                        } else {
                            if ($result !== null) {
                                $this->failure(implode('|', $options).' can only be specified once');
                            }
                            $result = $this->argv[$key + 1];
                        }
                        unset($this->argv[$key + 1]);
                    }
                    $this->argv = array_values($this->argv);
                }
            }
            $this->results[$name] = $result ?? $default;
        }

        if ($complainAboutUnknown === null || $complainAboutUnknown === true) {
            foreach($this->argv as $key => $arg) {
                if ($arg[0] === '-') {
                    if ($complainAboutUnknown === true) {
                        $this->failure("Unknown option $arg");
                    }
                    unset($this->argv[$key]);
                }
            }
        }

        if ($this->results['show_usage'] === true) {
            $this->showUsage();
        }

        if ($stopParsing) {
            $this->argv = array_merge($this->argv, $stopParsing);
        }
        foreach($this->arguments as list($name, $count, )) {
            if (($_count = $count) !== 1) {
                $this->results[$name] = array();
            }
            while($count === -1 || $count--) {
                if (!count($this->argv)) {
                    if ($count === -1) {
                        break;
                    }
                    $this->failure('Too few arguments');
                }
                if ($_count !== 1) {
                    $this->results[$name][] = array_shift($this->argv);
                } else {
                    $this->results[$name] = array_shift($this->argv);
                }
            }
        }
        if (count($this->argv)) {
            $this->failure('Too many arguments');
        }
        return $this->results;
    }
}
