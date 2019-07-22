<?php
/**
 * Author: sascha_lammers@gmx.de
 */

namespace ESPWebFramework\JsonConfiguration\Converter;

/**
 * Interface ConverterInterface
 * @package ESPWebFramework\JsonConfiguration\Converter
 */
interface ConverterInterface {
    /**
     * @param $value
     * @return mixed
     */
    public function convertToValue($value);
}
