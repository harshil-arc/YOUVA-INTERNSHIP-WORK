#pragma once

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

// Runs Wi-Fi and MQTT independently from sensor acquisition.
void networkTask(void* queueParameter);