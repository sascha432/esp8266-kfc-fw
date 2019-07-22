<?php
/**
 * Author: sascha_lammers@gmx.de
 */

namespace ESPWebFramework\Dependencies;

use ESPWebFramework\JsonConfiguration\Groups;

class Base {

    /**
     * @var string
     */
    private $path;
    /**
     * @var Groups[]
     */
    private $groups;
    /**
     * @var string
     */
    private $targetDir;

    public function __construct() {
        $this->groups = array();
    }

    /**
     * @return string
     */
    public function getPath(): string
    {
        return $this->path;
    }

    /**
     * @param string $path
     * @return self
     */
    public function setPath(string $path): self
    {
        $this->path = $path;
        return $this;
    }

    /**
     * @return Groups[]
     */
    public function getGroups(): array
    {
        return $this->groups;
    }

    /**
     * @return Groups
     * @throws \Exception
     */
    public function getGroup() : Groups
    {
        if (count($this->groups) !== 1) {
            throw new \Exception('Directory::getGroup() count '.count($this->groups).' != 1');
        }
        return reset($this->groups);
    }

    /**
     * @param Groups[] $groups
     * @return self
     */
    public function setGroups(Groups $groups): self
    {
        $this->groups = $groups;
        return $this;
    }

    /**
     * @param Groups $group
     * @return self
     */
    public function addGroup($group): self
    {
        if (!in_array($group, $this->groups, true)) {
            $this->groups[] = $group;
        }
        return $this;
    }

    /**
     * @param Groups $group
     * @return bool
     */
    public function hasGroup($group): bool
    {
        return (bool)in_array($group, $this->groups, true);

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
     * @return Base
     */
    public function setTargetDir(string $targetDir): Base
    {
        $this->targetDir = $targetDir;
        return $this;
    }
}