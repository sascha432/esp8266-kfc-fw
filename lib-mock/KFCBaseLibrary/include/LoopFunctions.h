/**
  Author: sascha_lammers@gmx.de
*/

#pragma once

#include <functional>
#include <vector>

class LoopFunctions {
public:
    typedef std::function<void()> ScheduledFunc;

    static std::vector<LoopFunctions::ScheduledFunc> &getScheduledFunctions() {
        static std::vector<LoopFunctions::ScheduledFunc> _scheduledFunctions;
        return _scheduledFunctions;
    }

    static void callOnce(ScheduledFunc callback) {
        getScheduledFunctions().push_back(callback);
    }

    static void loop() {
        do {
            auto &func = getScheduledFunctions();
            auto scheduled = std::move(func);
            for (auto &func : scheduled) {
                func();
            }
        } while (getScheduledFunctions().size());
    }

};
