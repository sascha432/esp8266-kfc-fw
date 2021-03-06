<?php

/**
 * Code generated by json2php. DO NOT EDIT.
 * Generator at 2018-12-29 19:28:11
 */

namespace ESPWebFramework\JsonConfiguration;

class Groups
{
    /** @var string|null */
    private $name;

    /** @var \ESPWebFramework\JsonConfiguration\TargetDirs[]|array */
    private $targetDirs = [];

    /** @var string[]|array */
    private $prepend = [];

    /** @var string[]|array */
    private $append = [];

    /** @var \ESPWebFramework\JsonConfiguration\Processors[]|array */
    private $processors = [];

    /**
     * @return string|null
     */
    public function getName(): ?string
    {
        return $this->name;
    }

    /**
     * @return \ESPWebFramework\JsonConfiguration\TargetDirs[]|array
     */
    public function getTargetDirs(): array
    {
        return $this->targetDirs;
    }

    /**
     * @return string[]|array
     */
    public function getPrepend(): array
    {
        return $this->prepend;
    }

    /**
     * @return string[]|array
     */
    public function getAppend(): array
    {
        return $this->append;
    }

    /**
     * @return \ESPWebFramework\JsonConfiguration\Processors[]|array
     */
    public function getProcessors(): array
    {
        return $this->processors;
    }
}
