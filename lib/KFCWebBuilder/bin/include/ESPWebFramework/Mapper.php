<?php
/**
 * Author: sascha_lammers@gmx.de
 */

namespace ESPWebFramework;

use ESPWebFramework\Processor\File;

/**
 * Class File
 * @package ESPWebFramework
 */
class Mapper implements PluginInterface
{
    private const FLAGS_GZIPPED = 0x01;

    /**
     * @var Mapper
     */
    private static $instance;
    /**
     * @var string
     */
    private $dataDir;
    /**
     * @var string
     */
    private $webDir;
    /**
     * @var string
     */
    private $mappingsFile;
    /**
     * @var array
     */
    private $mappedFiles;
    /**
     * @var bool
     */
    private $verbose;
    /**
     * @var int
     */
    private $counterTypeSize;
    /**
     * @var array
     */
    private $flags;
    // /**
    //  * @var callable
    //  */
    // private $hashFunction;

    /**
     * @param WebBuilder $webBuilder
     * @return Mapper
     */
    public static function getInstance(WebBuilder $webBuilder): Mapper
    {
        if (!self::$instance) {
            self::$instance = new self($webBuilder);
        }
        return self::$instance;
    }

    /**
     * Mapper constructor.
     * @param WebBuilder $webBuilder
     */
    public function __construct(WebBuilder $webBuilder)
    {
        $this->mappedFiles = array();
        $spiffs = $webBuilder->getConfig()->getSpiffs();
        $this->dataDir = $spiffs->getDataDir();
        $this->webDir = $spiffs->getWebTargetDir();
        $this->mappingsFile = $spiffs->getMappingsFile();
        $this->verbose = $webBuilder->isVerbose();

        // if (($hashAlgorithm = $webBuilder->getConfigReader()->getPlatformIOParser()->getDefinition('FS_MAPPINGS_HASH_ALGO')) === null) {
        //     throw new \RuntimeException('Constant FS_MAPPINGS_HASH_ALGO is not defined');
        // }
        // $this->hashFunction = $hashAlgorithm.'_file';
        // $funcName = $this->hashFunction.'()';
        // if (!function_exists($this->hashFunction)) {
        //     $funcName = "hash_file('$hashAlgorithm', ...)";
        //     $this->hashFunction = function($filename, $raw_output) use($hashAlgorithm) {
        //         return hash_file($hashAlgorithm, $filename, $raw_output);
        //     };
        //     if (in_array($hashAlgorithm, hash_algos(), true)) {
        //         throw new \RuntimeException(sprintf('FS_MAPPINGS_HASH_ALGO %s is not supported', $hashAlgorithm));

        //     }
        // }
        // if (($hashLength = (int)$webBuilder->getConfigReader()->getPlatformIOParser()->getDefinition('FS_MAPPINGS_HASH_LENGTH')) === null) {
        //     throw new \RuntimeException('Constant FS_MAPPINGS_HASH_LENGTH is not defined');
        // }

        // if (($len = strlen(call_user_func($this->hashFunction, __FILE__, true))) !== $hashLength) {
        //     throw new \RuntimeException(sprintf('FS_MAPPINGS_HASH_LENGTH %d does not match the output of %s = %d', $hashLength, $funcName, $len));
        // }

        $this->flags = array();
        if (($gzippedFlag = $webBuilder->getConfigReader()->getPlatformIOParser()->getDefinition('FS_MAPPINGS_FLAGS_GZIPPED')) === null) {
            throw new \RuntimeException('Constant FS_MAPPINGS_FLAGS_GZIPPED is not defined');
        }
        $this->flags[self::FLAGS_GZIPPED] = $gzippedFlag;

        if (($type = $webBuilder->getConfigReader()->getPlatformIOParser()->getDefinition('FS_MAPPINGS_COUNTER_TYPE')) === null) {
            throw new \RuntimeException('Constant FS_MAPPINGS_COUNTER_TYPE is not defined');
        }
        switch(strtolower($type)) {
            case 'byte':
            case 'char':
            case 'unsigned char':
            case 'int8_t':
            case 'uint8_t':
                $this->counterTypeSize = 1;
                $hexLength = 2;
                break;
            case 'word':
            case 'short':
            case 'unsigned short':
            case 'int16_t':
            case 'uint16_t':
                $this->counterTypeSize = 2;
                $hexLength = 4;
                break;
            default:
                throw new \RuntimeException(sprintf('FS_MAPPINGS_COUNTER_TYPE type %s is not supported', $type));
        }

        if ($this->webDir) {
            ConfigReader::recursiveDelete($this->webDir, false, '/^([a-z0-9]{'.$hexLength.'}|'.preg_quote(basename($this->mappingsFile), '/').')$/');
        }
    }

