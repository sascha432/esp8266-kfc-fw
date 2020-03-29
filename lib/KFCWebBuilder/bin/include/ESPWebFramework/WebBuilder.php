<?php
/**
 * Author: sascha_lammers@gmx.de
 */

namespace ESPWebFramework;

use ESPWebFramework\Exception\WebBuilderException;
use ESPWebFramework\JsonConfiguration\Command;
use ESPWebFramework\JsonConfiguration\Groups;
use ESPWebFramework\JsonConfiguration\JsonConfiguration;
use ESPWebFramework\JsonConfiguration\Processors;
use ESPWebFramework\Dependencies\Directory;
use ESPWebFramework\Dependencies\File;
use ESPWebFramework\FileUtils;

/**
 * Class WebBuilder
 * @package ESPWebFramework
 */
class WebBuilder extends Logger {

    /**
     * @var ConfigReader
     */
    private $configReader;

    /**
     * @var PackageFiles
     */
    private $packageFiles;

    /**
     * @var JsonConfiguration
     */
    private $config;

    /**
     * @var array
     */
    private $assets;

    /**
     * @var string|null
     */
    private $tmpCombined;

    /**
     * @var array
     */
    private $finishedCallbacks;

    /**
     * @var array
     */
    private $path;

    /**
     * @var array
     */
    private $branch;

    /**
     * WebBuilder constructor.
     * @param ConfigReader $configReader
     */
    public function __construct(ConfigReader $configReader)
    {
        $this->configReader = $configReader;
        $this->packageFiles = new PackageFiles();
        $this->tmpCombined = false;
        $this->assets = array();
        $this->finishedCallbacks = array();
        $this->path = array();
    }

    /**
     * @param array $branches
     */
    public function setBranch($branches): void
    {
        $this->branch = $branches;
    }

    /**
     * @return array
     */
    public function getBranch(): array
    {
        return $this->branch;
    }

    /**
     * @return string
     */
    public function getBranchString(): string
    {
        if (!$this->branch) {
            $str = 'default';
        } else {
            $tmp = array_unique($this->branch);
            sort($tmp);
            $str = '['.implode('|', $tmp).']';
        }
        return $str;
    }

    /**
     * @return string
     */
    public function getPath(): string
    {
        return implode('.', $this->path);
    }

    /**
     * @return ConfigReader
     */
    public function getConfigReader(): ConfigReader
    {
        return $this->configReader;
    }

    /**
     * @return JsonConfiguration
     */
    public function getConfig(): JsonConfiguration
    {
        return $this->config;
    }

    public function createDependencies(): void
    {
        $this->setLogFile($this->config->getConfiguration()->getLogFile(), $this->config->getConfiguration()->getLogLevel()->getValue());

        $this->packageFiles->clear();

        $groups = $this->config->getWebBuilder()->getGroups();
        foreach($groups as $group) {
            foreach($group->getTargetDirs() as $targetDir) {
                $targetDir->getTargetDir();
                foreach($targetDir->getSourceDirs() as $sourceDirs) {
                    $dir = new Directory();
                    $dir
                        ->setPattern($sourceDirs->getPattern()->getValue())
                        ->setPath($sourceDirs->getSourceDir())
                        ->setTargetDir($targetDir->getTargetDir())
                        ->addGroup($group)
                    ;
                    $this->packageFiles->addDirectory($dir);
                }
                foreach($targetDir->getSourceFiles() as $sourceFiles)  {

                    foreach(FileUtils::findFiles($sourceFiles->getSourceFile()) as $sourceFile) {
                        $file = new File();
                        $file
                            ->setIsStatic(true)
                            ->setPath($sourceFile)
                            // ->setPath($sourceFiles->getSourceFile())
                            ->setTargetDir($targetDir->getTargetDir())
                            ->addGroup($group)
                        ;
                        $this->packageFiles->addFile($file);

                    }
                }
            }
        }
    }

    /**
     * @param string $key
     * @param callable $cb
     */
    public function addFinishedCallback(string $key, callable $cb): void
    {
        $this->finishedCallbacks[$key] = $cb;
    }

