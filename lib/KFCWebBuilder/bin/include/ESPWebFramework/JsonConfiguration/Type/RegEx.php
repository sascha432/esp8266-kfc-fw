<?php
/**
 * Author: sascha_lammers@gmx.de
 */

namespace ESPWebFramework\JsonConfiguration\Type;

/**
 * Class RegExp
 * @package ESPWebFramework\JsonConfiguration\Type
 */
class RegEx implements TypeInterface
{
    /**
     * @var string
     */
    private $pattern;
    /**
     * @var bool
     */
    private $not;

    /**
     * @var string
     */
    private $name;

    /**
     * @param object $parent
     * @param string $name
     */
    public function setParent(object $parent, string $name): void
    {
        $this->name = $name;
    }

    /**
     * @param string $pattern
     */
    public function setValue($pattern): void
    {
        $this->not = false;
        $ch = substr($pattern, 0, 1);
        if (($pos = strrpos($pattern, $ch)) > 0) {
            $flags = preg_split('//', substr($pattern, $pos + 1), -1, PREG_SPLIT_NO_EMPTY);
            if (($key = array_search('!', $flags, true)) !== false) {
                unset($flags[$key]);
                $this->not = true;
                $pattern = substr($pattern, 0, $pos + 1).implode($flags);
            }
        }
        error_clear_last();
        @preg_match($pattern, '');
        $message = error_get_last()['message'];
        if (strpos($message, 'preg_match') !== false) {
            $array = explode(':', $message);
            throw new \RuntimeException(sprintf('%s: Invalid regular expression: %s: %s', $this->name, trim(end($array)), $pattern));
        }

        $this->pattern = $pattern;
    }

    /**
     * @return string
     */
    public function getValue(): string
    {
        return $this->pattern;
    }

    /**
     * @param string $subject
     * @return bool
     */
    public function isMatch(string $subject): bool
    {
        $result = preg_match($this->pattern, $subject);
        if ($this->not) {
            $result = !$result;
        }
        return $result;
    }
}