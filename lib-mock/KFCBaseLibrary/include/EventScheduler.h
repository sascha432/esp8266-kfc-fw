/**
  Author: sascha_lammers@gmx.de
*/

#if _MSC_VER

#pragma once

 class EventTimer {
 public:
     void detach() {}
 };

 class EventScheduler {
 public:
     typedef EventTimer *TimerPtr;
     class Timer {
     public:
         Timer() {
         }
         void remove() {
         }
     };
 };

#endif
