/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#if ESP32

#include <esp_task_wdt.h>

// Adds the calling task to the task watchdog for a long loop (esp_task_wdt_reset() inside the loop) and
// removes it afterwards - but only if it was not registered already: the loop task is registered for good
// (SerialHandler::Wrapper::addLoop()) and the async_tcp task registers itself, deleting the registration
// there unregistered AsyncTCP's watchdog ("Failed to remove loop task from WDT")
class TaskWdtScope {
public:
    TaskWdtScope() :
        _added(esp_task_wdt_status(nullptr) == ESP_ERR_NOT_FOUND && esp_task_wdt_add(nullptr) == ESP_OK)
    {
    }

    ~TaskWdtScope()
    {
        if (_added) {
            esp_task_wdt_delete(nullptr);
        }
    }

    TaskWdtScope(const TaskWdtScope &) = delete;
    TaskWdtScope &operator=(const TaskWdtScope &) = delete;

private:
    bool _added;
};

#endif