    public function displayDependencies(): void
    {
        foreach($this->configReader->getDependencies() as $filename) {
            echo $filename."\n";
        }
    }

    public function displayFiles(): void
    {
        foreach($this->packageFiles->getFilenames() as $filename) {
            echo $filename."\n";
        }
    }

    /**
     * @return int
     */
    public function getFileCount(): int
    {
        return count($this->packageFiles->getFiles());
    }

    /**
     * @return bool
     * @throws \Exception
     */
    public function scanDependencies(): bool
    {
        $this->config = $this->configReader->getConfiguration();

        $branches = $this->getBranches();
        foreach($this->branch as $branch) {
            if (!in_array($branch, $branches, true)) {
                throw new WebBuilderException(sprintf("Branch '%s' does not exist. (%s)", $branch, implode(',', $branches)), $this);
            }
        }

        $this->log(self::LOG_LEVEL_DEBUG, 'Scanning dependencies');

        $scanner = new FileScanner();

        $this->createDependencies();
        $this->packageFiles->setHashStorage($this->config->getConfiguration()->getPackageHashFile());
        $this->packageFiles->setDependencies($this->configReader->getDependencies());
        $this->packageFiles->removeScanResults();
        $scanner->scan($this->packageFiles);

        foreach($this->config->getWebBuilder()->getConditionalExcludes() as $filename) {
            $this->packageFiles->removeFileByName($filename);
        }

        $this->packageFiles->calcHash();

        if ($this->packageFiles->isHashMatch($this->getBranchString())) {
            return true;
        }

        return false;
    }

    /**
     * @return array
     */
    private function getGroups(): array
    {
        $groups = array();
        foreach($this->config->getWebBuilder()->getGroups() as $group) {
            if (!$group->getAppend() && !$group->getPrepend()) {
                $groups[] = $group;
            }
        }
        return $groups;
    }

    /**
     * @return array
     */
    private function getBranches(): array
    {
        $branches = $this->config->getWebBuilder()->getBranches();
        if (!$branches) {
            $branches = array();
        }
        foreach($this->config->getWebBuilder()->getGroups() as $group) {
            foreach($group->getProcessors() as $processor) {
                $branches[] = $processor->getBranch();
                unset($branch);
            }
        }
        $branches = array_merge(...$branches);
        foreach($branches as &$branch) {
            $branch = ltrim($branch, '!');
        }
        return array_unique($branches);
    }

    /**
     * @param Groups $group
     * @return array|Processors[]
     */
    private function getProcessors(Groups $group): array
    {
        $processors = $group->getProcessors();

        foreach($this->config->getWebBuilder()->getGroups() as $group2) {
            if ($group2->getPrepend() && in_array($group->getName(), $group2->getPrepend(), true)) {
                foreach(array_reverse($group2->getProcessors()) as $processor) {
                    array_unshift($processors, $processor);
                }
            }
            if ($group2->getAppend() && in_array($group->getName(), $group2->getAppend(), true)) {
                foreach($group2->getProcessors() as $processor) {
                    $processors[] = $processor;
                }
            }
        }
        foreach($processors as $key => $processor) {
            if ($processor->getBranch()) {
                $remove = false;
                foreach($processor->getBranch() as $branch) {
                    if ($branch[0] === '!') {
                        if (in_array(substr($branch, 1), $this->branch, true)) {
                            $remove = true;
                        }
                        break;
                    } else {
                        if (!in_array($branch, $this->branch, true)) {
                            $remove = true;
                        }
                        break;
                    }
                }
                if ($remove) {
                    unset($processors[$key]);
                    continue;
                }
            }
            if ($processor->getCondition()) {
                $condition = preg_replace('/^#\s*if/', '', $processor->getCondition());
                if (!$this->configReader->getPlatformIOParser()->evaluateExpression($condition)) {
                    unset($processors[$key]);
                }
            }
        }
        return $processors;
    }

