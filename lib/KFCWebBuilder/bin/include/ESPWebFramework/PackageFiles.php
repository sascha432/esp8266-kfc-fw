<?php
/**
 * Author: sascha_lammers@gmx.de
 */

namespace ESPWebFramework;

use ESPWebFramework\Dependencies\Directory;
use ESPWebFramework\Dependencies\File;
use ESPWebFramework\JsonConfiguration\Groups;

/**
 * Class PackageFiles
 * @package ESPWebFramework
 */
class PackageFiles {

    const DIRTY_HASH = '***DIRTY***';

    /**
     * @var File[]
     */
    private $files;

    /**
     * @var Directory[]
     */
    private $directories;

    /**
     * @var string[]
     */
    private $dependencies;

    /**
     * @var string
     */
    private $packageHash;

    /**
     * @var string
     */
    private $hashStorageFile;

    public function clear(): void
    {
        $this->files = array();
        $this->directories = array();
        $this->packageHash = null;
    }

    public function removeScanResults(): void
    {
        foreach($this->getFiles() as $file) {
            if (!$file->isStatic()) {
                $this->removeFile($file);
            } else {
                $file
                    ->setHash(null)
                    ->setFileSize(null);
            }
        }
    }

    /**
     * @return string[]
     */
    public function getDependencies(): array
    {
        return $this->dependencies;
    }

    /**
     * @param string[] $dependencies
     * @return PackageFiles
     */
    public function setDependencies(array $dependencies): PackageFiles
    {
        $this->dependencies = $dependencies;
        return $this;
    }

    /**
     * @return File[]
     */
    public function getFiles(): array
    {
        return $this->files;
    }

    /**
     * @return array
     */
    public function getFilenames(): array
    {
        $files = array();
        foreach($this->files as $file) {
            $files[] = $file->getPath();
        }
        return $files;
    }

    /**
     * @param File $file
     * @return $this
     */
    public function addFile(File $file): self
    {
        $this->files[] = $file;
        return $this;
    }

    /**
     * @param File $file
     * @return bool
     */
    public function removeFile(File $file): bool
    {
        if (($key = array_search($file, $this->files, true)) !== false) {
            unset($this->files[$key]);
            return true;
        }
        return false;
    }

    /**
     * @param string $filename
     * @return int
     */
    public function removeFileByName($filename): int
    {
        $count = 0;
        foreach($this->files as $key => $file) {
            if ($file->getPath() === $filename) {
                unset($this->files[$key]);
                $count++;
            }
        }
        return $count;
    }

    /**
     * @return Directory[]
     */
    public function getDirectories(): array
    {
        return $this->directories;
    }

    /**
     * @param Directory $dir
     * @return $this
     */
    public function addDirectory(Directory $dir): self
    {
        $this->directories[] = $dir;
        return $this;
    }

    /**
     * @param Groups $group
     * @param string|null $className
     * @return array
     */
    public function getFilesByGroup($group, $className = null): array
    {
        $files = array();
        foreach($this->files as $file) {
            if ($file->hasGroup($group)) {
                if ($className !== null) {
                    $files[] = new $className($file->getPath(), $file->getTargetDir());
                } else {
                    $files[] = $file->getPath();
                }
            }
        }
        return $files;
    }

    /**
     * @return string
     * @throws \Exception
     */
    public function calcHash(): string
    {
        // find the longest possible directory match between all files
        $commonDir = '';
        foreach($files = array_merge($this->getFilenames(), $this->getDependencies()) as $filename) {
            $dir = dirname($filename);
            if (!$commonDir) {
                $commonDir = $dir;
            } else {
                for($i = 0, $len = strlen($dir); $i < $len; $i++) {
                    if (strncmp($dir, $commonDir, $i) !== 0) {
                        $commonDir = substr($commonDir, 0, $i - 1);
                    }
                }
            }
        }

        $files = array();
        foreach($this->getDependencies() as $filename) {
            $index = substr($filename, strlen($commonDir));
            if (($hash = @sha1_file($filename)) === false) {
                throw new \Exception(sprintf("Cannot read hash of '%s'", $filename));
            }
            $files[$index] = $hash;
        }

        foreach($this->getFiles() as $file) {
            $index = substr($file->getPath(), strlen($commonDir));
            $files[$index] = $file->getHash();
        }

        // unique file names, ordered by name
        array_unique($files);
        ksort($files);

        $hash = sha1(implode(',', $files));
        $this->packageHash = $hash;
        return $hash;
    }

    /**
     * @return int
     */
    private function sumFileSize(): ?int
    {
        $size = 0;
        foreach($this->files as $file) {
            $size += $file->getFileSize();
        }
        return $size;
    }

    /**
     * @param $filename
     */
    public function setHashStorage($filename): void
    {
        $this->hashStorageFile = $filename;
    }

    /**
     * @param string $branch
     * @return bool
     */
    public function isHashMatch(string $branch): bool
    {
        if (($json = file_get_contents($this->hashStorageFile)) === false) {
            return false;
        }
        if (($json = json_decode($json, true)) === null) {
            return false;
        }

        if (isset($json[$branch])) {
            return ($json[$branch]['hash'] === $this->packageHash);
        }
        return false;
    }

    public function markAsDirty(): void
    {
        $this->packageHash = self::DIRTY_HASH;
    }

    /**
     * @param string $branch
     */
    public function storeHash(string $branch): void
    {
        $json = json_decode(file_get_contents($this->hashStorageFile), true);
        if ($this->packageHash === self::DIRTY_HASH) {
            $json[$branch] = array(
                'hash' => $this->packageHash,
                'last_update' => strftime('%FT%TZ%Z'),
                'files' => 0,
                'file_size' => 0,
            );
        } else {
            $json[$branch] = array(
                'hash' => $this->packageHash,
                'last_update' => strftime('%FT%TZ%Z'),
                'files' => count($this->files),
                'file_size' => $this->sumFileSize(),
            );
        }
        file_put_contents($this->hashStorageFile, json_encode($json));
    }

}