    public function finishBuild(): void
    {
        $this->writeMappings();
    }

    private function writeMappings(): void
    {
        usort($this->mappedFiles, function($a, $b) {
            return strcmp($a['mapped_file'], $b['mapped_file']);
        });

        $filenameMappings = '';
        $headers = pack($this->counterTypeSize === 1 ? 'C' : 'S', count($this->mappedFiles));
        $packFormat = $this->counterTypeSize === 1 ? 'SCCLL' : 'SSCLL';
        $totalSize = 0;

        foreach($this->mappedFiles as $file) {

            $ofs1 = strlen($filenameMappings);
            $filenameMappings .= $file['mapped_file']."\0";
            // $ofs2 = strlen($filenameMappings);
            // $filenameMappings .= $file['spiffs_file']."\0";
            $totalSize += $file['file_size'];

            if ($this->verbose) {
                echo $file['mapped_file'].' => '.$file['spiffs_file'].(($file['flags'] & $this->flags[self::FLAGS_GZIPPED]) ? ', gzipped' : '').', size '.$file['original_file_size'].'/'.$file['file_size'].', '.sprintf('%.2f%%', $file['file_size'] / $file['original_file_size'] * 100).' mtime '.$file['mtime']."\n";
            }

            $headers .= pack($packFormat, $ofs1, $file['uid'], $file['flags'], $file['mtime'], $file['file_size']);

            //$headers .= pack('SScLL', $ofs1, $ofs2, $file['flags'], $file['mtime'], $file['file_size']).$file['hash'];

        }
        $totalSize += strlen($headers) + strlen($filenameMappings);

        file_put_contents($this->mappingsFile, $headers.$filenameMappings);

        echo "--------------------------------------------\n";
        echo "Total size of SPIFFS usage: $totalSize\n";

    }

    /**
     * @param File $file
     * @return bool
     */
    public function process(Processor\File $file): bool
    {
        $num = count($this->mappedFiles);
        $outFile = $this->webDir.sprintf(DIRECTORY_SEPARATOR.($this->counterTypeSize === 1 ? '%02x' : '%04x'), $num);

        if (!@copy($file->getTmpIn(), $outFile)) {
            throw new \RuntimeException(sprintf('%s: Cannot copy %s to %s', $file->getSourceAsString(), $file->getTmpIn(), $outFile));
        }

        $orgFileSize = 0;
        $mtime = 0;
        $source = $file->getSource();
        if (is_array($source)) {
            foreach($source as $filename) {
                $mtime = max($mtime, filemtime($filename));
                $orgFileSize += filesize($filename);
            }
        } else {
            $mtime = filemtime($source);
            $orgFileSize = filesize($source);
        }

        $flags = 0;
        $fd = @fopen($file->getTmpIn(), 'r'); // assuming this won't fail since copy() worked
        if ($fd) {
            $header = fread($fd, 3);
            if (strcmp($header, "\x1f\x8b\x8") === 0) {
                $flags |= $this->flags[self::FLAGS_GZIPPED];
            }
            fclose($fd);
        }

        $this->mappedFiles[$num] = array(
            'spiffs_file' => str_replace('\\', '/', substr($outFile, strlen($this->dataDir))),
            'mapped_file' => str_replace('\\', '/', substr($file->getTarget(), strlen($this->webDir))),
            'original_file_size' => $orgFileSize,
            'file_size' => filesize($outFile),
            'flags' => $flags,
            'mtime' => $mtime,
            'uid' => $num,
            // 'hash' => call_user_func($this->hashFunction, $outFile, true),
        );

        $file->setTmpOut($outFile);

        return false;
    }
}
