#pragma once
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

TaskHandle_t print_runtime_stats_Hdl;

inline void print_system_stats(void *pvParameters)
{
    // 2KB is usually enough for ~20-25 tasks.
    // If you have a massive app, increase this.
    char *stats_buffer = (char *)malloc(2048);
    if (stats_buffer == NULL)
    {
        printf("Failed to allocate memory for stats\n");
        return;
    }

    while (1)
    {
        ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(5000));
        printf("\n======================================================\n");
        printf("Task Name       State   Pri     Stack    Num    Core\n");
        printf("------------------------------------------------------\n");
        /* vTaskList shows:
           Name, State (R=Running, B=Blocked, S=Suspended, D=Deleted),
           Priority, Stack High Water Mark, Task Number, Core ID
        */
        vTaskList(stats_buffer);
        printf("%s", stats_buffer);

        printf("\n------------------------------------------------------\n");
        printf("Task Name       Abs Time (ticks)        CPU %%\n");
        printf("------------------------------------------------------\n");
        /* vTaskGetRunTimeStats shows the CPU time each task has consumed
         */
        vTaskGetRunTimeStats(stats_buffer);
        printf("%s", stats_buffer);
        printf("======================================================\n");
    }

    free(stats_buffer);
}