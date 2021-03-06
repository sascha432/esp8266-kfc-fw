<?php

/**
 * Code generated by json2php. DO NOT EDIT.
 * Generator at 2018-12-29 19:28:11
 */

namespace ESPWebFramework\JsonConfiguration;

class WebBuilder
{
    /** @var string[]|array */
    private $branches = [];

    /** @var string[]|array */
    private $conditionalExcludes = [];

    /** @var \ESPWebFramework\JsonConfiguration\Groups[]|array */
    private $groups = [];

    /**
     * @return string[]|array
     */
    public function getBranches(): array
    {
        return $this->branches;
    }

    /**
     * @return string[]|array
     */
    public function getConditionalExcludes(): array
    {
        return $this->conditionalExcludes;
    }

    /**
     * @return \ESPWebFramework\JsonConfiguration\Groups[]|array
     */
    public function getGroups(): array
    {
        return $this->groups;
    }
}