    /**
     * @throws \Exception
     */
    public function build(): void
    {
        if (!count($this->packageFiles->getDependencies())) {
            throw new WebBuilderException('Dependencies must be scanned before building the package', $this);
        }

        $this->log(self::LOG_LEVEL_NOTICE, 'Starting build process, branch: '.$this->getBranchString());


        foreach($this->getGroups() as $group) {

            if ($this->verbose) {
                echo 'Group '.$group->getName()."\n";
            }

            /* @var Processor\File[] $files */
            $files = $this->packageFiles->getFilesByGroup($group, Processor\File::class);
            if (!count($files)) {
                echo "This group does not have any files assigned.\n";
                continue;
            }

            $this->path[] = 'group['.$group->getName().']';
            $this->tmpCombined = false;

            foreach($this->getProcessors($group) as $processor) {

                $this->path[] = 'processor['.$processor->getName().']';

                if ($this->tmpCombined && count($files) && !isset($files['combined'])) {
                    $filesArray = array();
                    foreach($files as $file) {
                        $filesArray[] = $file->getSource();
                    }
                    $file = new Processor\File($filesArray);
                    $file->setTmpOut($this->tmpCombined);
                    $file->setTargetDir(reset($files)->getTargetDir());
                    $files = array(
                        'combined' => $file,
                    );
                }

                if ($this->verbose) {
                    echo 'Processor '.$processor->getName().', '.count($files)." file(s)\n";
                }
                foreach($files as $key => $file) {
                    if (!$this->executeProcessor($processor, $file)) {
                        unset($files[$key]);
                    }
                }

                array_pop($this->path);
            }

            array_pop($this->path);
        }

        foreach($this->finishedCallbacks as $callback) {
            $this->path[] = 'finishBuild::'.get_class($callback[0]);
            $callback();
            array_pop($this->path);
        }

        $this->packageFiles->storeHash($this->getBranchString());

        $this->configReader->setCleanUpOnExit(true);
    }

    public function onSuccess(): void
    {
        if ($this->config->getConfiguration()->getRunCommandOnSuccess()) {
            $this->executeExternalCommand($this->config->getConfiguration()->getRunCommandOnSuccess(), true);
        }
    }

    public function onFailure(): void
    {
        if ($this->config->getConfiguration()->getRunCommandOnFailure()) {
            try {
                $this->executeExternalCommand($this->config->getConfiguration()->getRunCommandOnFailure(), true);
            } catch (\Exception $exception) {
            }
        }
    }

    public function markAsDirty(): void
    {
        $this->config = $this->configReader->getConfiguration();
        $this->packageFiles->setHashStorage($this->config->getConfiguration()->getPackageHashFile());
        $this->packageFiles->markAsDirty();
        $this->packageFiles->storeHash($this->getBranchString());
    }

    /**
     * @param string $name
     * @param string $filename
     */
    public function addAsset(string $name, string $filename): void
    {
        $webDir = $this->config->getSpiffs()->getWebTargetDir();
        if (strpos($filename, $webDir) === 0) {
            $filename = substr($filename, strlen($webDir) + 1);
        }
        $this->assets['%'.$name.'%'] = str_replace('\\', '/', $filename);
    }

    /**
     * @return array
     */
    public function getAssets(): array
    {
        return $this->assets;
    }

    /**
     * @return string
     */
    public function getWebTargetDir(): string
    {
        return $this->config->getSpiffs()->getWebTargetDir();
    }

    /**
     * @param Processors $processor
     * @return string
     */
    private function createTmpFile(Processors $processor): string
    {
        do {
            $tmp = $this->configReader->getProcessTempDir().DIRECTORY_SEPARATOR.$processor->getName().'_'.sha1(microtime(true));
        } while(file_exists($tmp));
        touch($tmp);
        return $tmp;
    }

    /**
     * @param string $code
     * @param Processor\File $file
     */
    private function phpEval(string $code, Processor\File $file): void
    {
        try {
            $res = eval($code);
            if ($res) {
                throw new WebBuilderException(sprintf('Exit code %d', $res), $this,  $file);
            }
        } catch (\Exception $exception) {
            throw new WebBuilderException(sprintf("Eval failed: %s\nCode: %s", $exception->getMessage(), $code), $this, $file);
        }
    }

