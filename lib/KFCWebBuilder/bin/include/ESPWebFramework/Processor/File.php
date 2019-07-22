<?php
/**
 * Author: sascha_lammers@gmx.de
 */

namespace ESPWebFramework\Processor;

use ESPWebFramework\FileUtils;

/**
 * Class File
 * @package ESPWebFramework\Processor
 */
class File {

    /**
     * @var string|array
     */
    private $source;
    /**
     * @var string
     */
    private $target;
    /**
     * @var string
     */
    private $tmpIn;
    /**
     * @var string
     */
    private $tmpOut;
    /**
     * @var string
     */
    private $targetDir;
    /**
     * @var bool
     */
    private $published;
    /**
     * @var array
     */
    private $processors;

    /**
     * File constructor.
     * @param string $filename
     * @param string $targetDir
     */
    public function __construct($filename, $targetDir = null)
    {
        $this->source = $filename;
        if ($targetDir) {
            $this->target = $targetDir.DIRECTORY_SEPARATOR.basename($filename);
            $this->targetDir = $targetDir;
        }
        $this->published = false;
        $this->processors = array();
    }

    /**
     * @param string $name
     */
    public function addProcessor($name): void
    {
        $this->processors[] = $name;
    }

    /**
     * @return array
     */
    public function getProcessors(): array
    {
        return $this->processors;
    }

    /**
     * @return string
     */
    public function toString(): string
    {
        return $this->getSourceAsString().' => '.$this->targetDir;
    }

    /**
     * @return string
     */
    public function getSourceAsString(): string
    {
        if (is_array($this->source)) {
            return implode("\n", $this->source);
        }
        return $this->source;
    }

    /**
     * @return string|array
     */
    public function getSource()
    {
        return $this->source;
    }

    /**
     * @param string|array $source
     * @return File
     */
    public function setSource($source): File
    {
        $this->source = $source;
        return $this;
    }

    /**
     * @return string
     */
    public function getTarget(): ?string
    {
        return $this->target;
    }

    /**
     * @param string $target
     * @return File
     */
    public function setTarget(?string $target): File
    {
        $this->target = $target ? FileUtils::realpath($target) : $target;
        return $this;
    }

    /**
     * @return string
     */
    public function getTmpIn(): ?string
    {
        return $this->tmpIn;
    }

    /**
     * @param string $tmpIn
     * @return File
     */
    public function setTmpIn(?string $tmpIn): File
    {
        $this->tmpIn = $tmpIn;
        return $this;
    }

    /**
     * @return string
     */
    public function getTmpOut(): ?string
    {
        return $this->tmpOut;
    }

    /**
     * @param string $tmpOut
     * @return File
     */
    public function setTmpOut(?string $tmpOut): File
    {
        $this->tmpOut = $tmpOut;
        return $this;
    }

    /**
     * @return string
     */
    public function getTargetDir(): string
    {
        return $this->targetDir;
    }

    /**
     * @param string $targetDir
     * @return File
     */
    public function setTargetDir(string $targetDir): File
    {
        $this->targetDir = $targetDir;
        return $this;
    }

    /**
     * @return bool
     */
    public function isPublished(): bool
    {
        return $this->published;
    }

    /**
     * @param bool $published
     * @return File
     */
    public function setPublished(bool $published): File
    {
        $this->published = $published;
        return $this;
    }

    public function passThru(): void
    {
        $this->setTmpOut($this->getTmpIn());
    }

    /**
     * @return array
     */
    public function getVars(): array
    {
        return array(
            '${file.target_dir}' => $this->targetDir,
            '${file.all_sources}' => is_array($this->source) ? $this->source : null,
            '${file.source}' => is_array($this->source) ? null : $this->source,
            '${file.source_name}' => is_array($this->source) ? null : basename($this->source),
            '${file.target}' => $this->target,
            '${file.target_name}' => basename($this->target),
            '${file.tmp_in}' => $this->tmpIn,
            '${file.tmp_out}' => $this->tmpOut,
        );
    }
}
