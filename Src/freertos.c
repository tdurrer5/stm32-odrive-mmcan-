/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * File Name          : freertos.c
  * Description        : Code for freertos applications
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; Copyright (c) 2020 STMicroelectronics.
  * All rights reserved.</center></h2>
  *
  * This software component is licensed by ST under Ultimate Liberty license
  * SLA0044, the "License"; You may not use this file except in compliance with
  * the License. You may obtain a copy of the License at:
  *                             www.st.com/SLA0044
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include <stdio.h> //tdu for printf ?


#include "FreeRTOS.h"
#include "task.h"
#include "main.h"
#include "cmsis_os.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */     
#include "display.h"
#include "odrive_can.h"

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN Variables */
int pb7_state, pb7_led_count;
/* USER CODE END Variables */
/* Definitions for controlTask */
osThreadId_t controlTaskHandle;
const osThreadAttr_t controlTask_attributes = {
  .name = "controlTask",
  .priority = (osPriority_t) osPriorityAboveNormal,
  .stack_size = 128 * 4
};
/* Definitions for displayTask */
osThreadId_t displayTaskHandle;
const osThreadAttr_t displayTask_attributes = {
  .name = "displayTask",
  .priority = (osPriority_t) osPriorityLow,
  .stack_size = 128 * 4
};
/* Definitions for canTask */
osThreadId_t canTaskHandle;
const osThreadAttr_t canTask_attributes = {
  .name = "canTask",
  .priority = (osPriority_t) osPriorityNormal,
  .stack_size = 256 * 4
};
/* Definitions for canCommandsQueue */
osMessageQueueId_t canCommandsQueueHandle;
const osMessageQueueAttr_t canCommandsQueue_attributes = {
  .name = "canCommandsQueue"
};
/* Definitions for canRxBuffer */
osMessageQueueId_t canRxBufferHandle;
const osMessageQueueAttr_t canRxBuffer_attributes = {
  .name = "canRxBuffer"
};
/* Definitions for canSendTimer */
osTimerId_t canSendTimerHandle;
const osTimerAttr_t canSendTimer_attributes = {
  .name = "canSendTimer"
};
/* Definitions for canInterruptBinarySem */
osSemaphoreId_t canInterruptBinarySemHandle;
const osSemaphoreAttr_t canInterruptBinarySem_attributes = {
  .name = "canInterruptBinarySem"
};
/* Definitions for displayBinarySem */
osSemaphoreId_t displayBinarySemHandle;
const osSemaphoreAttr_t displayBinarySem_attributes = {
  .name = "displayBinarySem"
};

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN FunctionPrototypes */

/* USER CODE END FunctionPrototypes */

void controlTaskStart(void *argument);
void diplayTaskStart(void *argument);
void canTaskStart(void *argument);
void canSendTimerCallback(void *argument);

void MX_FREERTOS_Init(void); /* (MISRA C 2004 rule 8.1) */

/**
  * @brief  FreeRTOS initialization
  * @param  None
  * @retval None
  */
void MX_FREERTOS_Init(void) {
  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* USER CODE BEGIN RTOS_MUTEX */
  /* add mutexes, ... */
  /* USER CODE END RTOS_MUTEX */

  /* Create the semaphores(s) */
  /* creation of canInterruptBinarySem */
  canInterruptBinarySemHandle = osSemaphoreNew(1, 1, &canInterruptBinarySem_attributes);

  /* creation of displayBinarySem */
  displayBinarySemHandle = osSemaphoreNew(1, 1, &displayBinarySem_attributes);

  /* USER CODE BEGIN RTOS_SEMAPHORES */
  // Grab the semaphore so the diplay task won't update until released
  osSemaphoreAcquire(displayBinarySemHandle, 0);
  /* add semaphores, ... */
  /* USER CODE END RTOS_SEMAPHORES */

  /* Create the timer(s) */
  /* creation of canSendTimer */
  canSendTimerHandle = osTimerNew(canSendTimerCallback, osTimerPeriodic, NULL, &canSendTimer_attributes);

  /* USER CODE BEGIN RTOS_TIMERS */
  /* start timers, add new ones, ... */
  osTimerStart(canSendTimerHandle, 10);
  /* USER CODE END RTOS_TIMERS */

  /* Create the queue(s) */
  /* creation of canCommandsQueue */
  canCommandsQueueHandle = osMessageQueueNew (8, sizeof(uint16_t), &canCommandsQueue_attributes);

  /* creation of canRxBuffer */
  canRxBufferHandle = osMessageQueueNew (8, sizeof(CanMessage_t), &canRxBuffer_attributes);

  /* USER CODE BEGIN RTOS_QUEUES */
  /* add queues, ... */
  /* USER CODE END RTOS_QUEUES */

  /* Create the thread(s) */
  /* creation of controlTask */
  controlTaskHandle = osThreadNew(controlTaskStart, NULL, &controlTask_attributes);

  /* creation of displayTask */
  displayTaskHandle = osThreadNew(diplayTaskStart, NULL, &displayTask_attributes);

  /* creation of canTask */
  canTaskHandle = osThreadNew(canTaskStart, NULL, &canTask_attributes);

  /* USER CODE BEGIN RTOS_THREADS */
  /* add threads, ... */
  /* USER CODE END RTOS_THREADS */

}