    /**
     * @param Processor\File $file
     */
    private function createOutputDirectory(Processor\File $file): void
    {
        /** @noinspection MkdirRaceConditionInspection */
        if (@mkdir($dir = dirname($file->getTmpOut()), 0777, true)) {
            $this->log(self::LOG_LEVEL_DEBUG, "Created directory $dir");
        }
    }

    /**
     * @param string $command
     * @return array
     */
    private function executeExternalCommand(string $command, $showOutput = false): array
    {

        $envContext = array();
        foreach($this->config->getConfiguration()->getSetEnv() ?? [] as $env) {
            putenv($env->getName().'='.$env->getValue());
            $envContext[] = ''.$env->getName().'='.$env->getValue();
        }
        $this->log(self::LOG_LEVEL_DEBUG, 'Executing command '.$command.($envContext ? ' with environment variables '.implode(', ', $envContext) : ''));

        $output = array();
        $exitCode = 0;
        if ($showOutput) {
            passthru($command, $exitCode);
        } else {
            exec($command, $output, $exitCode);
        }

        if($exitCode !== 0) {
            throw new \RuntimeException(sprintf('Exit code %d', $exitCode));
        }
        return $output;
    }

    /**
     * @param Processors $processor
     * @param Command $command
     * @param Processor\File $file
     * @return bool
     */
    private function executeCommand(Processors $processor, Command $command, Processor\File $file): bool
    {
        $result = false;
        switch($command->getType()->getValue()) {
            case Command::TYPE_COMMAND:
                $this->createOutputDirectory($file);
                $vars = $file->getVars();
                $cmd = $command->getCommand();
                array_walk($vars, function(&$val, $key) {
                    if (is_array($val)) {
                        foreach($val as &$item) {
                            $item = escapeshellarg(str_replace(array('/', '\\'), DIRECTORY_SEPARATOR, $item));
                        }
                        unset($item);
                        $val = implode(' ', $val);
                    } else {
                        $val = escapeshellarg(str_replace(array('/', '\\'), DIRECTORY_SEPARATOR, $val));
                    }
                });
                $cmd = str_replace(array_keys($vars), array_values($vars), $cmd);

                try {
                    $this->executeExternalCommand($cmd);
                } catch (\Exception $exception) {
                    throw new WebBuilderException(sprintf("Command '%s' failed.\nSource %s: %s", $cmd, $file->getSourceAsString(), $exception->getMessage()), $this, $file, 0, $exception);
                }

                if (!@filesize($file->getTmpOut())) {
                    throw new WebBuilderException(sprintf("Command '%s' failed.\nSource %s: Output file %s is empty", $cmd, $file->getSourceAsString(), $file->getTmpOut()), $this, $file);
                }
                break;
            case Command::TYPE_INTERNAL_COMMAND:
                $this->createOutputDirectory($file);
                switch($cmd = $command->getCommand()) {
                    case 'remove-file':
                        return false;
                    case 'create-html-template':
                        try {
                            $template = new HtmlTemplate($this->configReader->getPlatformIOParser());
                            $template->process($file->getSource(), $file->getTmpIn(), $file->getTmpOut(), $this->assets);
                        } catch (\Exception $exception) {
                            throw new WebBuilderException($exception->getMessage(), $this, $file, 0, $exception);
                        }
                        break;
                    case 'copy':
                        if (!@copy($file->getTmpIn(), $file->getTmpOut())) {
                            throw new WebBuilderException(sprintf('Source %s: Failed to copy %s to %s', $file->getSourceAsString(), $file->getTmpIn(), $file->getTmpOut()), $this, $file);
                        }
                        break;
                    case 'copy-combined':
                        if (!$this->tmpCombined) {
                            $this->tmpCombined = $file->getTmpOut();
                            if (!@copy($file->getTmpIn(), $this->tmpCombined)) {
                                throw new WebBuilderException(sprintf('Source %s: Failed to copy %s to %s', $file->getSourceAsString(), $file->getTmpIn(), $this->tmpCombined), $this, $file);
                            }
                        } else {
                            if (($contents = @file_get_contents($file->getTmpIn())) === false) {
                                throw new WebBuilderException(sprintf('Source %s: Failed to read %s', $file->getSourceAsString(), $file->getTmpIn()), $this, $file);
                            }
                            if (@file_put_contents($this->tmpCombined, $contents, FILE_APPEND) === false) {
                                throw new WebBuilderException(sprintf('Source %s: Cannot write to %s', $file->getSourceAsString(), $file->getTmpOut()), $this, $file);
                            }
                        }
                        break;
                    default:
                        throw new WebBuilderException(sprintf("Invalid internal command '%s'", $cmd), $this, $file) ;
                }
                break;
            case Command::TYPE_PHP_EVAL:
                $this->phpEval($command->getCode(), $file);
                break;
            case Command::TYPE_PHP_CLASS:
                if ($command->getIncludeFile()) {
                    require_once $command->getIncludeFile();
                }
                $className = $command->getClassName();
                try {
                    if (!class_exists($className, true)) {
                        throw new WebBuilderException(sprintf('Class %s does not exist', $className), $this);
                    }
                    $obj = $className::getInstance($this);
                } catch (\Exception $exception) {
                    throw new WebBuilderException(sprintf('Failed to get instance of %s: %s', $className, $exception->getMessage()), $this, $file, 0,  $exception);
                }
                if (!$obj instanceof PluginInterface) {
                    throw new WebBuilderException(sprintf('%s does not implement %s', $className, PluginInterface::class), $this);
                }

                if (!isset($this->finishedCallbacks[spl_object_hash($obj)])) {
                    $this->addFinishedCallback(spl_object_hash($obj), array($obj, 'finishBuild'));
                }
                $result = $obj->process($file);
                break;
        }

        $this->log(self::LOG_LEVEL_DEBUG, $processor->getName().' '.basename($file->getTmpIn()).' '.filesize($file->getTmpIn()).' => '.basename($file->getTmpOut()).' '.filesize($file->getTmpOut()));

        return $processor->getPublish() ? $result : true;
    }

