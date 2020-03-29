<?php

/**
 * Code generated by json2php. DO NOT EDIT.
 * Generator at 2018-12-29 19:28:11
 */

namespace ESPWebFramework\JsonConfiguration;

class SourceFiles
{
    /**
     * @var string|null
     * @validator \ESPWebFramework\FileUtils::findFiles
     * validator \ESPWebFramework\FileUtils::realpath
     * validator file_exists
     * validator is_file
     */
    private $sourceFile;

    /**
     * @return string|null
     */
    public function getSourceFile(): ?string
    {
        return $this->sourceFile;
    }
}