/* USER CODE BEGIN Header_controlTaskStart */
/**
  * @brief  Function implementing the controlTask thread.
  * @param  argument: Not used 
  * @retval None
  */
/* USER CODE END Header_controlTaskStart */
void controlTaskStart(void *argument)
{
  /* USER CODE BEGIN controlTaskStart */
  // Adding display related task to start of the control task as it has higher priority
  display_init();
  odrive_can_write(AXIS_0, MSG_CLEAR_ERRORS);
  osDelay(1000);
//  odrive_set_axis0.requested_state = AXIS_STATE_FULL_CALIBRATION_SEQUENCE;
  odrive_set_axis0.requested_state = AXIS_STATE_ENCODER_INDEX_SEARCH; //tdu
  odrive_can_write(AXIS_0, MSG_SET_AXIS_REQUESTED_STATE);
  printf("search index \n");
  osDelay(100);
  while (odrive_get_axis0.axis_current_state != AXIS_STATE_IDLE)
  {
    osDelay(100);
  }
  odrive_set_axis0.requested_state = AXIS_STATE_CLOSED_LOOP_CONTROL;
  odrive_can_write(AXIS_0, MSG_SET_AXIS_REQUESTED_STATE);
  printf("close loop \n");
  
  /* Infinite loop */
  for (;;)
  {
    uint32_t count = osKernelGetTickCount();
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_7, RESET); //turn CN10.19 LED on

    //display_add_float_line("Ticks", count, 1);
    printf("ticks%d",count);
    uint32_t msg_count = osMessageQueueGetCount(canRxBufferHandle);
    printf("msg_count %d \n",msg_count); //tdu
    //display_add_float_line("MsgQ", msg_count, 2);
    // display_add_float_line("Vbus", odrive_state.vbus_voltage, 3);
    //display_add_float_line("Enc", odrive_get_axis0.encoder_pos_estimate, 4);
    printf("enc f %0.2f \n",odrive_get_axis0.encoder_pos_estimate);
    //display_add_float_line("State", odrive_get_axis0.axis_current_state, 5);
    printf("req state %d \n",odrive_get_axis0.axis_current_state);
    osSemaphoreRelease(displayBinarySemHandle);
    osDelay(1000);
    //--- motion loop start -- tdu
    // read in axis positions
    // pos PID
    // get next delta pos seg from trajec_gen
    // send new pos to CAN nodes
  }
  /* USER CODE END controlTaskStart */
}

/* USER CODE BEGIN Header_diplayTaskStart */
/**
* @brief Function implementing the displayTask thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_diplayTaskStart */
void diplayTaskStart(void *argument)
{
  /* USER CODE BEGIN diplayTaskStart */
  /* Infinite loop */
  for (;;)
  {
    // Wait for another thread to release the semaphore which will call a display update
    osSemaphoreAcquire(displayBinarySemHandle, osWaitForever);
    display_update();

    if(pb7_led_count>2)
    {  
      if(pb7_state==1){  
        HAL_GPIO_WritePin(GPIOC, GPIO_PIN_7, SET); //turn CN10.19 LED on
        pb7_state=0;
      } else{
        HAL_GPIO_WritePin(GPIOC, GPIO_PIN_7, RESET); //turn CN10.19 LED on
        pb7_state=1;

      }
      pb7_led_count=0;
    }
    pb7_led_count++;
    //osDelay(1);
  }
  /* USER CODE END diplayTaskStart */
}

/* USER CODE BEGIN Header_canTaskStart */
/**
* @brief Function implementing the canTask thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_canTaskStart */
void canTaskStart(void *argument)
{
  /* USER CODE BEGIN canTaskStart */
  /* Infinite loop */
  for (;;)
  {
    CanMessage_t msg;
    osMessageQueueGet(canRxBufferHandle, &msg, NULL, osWaitForever);
    odrive_handle_msg(&msg);
  }
  /* USER CODE END canTaskStart */
}

/* canSendTimerCallback function */
void canSendTimerCallback(void *argument)
{
  /* USER CODE BEGIN canSendTimerCallback */
  odrive_can_write(AXIS_0, MSG_GET_ENCODER_ESTIMATES);

  /* USER CODE END canSendTimerCallback */
}

/* Private application code --------------------------------------------------*/
/* USER CODE BEGIN Application */

/* USER CODE END Application */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