    /**
     * @param Processors $processor
     * @param Processor\File $file
     * @return bool
     */
    private function executeProcessor(Processors $processor, Processor\File $file): ?bool
    {
        if (!$file->getTmpOut()) {
            $tmpIn = $file->getSource();
        } else {
            $tmpIn = $file->getTmpOut();
        }
//        echo "$tmpIn ".filesize($tmpIn)."\n";

        if ($processor->getPattern() && !$processor->getPattern()->isMatch($file->getSource())) {
            $this->log(self::LOG_LEVEL_DEBUG, 'Pattern does not match', array('pattern' => $processor->getContentPattern(), 'file' => $file));
            return true;
        }
        if ($processor->getFilenamePattern() && !$processor->getFilenamePattern()->isMatch(basename($file->getSource()))) {
            $this->log(self::LOG_LEVEL_DEBUG, 'Filename pattern does not match', array('pattern' => $processor->getFilenamePattern(), 'file' => $file));
            return true;
        }
        if ($processor->getContentPattern()) {
            $contents = @file_get_contents($tmpIn);
            if (!$processor->getContentPattern()->isMatch($contents)) {
                $this->log(self::LOG_LEVEL_DEBUG, 'Content pattern does not match', array('pattern' => $processor->getContentPattern(), 'file' => $file));
                return true;
            }
        }

        $file->setTmpIn($tmpIn);

        if ($processor->getTarget()) {
            $vars = $file->getVars();
            $file->setTmpOut(str_replace(array_keys($vars), array_values($vars), $processor->getTarget()));
        } else {
            $publish = $processor->getPublish();
            if ($publish) {
                switch($processor->getCommand()->getType()->getValue()) {
                    case Command::TYPE_INTERNAL_COMMAND:
                    case Command::TYPE_COMMAND:
                        if (!$file->getTarget()) {
                            throw new WebBuilderException(sprintf('No target for file %s', $file->getSource()), $this, $file);
                        }
                        break;
                }
                $file->setTmpOut($file->getTarget());
            } else {
                $file->setTmpOut($this->createTmpFile($processor));
            }
        }

        $file->addProcessor($processor->getName());

        return $this->executeCommand($processor, $processor->getCommand(), $file);
    }
}
