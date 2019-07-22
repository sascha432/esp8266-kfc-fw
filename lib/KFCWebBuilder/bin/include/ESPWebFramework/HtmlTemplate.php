<?php
/**
 * Author: sascha_lammers@gmx.de
 */

namespace ESPWebFramework;

use http\Exception\RuntimeException;
use mysql_xdevapi\Exception;

/**
 * Class HtmlTemplate
 * @package ESPWebFramework
 */
class HtmlTemplate {

    /**
     * @var PlatformIOParser
     */
    private $platformIoParser;

    /**
     * @var string
     */
    private $source;

    /**
     * HtmlTemplate constructor.
     * @param PlatformIOParser $platformIoParser
     */
    public function __construct($platformIoParser)
    {
        $this->platformIoParser = $platformIoParser;
    }

    /**
     * @param string $filename
     * @return string
     */
    private function preProcessTemplate($filename): string
    {
        if (($lines = @file($filename, FILE_IGNORE_NEW_LINES)) === false) {
            throw new \RuntimeException(sprintf('%s: Cannot read %s', $this->source, $filename));
        }

        $activeBlocks = array(true);
        $level = 0;
        $content = '';
        $commentStart = '(?:\/\*|<!)';
        $commentEnd = '(?:\*\/|>)';
        foreach ($lines as $line) {
            if (preg_match('/^\s*'.$commentStart.'--\s*#if\s+([^-]+)--'.$commentEnd.'/', $line, $out)) {
                $level++;
                $activeBlocks[$level] = $this->platformIoParser->evaluateExpression($out[1]);
            } else if (preg_match('/^\s*'.$commentStart.'--\s*#elif\s+([^-]+)\--'.$commentEnd.'/', $line, $out)) {
                if (!$activeBlocks[$level]) {
                    $activeBlocks[$level] = $this->platformIoParser->evaluateExpression($out[1]);
                }
            } else if (preg_match('/^\s*'.$commentStart.'--\s*#else\s*--'.$commentEnd.'/', $line)) {
                $activeBlocks[$level] = !$activeBlocks[$level];
            } else if (preg_match('/^\s*'.$commentStart.'--\s*#endif\s*--'.$commentEnd.'/', $line)) {
                $activeBlocks[$level] = false;
                $level--;
            } else if ($activeBlocks[$level]) {
                $content .= $line . "\n";
            }
        }

        return $content;
    }

    /**
     * @param string $content
     * @param string $dir
     * @return string
     */
    private function includeFiles($content, $dir): string
    {
        $commentStart = '(?:\/\*|<!)';
        $commentEnd = '(?:\*\/|>)';
        if (preg_match_all('/'.$commentStart.'--%%([^-]+)--'.$commentEnd.'[\S\s]*'.$commentStart.'--\1%%--'.$commentEnd.'/m', $content, $out)) {
            foreach ($out[1] as $key => $includeFile) {
                $includeFile = $dir.sprintf('.%s.html', strtolower($includeFile));
                if (!file_exists($includeFile)) {
                    throw new \RuntimeException(sprintf('%s: Cannot include %s', $this->source, $includeFile));
                }
                $includeContent = $this->preProcessTemplate($includeFile);
                $content = str_replace($out[0][$key], $includeContent, $content);
            }
        }
        return $content;
    }

    /**
     * @param string $sourceFile
     * @param string $input
     * @param string $output
     * @param array $assets
     */
    public function process($sourceFile, $input, $output, $assets): void
    {
        $this->source = $sourceFile;

        $dir = dirname($sourceFile).DIRECTORY_SEPARATOR;
        $content = $this->preProcessTemplate($input);
        $content = $this->includeFiles($content, $dir);
        $content = strtr($content, $assets);
        if (@file_put_contents($output, $content) === false) {
            throw new \RuntimeException(sprintf('%s: Cannot write %s', $sourceFile, $output));
        }

    }
}
