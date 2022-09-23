<?php

namespace Json2php;

use Nette\PhpGenerator\ClassType;
use Screw\Str;

trait GeneratorTrait
{
    private static $keywords = [
        'string',
        'int',
        'float',
        'bool',
        'array',
        'object',
        'callable',
        'iterable',
        'void',
        'self',
        'parent',
    ];

    public static function formatClassName($className): string
    {
        return $className ? Str::toUpperCamelCase($className) : 'Generator' . time();
    }

    public static function createFile($fileName, $content): bool
    {
        file_put_contents($fileName, $content);
        return true;
    }

    public static function getOrSetMethod(ClassType $class, $property, $type): ClassType
    {
        $type = $type === 'integer' ? 'int' : ($type === 'boolean' ? 'bool' : $type);

        $lower = Str::toLowerCamelCase($property);
        $upper = Str::toUpperCamelCase($property);
        $getter = 'get' . $upper;
        $setter = 'set' . $upper;

        if (!in_array($type, self::$keywords, true)) {
            $namespace = $class->getNamespace()->getName();
            $type = "\\$namespace\\$type";
        }

        $class->addProperty($lower)->setVisibility('private')->addComment("@var {$type}|null");
        $class->addMethod($getter)
            ->setReturnNullable()
            ->setReturnType($type)
            ->addBody("return \$this->{$lower};")
            ->addComment("@return {$type}|null");
        $class->addMethod($setter)
            ->setBody("\$this->{$lower} = \${$lower};")
            ->addComment("@param {$type}|null \${$lower}")
            ->setReturnType('void')
            ->addParameter($lower)
            ->setTypeHint($type)
            ->setNullable();

        return $class;
    }
}