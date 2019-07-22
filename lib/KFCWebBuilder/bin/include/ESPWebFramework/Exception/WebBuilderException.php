<?php
/**
 * Author: sascha_lammers@gmx.de
 */

namespace ESPWebFramework\Exception;

use ESPWebFramework\Processor\File;
use ESPWebFramework\WebBuilder;

/**
 * Class WebBuilderException
 * @package ESPWebFramework\Exception
 */
class WebBuilderException extends \RuntimeException {

    /**
     * WebBuilderException constructor.
     * @param string $message
     * @param WebBuilder $webBuilder
     * @param File|null $file
     * @param int $code
     * @param \Throwable|null $previous
     */
    public function __construct(string $message, WebBuilder $webBuilder, ?File $file = null, int $code = 0, \Throwable $previous = null)
    {
        $webBuilder->log(WebBuilder::LOG_LEVEL_ERROR, $message, array('file' => $file, 'last_error' => error_get_last()['message'] ?? null));
        parent::__construct($message, $code, $previous);
    }
}
