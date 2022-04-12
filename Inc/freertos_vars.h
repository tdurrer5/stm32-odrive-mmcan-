/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __FREERTOS_H
#define __FREERTOS_H

#include "cmsis_os.h"

// List of semaphores
extern osTimerId_t canSendTimerHandle;
extern osThreadId_t controlTaskHandle;
extern osThreadId_t displayTaskHandle;
extern osThreadId_t canTaskHandle;
extern osSemaphoreId_t canInterruptBinarySemHandle;
extern osSemaphoreId_t displayBinarySem;
extern osMessageQueueId_t canRxBufferHandle;

#endif /* __FREERTOS_H */