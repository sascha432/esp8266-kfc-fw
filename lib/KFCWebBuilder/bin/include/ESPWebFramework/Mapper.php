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
    private const FLAGS_DIR     = 0x02;

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
     * @var string
     */
    private $listingsFile;
    /**
     * @var array
     */
    private $mappedFiles;
    /**
     * @var bool
     */
    private $verbose;
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
        $this->listingsFile = $spiffs->getListingsFile();
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
        // if (($gzippedFlag = $webBuilder->getConfigReader()->getPlatformIOParser()->getDefinition('FS_MAPPINGS_FLAGS_GZIPPED')) === null) {
        //     throw new \RuntimeException('Constant FS_MAPPINGS_FLAGS_GZIPPED is not defined');
        // }
        $this->flags[self::FLAGS_GZIPPED] = 0x01;

        if ($this->webDir) {
            ConfigReader::recursiveDelete($this->webDir, false, '/^([a-z0-9]{2,8}(?:\.(lnk|gz))?|'.preg_quote(basename($this->mappingsFile), '/').')$/');
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

        $headers = pack('S', count($this->mappedFiles));
        $totalSize = 0;

        foreach($this->mappedFiles as $file) {

            $totalSize += $file['file_size'];
            if ($this->verbose) {
                echo $file['mapped_file'].' => '.$file['spiffs_file'].(($file['flags'] & $this->flags[self::FLAGS_GZIPPED]) ? ', gzipped' : '').', size '.$file['original_file_size'].'/'.$file['file_size'].', '.sprintf('%.2f%%', $file['file_size'] / $file['original_file_size'] * 100).' mtime '.$file['mtime']."\n";
            }
            $sizeAndFlags = $file['file_size'] | ($file['flags'] << 24);

            $headers .= pack('LLL', $file['uid'], $file['mtime'], $sizeAndFlags);

        }
        $totalSize += strlen($headers);

        //file_put_contents($this->mappingsFile, $headers);

        $listing = '';
        $plain_listing = "filename:uid:file-size:original-file-size:modification-time:flags:original-filename\n";

        $dirs = array();
        foreach($this->mappedFiles as $file) {
            $name = str_replace('\\', '/', dirname($file['mapped_file']));
            if ($name != '/') {
                $dirs[$name] = max(@$dirs[$name], $file['mtime']);
            }
        }

        foreach($dirs as $dirname => $mtime) {
            $listing .= pack('LLLLC', 0, 0, 0, $mtime, self::FLAGS_DIR);
            $listing .= $dirname."\n";

            $plain_listing .= sprintf("%08x:%08x:%08x:%08x:%08x:%02x:%s\n", 0, 0, 0, 0, $mtime, self::FLAGS_DIR, $dirname);
        }

        foreach($this->mappedFiles as $file) {
            $listing .= pack('LLLLC', $file['uid'], $file['file_size'], $file['original_file_size'], $file['mtime'], $file['flags']);
            $listing .= $file['mapped_file']."\n";

            $plain_listing .= sprintf("%s:%08x:%08x:%08x:%08x:%02x:%s\n", substr($file['spiffs_file'], -8, 8), $file['uid'], $file['file_size'], $file['original_file_size'], $file['mtime'], $file['flags'], $file['mapped_file']);
        }

        file_put_contents($this->listingsFile, $listing);
        file_put_contents($this->listingsFile.".txt", $plain_listing);

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

        $target = str_replace('\\', '/', $file->getTarget());
        $webDir = str_replace('\\', '/', $this->webDir);
        $dataDir = str_replace('\\', '/', $this->dataDir);
        if (substr($target, 0, strlen($webDir)) == $webDir) {
            $mappedFile = substr($target, strlen($webDir));
        }
        else if (substr($target, 0, strlen($dataDir)) == $dataDir) {
            $mappedFile = substr($target, strlen($dataDir));
        }
        else {
            throw new \RuntimeException(sprintf('%s: Outside data directoy %s', $file->getSourceAsString(), $target));
        }

        $mappedFileCrc = hash("crc32b", $mappedFile);
        $outFile = $this->webDir.sprintf(DIRECTORY_SEPARATOR.$mappedFileCrc);
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
                // $zipped = $outFile.'.gz';
                // if (!@file_put_contents($zipped, pack('L', $orgFileSize))) {
                //     throw new \RuntimeException(sprintf('%s: Cannot create %s', $file->getSourceAsString(), $zipped));
                // }
            }
            fclose($fd);
        }

        $symlink = $outFile.'.lnk';
        if (!@file_put_contents($symlink, pack('LL', $mtime, ($flags << 24) | $orgFileSize).$mappedFile)) {
            throw new \RuntimeException(sprintf('%s: Cannot create %s', $file->getSourceAsString(), $symlink));
        }

        foreach($this->mappedFiles as $data) {
            if ($data['crc'] == $mappedFileCrc) {
                throw new \RuntimeException(sprintf('%s: %s and %s have the same crc', $file->getSourceAsString(), $data['mapped_file'], $mappedFile));
            }
        }

        $this->mappedFiles[$num] = array(
            'spiffs_file' => str_replace('\\', '/', substr($outFile, strlen($this->dataDir))),
            'mapped_file' => $mappedFile,
            'original_file_size' => $orgFileSize,
            'file_size' => filesize($outFile),
            'crc' => $mappedFileCrc,
            'flags' => $flags,
            'mtime' => $mtime,
            'uid' => $mappedFileCrc,
            // 'hash' => call_user_func($this->hashFunction, $outFile, true),
        );

        $file->setTmpOut($outFile);

        return false;
    }
}
