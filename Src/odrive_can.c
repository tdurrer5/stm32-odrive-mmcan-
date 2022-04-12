#include "stm32f4xx_hal.h"
#include "stm32f4xx_hal_can.h"
#include "can.h"
#include "odrive_can.h"
#include "freertos_vars.h"
#include "cmsis_os.h"
#include "string.h"

uint8_t odrive_can_init(Axis_t axis)
{
    CAN_FilterTypeDef filter;
    filter.FilterActivation = ENABLE;
    filter.FilterBank = 0;
    filter.FilterFIFOAssignment = CAN_RX_FIFO0;
    filter.FilterIdHigh = 0x0000;
    filter.FilterIdLow = 0x0000;
    filter.FilterMaskIdHigh = 0x0000;
    filter.FilterMaskIdLow = 0x0000;
    filter.FilterMode = CAN_FILTERMODE_IDMASK;
    filter.FilterScale = CAN_FILTERSCALE_32BIT;

    HAL_StatusTypeDef status = HAL_CAN_ConfigFilter(&hcan1, &filter);
    if (status != HAL_OK)
    {
        /* Start Error */
        Error_Handler();
        return 1;
    }

    status = HAL_CAN_Start(&hcan1);

    if (status != HAL_OK)
    {
        /* Start Error */
        Error_Handler();
        return 1;
    }

    HAL_CAN_ActivateNotification(&hcan1, CAN_IT_RX_FIFO0_MSG_PENDING);
    return 0;
}

uint8_t odrive_handle_msg(CanMessage_t *msg)
{
    uint8_t first_word[4];
    uint8_t second_word[4];
    if (msg->len == 8)
    {
        memcpy(&first_word, &msg->buf, 4);
        second_word[3] = msg->buf[7];
        second_word[2] = msg->buf[6];
        second_word[1] = msg->buf[5];
        second_word[0] = msg->buf[4];
    }
    else if (msg->len == 4)
    {
        memcpy(&first_word, &msg->buf, 4);
    }

    OdriveAxisGetState_t *odrive_get;
    if ((msg->id & AXIS0_NODE_ID) == AXIS0_NODE_ID) //  Mask off the first 5 bits of the 11 bit id
    {
        odrive_get = &odrive_get_axis0;
    }
    else if ((msg->id & AXIS1_NODE_ID) == AXIS1_NODE_ID)
    {
        odrive_get = &odrive_get_axis1;
    }
    else
    {
        return 1; // Not an axis
    }

    if (msg->rtr == 0)
    {
        switch (msg->id & 0b11111) // Mask out the first 5 bits
        {
        case (MSG_ODRIVE_HEARTBEAT):
            memcpy(&odrive_get->axis_error, &first_word, sizeof(uint32_t));
            memcpy(&odrive_get->axis_current_state, &second_word, sizeof(uint32_t));
            break;
        case (MSG_GET_ENCODER_ESTIMATES):
            memcpy(&odrive_get->encoder_pos_estimate, &first_word, sizeof(float));
            memcpy(&odrive_get->encoder_vel_estimate, &second_word, sizeof(float));
            break;
        case (MSG_GET_ENCODER_COUNT):
            memcpy(&odrive_get->encoder_shadow_count, &first_word, sizeof(uint32_t));
            memcpy(&odrive_get->encoder_cpr_count, &second_word, sizeof(uint32_t));
            break;
        case (MSG_GET_MOTOR_ERROR):
            memcpy(&odrive_get->motor_error, &first_word, sizeof(uint32_t));
            break;
        case (MSG_GET_ENCODER_ERROR):
            memcpy(&odrive_get->encoder_error, &first_word, sizeof(uint32_t));
            break;
        case (MSG_GET_VBUS_VOLTAGE):
            memcpy(&odrive_state.vbus_voltage, &first_word, sizeof(float));
            break;
        default:
            return 1;
            break;
        }
    }

    return 0;
}

