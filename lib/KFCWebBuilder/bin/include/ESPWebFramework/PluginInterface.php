<?php
/**
 * Author: sascha_lammers@gmx.de
 */

namespace ESPWebFramework;

/**
 * Interface PluginInterface
 * @package ESPWebFramework
 */
interface PluginInterface {

    /**
     *
     * Gets called to retrieve the object and should return a singleton
     *
     * @param WebBuilder $webBuilder
     * @return self
     */
    public static function getInstance(WebBuilder $webBuilder);

    /**
     * Gets called for every file that passes the processor
     *
     * @param Processor\File $file
     * @return bool
     */
    public function process(Processor\File $file): bool;

    /**
     * Gets called once the build is complete
     */
    public function finishBuild(): void;

}
