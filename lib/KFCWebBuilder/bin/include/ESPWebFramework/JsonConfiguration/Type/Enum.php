<?php
/**
 * Author: sascha_lammers@gmx.de
 */

namespace ESPWebFramework\JsonConfiguration\Type;

/**
 * Class Enum
 * @package ESPWebFramework\JsonConfiguration\Type
 */
class Enum implements TypeInterface
{
    /**
     * @var object
     */
    private $parent;
    /**
     * @var string
     */
    private $name;
    /**
     * @var int
     */
    private $value;

    /**
     * @param object $class
     * @param string $name
     */
    public function setParent(object $class, string $name): void
    {
        $this->parent = $class;
        $this->name = $name;
    }

    /**
     * @param string|int $value
     * @throws \ReflectionException
     */
    public function setValue($value): void
    {
        $value = strtoupper(str_replace(' ', '_', $value));
        $prefix = strtoupper($this->name.'_');
        $name = $prefix.$value;
        $className = get_class($this->parent);
        $reflection  = new \ReflectionClass($className);
        $constants = $reflection->getConstants();
        if (!isset($constants[$name])) {
            $prefixLen = strlen($prefix);
            $values = array();
            foreach($constants as $key => $val) {
                if (strncmp($key, $prefix, $prefixLen) === 0) {
                    $values[] = substr($key, $prefixLen);
                }
            }
            throw new \RuntimeException(sprintf("Value '%s' is not allowed for %s::%s, possible values %s", $value, $className, $this->name, implode('|', $values)));
        }
        $this->value = $constants[$name];
    }

    /**
     * @return int
     */
    public function getValue(): int
    {
        return $this->value;
    }
}
