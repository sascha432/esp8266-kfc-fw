<?php
/**
 * Author: sascha_lammers@gmx.de
 */

namespace ESPWebFramework;

use ESPWebFramework\Dependencies\File;

/**
 * Class FileScanner
 * @package ESPWebFramework
 */
class FileScanner
{

    /**
     * @param PackageFiles $package
     * @throws \Exception
     */
    private function scanDirectories(PackageFiles $package): void
    {
        foreach($package->getDirectories() as $directory) {

            $dir = dir($path = $directory->getPath());
            while(false !== ($entry = $dir->read())) {
                if ($entry !== '.' && $entry !== '..') {
                    $filename = realpath($path.'/'.$entry);
                    if (is_file($filename) && preg_match($directory->getPattern(), $entry)) {
                        $file = new File();
                        $file
                            ->setIsStatic(false)
                            ->setPath($filename)
                            ->setTargetDir($directory->getTargetDir())
                            ->addGroup($directory->getGroup())
                        ;
                        $package->addFile($file);
                    }
                }
            }
            $dir->close();
        }
    }

    /**
     * @param PackageFiles $package
     */
    public function scanFiles(PackageFiles $package): void
    {
        foreach($package->getFiles() as $file) {
            if (($hash = @sha1_file($file->getPath())) === false) {
                throw new \RuntimeException(sprintf("Cannot read hash of '%s'", $file->getPath()));
            }
            if (($fileSize = @filesize($file->getPath())) === false) {
                throw new \RuntimeException(sprintf("Cannot read file size of '%s'", $file->getPath()));

            }
            $file
                ->setHash($hash)
                ->setFileSize($fileSize)
            ;
        }
    }

    /**
     * @param PackageFiles $package
     * @throws \Exception
     */
    public function scan(PackageFiles $package): void
    {
        $this->scanDirectories($package);
        $this->scanFiles($package);
    }
}