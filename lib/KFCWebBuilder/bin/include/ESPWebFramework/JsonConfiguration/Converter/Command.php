<?php
/**
 * Author: sascha_lammers@gmx.de
 */

namespace ESPWebFramework\JsonConfiguration\Converter;

/**
 * Class Command
 * @package ESPWebFramework\JsonConfiguration\Converter
 */
class Command implements ConverterInterface
{
    /**
     * @param $value
     * @return object
     */
    public function convertToValue($value)
    {
        if (is_string($value)) {
            /** @noinspection SubStrUsedAsStrPosInspection */
            if ($value[0] === '@') {
                return (object)array(
                    'type' => 'internal_command',
                    'command' => substr($value, 1),
                );
            }
            return (object)array(
                'type' => 'command',
                'command' => $value,
            );
        }
        return $value;
    }
}
