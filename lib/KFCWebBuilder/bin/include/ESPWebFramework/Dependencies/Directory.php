<?php
/**
 * Author: sascha_lammers@gmx.de
 */

namespace ESPWebFramework\Dependencies;

class Directory extends Base {

    /**
     * @var string
     */
    private $pattern;

    /**
     * @return string
     */
    public function getPattern(): string
    {
        return $this->pattern;
    }

    /**
     * @param string $pattern
     * @return Directory
     */
    public function setPattern(string $pattern): Directory
    {
        if ($pattern[0] !== '/') {
            $pattern= "/$pattern/";
        }
        $this->pattern = $pattern;
        return $this;
    }
}