uint8_t odrive_can_write(Axis_t axis, OdriveMsg_t msg)
{
    uint32_t can_error = HAL_CAN_GetError(&hcan1);
    if (can_error == HAL_CAN_ERROR_NONE)
    {
        CAN_TxHeaderTypeDef header;
        header.IDE = CAN_ID_STD;
        uint8_t data[8];
        uint8_t tmp_word[4]; // TODO: Get rid of this with better mem management
        OdriveAxisSetState_t *odrive_set;
        if (axis == AXIS_0)
        {
            header.StdId = AXIS0_NODE_ID + msg;
            odrive_set = &odrive_set_axis0;
        }
        else if (axis == AXIS_1)
        {
            header.StdId = AXIS1_NODE_ID + msg;
            odrive_set = &odrive_set_axis1;
        }
        else if (axis == AXIS_2)
        {
            header.StdId = AXIS2_NODE_ID + msg;
            odrive_set = &odrive_set_axis2;
        }
        else if (axis == AXIS_3)
        {
            header.StdId = AXIS3_NODE_ID + msg;
            odrive_set = &odrive_set_axis3;
        }
        else
        {
            return -1;
        }

        switch (msg)
        {
        case MSG_ODRIVE_ESTOP:
            /* TODO: Implement */
            break;
        case MSG_GET_MOTOR_ERROR:
            header.RTR = CAN_RTR_REMOTE;
            header.DLC = 0;
            break;
        case MSG_GET_ENCODER_ERROR:
            header.RTR = CAN_RTR_REMOTE;
            header.DLC = 0;
            break;
        case MSG_GET_SENSORLESS_ERROR:
            /* TODO: Implement */
            break;
        case MSG_SET_AXIS_NODE_ID:
            /* TODO: Implement */
            break;
        case MSG_SET_AXIS_REQUESTED_STATE:
            memcpy(&data, &odrive_set->requested_state, 4);
            header.RTR = CAN_RTR_DATA;
            header.DLC = 4;
            break;
        case MSG_SET_AXIS_STARTUP_CONFIG:
            /* TODO: Implement */
            break;
        case MSG_GET_ENCODER_ESTIMATES:
            header.RTR = CAN_RTR_REMOTE;
            header.DLC = 0;
            break;
        case MSG_GET_ENCODER_COUNT:
            header.RTR = CAN_RTR_REMOTE;
            header.DLC = 0;
            break;
        case MSG_SET_CONTROLLER_MODES:
            data[0] = odrive_set->control_mode;
            data[4] = odrive_set->input_mode;
            header.RTR = CAN_RTR_DATA;
            header.DLC = 8;
            break;
        case MSG_SET_INPUT_POS:
            memcpy(&data, &odrive_set->input_pos, 4);
            data[4] = odrive_set->vel_ff & 0x00FF;
            data[5] = odrive_set->vel_ff >> 8;
            data[6] = odrive_set->current_ff & 0x00FF;
            data[7] = odrive_set->current_ff >> 8;
            header.RTR = CAN_RTR_DATA;
            header.DLC = 8;
            break;
        case MSG_SET_INPUT_VEL:
            memcpy(&data, &odrive_set->input_vel, 4);
            data[4] = odrive_set->current_ff & 0x00FF;
            data[5] = odrive_set->current_ff >> 8;
            header.RTR = CAN_RTR_DATA;
            header.DLC = 6;
            break;
        case MSG_SET_INPUT_CURRENT:
            memcpy(&data, &odrive_set->input_current, 4);
            header.RTR = CAN_RTR_DATA;
            header.DLC = 4;
            break;
        case MSG_SET_VEL_LIMIT:
            memcpy(&data, &odrive_set->vel_limit, 4);
            header.RTR = CAN_RTR_DATA;
            header.DLC = 4;
            break;
        case MSG_START_ANTICOGGING:
            header.RTR = CAN_RTR_REMOTE;
            header.DLC = 0;
            break;
        case MSG_SET_TRAJ_VEL_LIMIT:
            memcpy(&data, &odrive_set->traj_vel_limit, 4);
            header.RTR = CAN_RTR_DATA;
            header.DLC = 4;
            break;
        case MSG_SET_TRAJ_ACCEL_LIMITS:
            memcpy(&data, &odrive_set->traj_accel_limit, 4);
            memcpy(&tmp_word, &odrive_set->traj_decel_limit, 4);
            data[4] = tmp_word[0];
            data[5] = tmp_word[1];
            data[6] = tmp_word[2];
            data[7] = tmp_word[3];
            header.RTR = CAN_RTR_DATA;
            header.DLC = 4;
            break;
        case MSG_SET_TRAJ_A_PER_CSS:
            memcpy(&data, &odrive_set->traj_a_per_css, 4);
            header.RTR = CAN_RTR_DATA;
            header.DLC = 4;
            break;
        case MSG_GET_IQ:
            /* TODO: Implement */
            break;
        case MSG_GET_SENSORLESS_ESTIMATES:
            /* TODO: Implement */
            break;
        case MSG_RESET_ODRIVE:
            header.RTR = CAN_RTR_REMOTE;
            header.DLC = 0;
            break;
        case MSG_GET_VBUS_VOLTAGE:
            header.RTR = CAN_RTR_REMOTE;
            header.DLC = 0;
            break;
        case MSG_CLEAR_ERRORS:
            header.RTR = CAN_RTR_REMOTE;
            header.DLC = 0;
            break;
        case MSG_CO_HEARTBEAT_CMD:
            /* TODO: Implement */
            break;
        default:
            break;
        }

        uint32_t retTxMailbox = 0;
        if (HAL_CAN_GetTxMailboxesFreeLevel(&hcan1) > 0)
        {
            HAL_CAN_AddTxMessage(&hcan1, &header, data, &retTxMailbox);
        }

        return 1;
    }
    else
    {
        return can_error;
    }
}

void HAL_CAN_RxFifo0MsgPendingCallback(CAN_HandleTypeDef *hcan)
{
    // HAL_CAN_DeactivateNotification(hcan, CAN_IT_RX_FIFO0_MSG_PENDING);
    // osSemaphoreRelease(canInterruptBinarySemHandle);
    while (HAL_CAN_GetRxFifoFillLevel(hcan, CAN_RX_FIFO0))
    {
        CAN_RxHeaderTypeDef header;
        if (HAL_CAN_GetRxFifoFillLevel(hcan, CAN_RX_FIFO0) > 0)
        {
            CanMessage_t rxmsg;
            HAL_CAN_GetRxMessage(hcan, CAN_RX_FIFO0, &header, rxmsg.buf);

            rxmsg.id = header.StdId;
            rxmsg.len = header.DLC;
            rxmsg.rtr = header.RTR;
            osMessageQueuePut(canRxBufferHandle, &rxmsg, 0, 0);
        }
    }
}