<?php
/**
 * Author: sascha_lammers@gmx.de
 */

namespace ESPWebFramework\Dependencies;

/**
 * Class File
 * @package ESPWebFramework\Dependencies
 */
class File extends Base {

    /**
     * @var string
     */
    private $hash;
    /**
     * @var int
     */
    private $fileSize;
    /**
     * @var bool
     */
    private $isStatic;

    /**
     * @return string
     */
    public function getHash(): ?string
    {
        return $this->hash;
    }

    /**
     * @param string|null $hash
     * @return File
     */
    public function setHash(?string $hash): File
    {
        $this->hash = $hash;
        return $this;
    }

    /**
     * @return int
     */
    public function getFileSize(): ?int
    {
        return $this->fileSize;
    }

    /**
     * @param int|null $fileSize
     * @return File
     */
    public function setFileSize(?int $fileSize): File
    {
        $this->fileSize = $fileSize;
        return $this;
    }

    /**
     * @return bool
     */
    public function isStatic(): bool
    {
        return $this->isStatic;
    }

    /**
     * @param bool $isStatic
     * @return File
     */
    public function setIsStatic(bool $isStatic): File
    {
        $this->isStatic = $isStatic;
        return $this;
    }

}