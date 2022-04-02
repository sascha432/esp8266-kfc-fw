<?php
/**
 * Author: sascha_lammers@gmx.de
 */

// tested with
// PHP 7.4.28 (cli) (built: Feb 24 2022 02:16:32) ( ZTS Visual C++ 2017 x64 )
// It is not compatible with PHP 8


use ESPWebFramework\DumpConfig;
use ESPWebFramework\WebBuilder;
use ESPWebFramework\ConfigReader;

require_once __DIR__.'/misc/argv.php';

spl_autoload_register(
    function ($class_name) {
        $file = __DIR__.'/'.str_replace('\\', '/', $class_name).'.php';
        if (file_exists($file)) {
            require_once $file;
        } else {
            throw new \Exception(sprintf('Class loader: %s, file does not exist: %s', $class_name, $file));
        }
    }
);

$argParser = new ArgsParser();
$argParser->addBoolOption('force', array('-f', '--force'), null, 'Force to run even if all files are up-to-date');
$argParser->addBoolOption('dirty', array('--dirty'), null, 'Mark files as modified');
$argParser->addBoolOption('verbose', array('-v', '--verbose'), false, 'Be more talkative');
$argParser->addOption('env', array('-e', '--env'), null, null, 'Specify which platformIO environment to use');
$argParser->addMultiOption('branches', array('-b', '--branch'), array(), null, 'Specify one or more branches to execute');
$argParser->addOption('clean_exit_code', array('--clean-exit-code'), 127, null, 'Exit code if the project has not been modified');
$argParser->addOption('dirty_exit_code', array('--dirty-exit-code'), 0, null, 'Exit code if the project has been modified');
$argParser->addBoolOption('dependencies', array('--dep'), false, 'Display external dependencies');
$argParser->addBoolOption('files', array('--files'), false, 'Scan for files and result');
$argParser->addBoolOption('show_env', array('--show-env'), false, 'Show available environments');
$argParser->addBoolOption('dump', array('-D', '--dump-config'), false, 'Display configuration as JSON string');
$argParser->addBoolOption('defines', array('-DD', '--dump-defines'), false, 'Display CPPDEFINES from configuration');
$argParser->addArgument('config', 1, '/path/to/ESPWebFramework.json', 'Configuration file for the project');
$argParser->addOption('cppdefines', array('--defines-from'), '/path/to/defines.txt', null, 'Read CPPDEFINES from file');
$argParser->parse();

$configReader = new ConfigReader($argParser->getArgument('config'));
$configReader->getPlatformIOParser()->setEnvironment($argParser->getOption('env'));
try {
    $filename = $argParser->getOption('cppdefines');
    $configReader->getPlatformIOParser()->readDefines($filename);
    $configReader->addDependency($filename);
} catch (\Exception $exception) {
    echo sprintf("Build failed with: %s\n", $exception->getMessage());
    exit(1);
}
try {
    if ($configReader->read() === false) {
        echo $configReader->getLastError()."\n";
        exit(1);
    }
} catch (\Exception $exception) {
    echo sprintf("Build failed with: %s\n", $exception->getMessage());
    exit(1);
}
if ($argParser->isTrue('show_env')) {
    $env = $configReader->getPlatformIOParser()->getEnvironments();
    if (count($env) === 0) {
        echo 'No environments defined in '.$configReader->getConfiguration()->getPlatformioIni()."\n";
    } else {
        echo "PlatformIO environments:\n";
        foreach($env as $name) {
            echo $name."\n";
        }
    }
    exit(0);
}

if ($argParser->isTrue('dump')) {
    DumpConfig::dump($configReader->getRawJson());
    exit(0);
}
if ($argParser->isTrue('defines')) {
    $configReader->getPlatformIOParser()->dumpDefines();
    exit(0);
}

$webBuilder = new WebBuilder($configReader);
$webBuilder->setVerbose($argParser->isTrue('verbose'));
$webBuilder->setBranch($argParser->getOption('branches'));
if ($argParser->isTrue('files')) {
    echo "Files:\n";
    $webBuilder->scanDependencies();
    $webBuilder->displayFiles();
    exit(0);
}
if ($argParser->isTrue('dependencies')) {
     echo "Dependencies:\n";
     $hasChanged = $webBuilder->scanDependencies();
     $webBuilder->displayDependencies();
     echo sprintf("(and %d project files. Use --files to display)\n", $webBuilder->getFileCount());
     if ($hasChanged) {
         echo "One or more file(s) were modified\n";
     } else {
         echo "No changes detected\n";
     }
     exit(0);
}
try {
    if ($argParser->isTrue('dirty')) {
        $webBuilder->markAsDirty();
        echo "Marked files as modified\n";
        exit(0);
    }
    if ($webBuilder->scanDependencies()) {
        if ($argParser->isFalse('force')) {
            echo "No changes detected.\n";
            exit(intval($argParser->getOption("clean_exit_code")));
        }
        echo "No changes detected, build forced...\n";
    }
    $webBuilder->build();
    $webBuilder->onSuccess();
    exit(intval($argParser->getOption("dirty_exit_code")));

} catch (\Exception $exception) {
    $webBuilder->onFailure();
    if ($path = $webBuilder->getPath()) {
        echo $path.":\n";
    }
    echo sprintf("Build failed with: %s\n", $exception->getMessage());
    exit(1);
}
