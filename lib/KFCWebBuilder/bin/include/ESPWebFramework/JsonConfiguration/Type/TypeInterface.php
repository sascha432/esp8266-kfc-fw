<?php
/**
 * Author: sascha_lammers@gmx.de
 */

namespace ESPWebFramework\JsonConfiguration\Type;

/**
 * Interface TypeInterface
 * @package ESPWebFramework\JsonConfiguration\Type
 */
interface TypeInterface
{
    /**
     * @param object $class
     * @param string $name
     */
    public function setParent(object $class, string $name): void;

    /**
     * @param mixed $value
     */
    public function setValue($value): void;

    /**
     * @return mixed
     */
    public function getValue();
}
