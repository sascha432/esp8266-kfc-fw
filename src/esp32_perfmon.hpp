/**
 * Author: sascha_lammers@gmx.de
 */

#include <Arduino_compat.h>
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_freertos_hooks.h"
#include "esp_log.h"
#include "sdkconfig.h"

static TaskHandle_t perfmonTask = nullptr;
static constexpr int kDisplayCpuIntervalMillis = 5000;

#define USE_IDLE_HOOK 0

#if USE_IDLE_HOOK

static uint64_t idle0Calls = 0;
static uint64_t idle1Calls = 0;

#if F_CPU == 240000000L
    constexpr static const uint64_t MaxIdleCalls = 1855000;
#elif F_CPU == 160000000L
    constexpr static const uint64_t MaxIdleCalls = 1233100;
#else
#    error "Unsupported CPU frequency"
#endif

static bool idle_task_0()
{
	idle0Calls += 1;
	return false;
}

static bool idle_task_1()
{
	idle1Calls += 1;
	return false;
}

static void perfmon_task(void *args)
{
    Stream &output = *reinterpret_cast<Stream *>(args);
	while (1) {
        float idle0 = idle0Calls;
        float idle1 = idle1Calls;
        idle0Calls = 0;
        idle1Calls = 0;

        int cpu0 = 100.f - idle0 / MaxIdleCalls * 100.f;
        int cpu1 = 100.f - idle1 / MaxIdleCalls * 100.f;

        output.printf("Core 0 at %d%%, core 1 at %d%%\n", cpu0, cpu1);

		vTaskDelay(kDisplayCpuIntervalMillis / portTICK_PERIOD_MS);
	}
	vTaskDelete(NULL);
}

#elif USE_IDLE_HOOK == 0

const int CPU0_LOAD_START_BIT = BIT0;
const int CPU1_LOAD_START_BIT = BIT1;

EventGroupHandle_t cpu_load_event_group;
ulong idleCnt = 0;

void idleCPU0Task(void *parm)
{
    ulong now, now2;

    while (true) {
        // Wait here if not enabled
        xEventGroupWaitBits(cpu_load_event_group, CPU0_LOAD_START_BIT, false, false, portMAX_DELAY);
        now = millis(); // esp_timer_get_time();
        vTaskDelay(0 / portTICK_RATE_MS);
        now2 = millis(); // esp_timer_get_time();
        idleCnt += (now2 - now); // accumulate the usec's
    }
}

void idleCPU1Task(void *parm)
{
    ulong now, now2;

    while (true) {
        // Wait here if not enabled
        xEventGroupWaitBits(cpu_load_event_group, CPU1_LOAD_START_BIT, false, false, portMAX_DELAY);
        now = millis();
        vTaskDelay(0 / portTICK_RATE_MS);
        now2 = millis();
        idleCnt += (now2 - now); // accumulate the msec's while enabled
    }
}

void perfmon_task(void *args)
{
    Stream &output = *reinterpret_cast<Stream *>(args);
    // float adjust = 1.11; //900msec = 100% - this doesnt seem to work
    float adjust = 1.00;

    cpu_load_event_group = xEventGroupCreate();
    xEventGroupClearBits(cpu_load_event_group, CPU0_LOAD_START_BIT);
    xEventGroupClearBits(cpu_load_event_group, CPU1_LOAD_START_BIT);
    xTaskCreatePinnedToCore(&idleCPU0Task, "idleCPU0Task", configMINIMAL_STACK_SIZE, NULL, 1, NULL, 0); // lowest priority CPU 0 task
    xTaskCreatePinnedToCore(&idleCPU1Task, "idleCPU1Task", configMINIMAL_STACK_SIZE, NULL, 1, NULL, 1); // lowest priority CPU 1 task

    while (true) {
        // measure CPU0
        idleCnt = 0; // Reset usec timer
        xEventGroupSetBits(cpu_load_event_group, CPU0_LOAD_START_BIT); // Signal idleCPU0Task to start timing
        vTaskDelay(1000 / portTICK_RATE_MS); // measure for 1 second
        xEventGroupClearBits(cpu_load_event_group, CPU0_LOAD_START_BIT); // Signal to stop the timing
        vTaskDelay(1 / portTICK_RATE_MS); // make sure idleCnt isnt being used

        // Compensate for the 100 ms delay artifact: 900 msec = 100%
        // cpuPercent = ((99.9 / 90.0) * idleCnt/1000.0) / 10.0;
        float cpuPercent0 = adjust * (float)idleCnt / 10.0;

        // measure CPU1
        idleCnt = 0; // Reset usec timer
        xEventGroupSetBits(cpu_load_event_group, CPU1_LOAD_START_BIT); // Signal idleCPU1Task to start timing
        vTaskDelay(1000 / portTICK_RATE_MS); // measure for 1 second
        xEventGroupClearBits(cpu_load_event_group, CPU1_LOAD_START_BIT); // Signal idle_task to stop the timing
        vTaskDelay(1 / portTICK_RATE_MS); // make sure idleCnt isnt being used

        // Compensate for the 100 ms delay artifact: 900 msec = 100%
        // cpuPercent = ((99.9 / 90.0) * idleCnt/1000.0) / 10.0;
        float cpuPercent1 = adjust * (float)idleCnt / 10.0;
        output.printf("Core 0 at %.0f%%, core 1 at %.0f%%, total %.0f%%\n", 100 - cpuPercent0, 100 - cpuPercent1, 100 - ((cpuPercent0 + cpuPercent1) / 2));

        vTaskDelay(kDisplayCpuIntervalMillis / portTICK_RATE_MS); // measure every 10 seconds
    }
    vTaskDelete(NULL);
}

#endif

esp_err_t perfmon_start(Stream *output)
{
    if (perfmonTask) {
        return ESP_ERR_INVALID_STATE;
    }
    #if USE_IDLE_HOOK
        ESP_ERROR_CHECK(esp_register_freertos_idle_hook_for_cpu(idle_task_0, 0));
        ESP_ERROR_CHECK(esp_register_freertos_idle_hook_for_cpu(idle_task_1, 1));
    #endif
    xTaskCreate(perfmon_task, "perfmon", 2048, output, 1, &perfmonTask);
	return ESP_OK;
}

esp_err_t perfmon_stop()
{
    if (!perfmonTask) {
        return ESP_ERR_INVALID_STATE;
    }
    vTaskDelete(perfmonTask);
    perfmonTask = nullptr;
    return ESP_OK;
}
