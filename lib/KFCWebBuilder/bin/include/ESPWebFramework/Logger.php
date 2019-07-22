<?php
/**
 * Author: sascha_lammers@gmx.de
 */

namespace ESPWebFramework;

/**
 * Class Logger
 * @package ESPWebFramework
 */
class Logger
{
    public const LOG_LEVEL_DEBUG    = 100;
    public const LOG_LEVEL_NOTICE   = 200;
    public const LOG_LEVEL_WARNING  = 300;
    public const LOG_LEVEL_ERROR    = 400;

    public const LOG_LEVEL_2_STR = array( 100 => 'DEBUG', 200 => 'NOTICE', 300 => 'WARNING', 400 => 'ERROR');

    /**
     * @var bool
     */
    protected $verbose;
    /**
     * @var string
     */
    private $logFile;
    /**
     * @var int
     */
    private $logLevel;

    /**
     * @param int $logLevel
     * @param string $message
     * @param array $data
     */
    public function log(int $logLevel, string $message, array $data = array()): void
    {
        if (isset($this->logFile) && $logLevel >= $this->logLevel) {
            $json = array();
            foreach($data as $key => $obj) {
                if (method_exists($obj, 'toString')) {
                    $json[$key] = $obj->toString();
                } else {
                    $json[$key] = $obj;
                }
            }
            $message = strftime('%FT%TZ').' ['.self::LOG_LEVEL_2_STR[$logLevel].'] '.$message.($json ? ' '.json_encode($json) : '')."\n";
            if ($this->verbose) {
                echo $message;
            }
            file_put_contents($this->logFile, $message, FILE_APPEND);
        }
    }

    /**
     * @param string $logFile
     * @param int $logLevel
     */
    public function setLogFile(string $logFile, int $logLevel): void
    {
        $this->logFile = $logFile;
        $this->logLevel = $logLevel;
    }

    /**
     * @return bool
     */
    public function isVerbose(): bool
    {
        return $this->verbose;
    }

    /**
     * @param bool $verbose
     */
    public function setVerbose(bool $verbose): void
    {
        if ($verbose) {
            echo "Verbose mode on\n";
        }
        $this->verbose = $verbose;
    }
}
