/**
* Author: sascha_lammers@gmx.de
*/

#pragma once

#include <Arduino_compat.h>
#include <DataProviderInterface.h>

class WebTemplate;

class TemplateDataProvider : public DataProviderInterface
{
public:
    using DataProviderInterface::DataProviderInterface;

    static bool callback(const String& name, DataProviderInterface& provider, WebTemplate &webTemplate);
};
