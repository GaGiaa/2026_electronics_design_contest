#ifndef STM32H723_SERVICE_TEST_FDCAN_H
#define STM32H723_SERVICE_TEST_FDCAN_H

#include <stdint.h>

typedef enum {
    HAL_OK = 0,
    HAL_ERROR = 1,
} HAL_StatusTypeDef;

typedef struct {
    uint32_t instance;
} FDCAN_HandleTypeDef;

typedef struct {
    uint32_t IdType;
    uint32_t DataLength;
    uint32_t Identifier;
} FDCAN_RxHeaderTypeDef;

typedef struct {
    uint32_t Identifier;
    uint32_t IdType;
    uint32_t TxFrameType;
    uint32_t DataLength;
    uint32_t ErrorStateIndicator;
    uint32_t BitRateSwitch;
    uint32_t FDFormat;
    uint32_t TxEventFifoControl;
    uint32_t MessageMarker;
} FDCAN_TxHeaderTypeDef;

typedef struct {
    uint32_t IdType;
    uint32_t FilterIndex;
    uint32_t FilterType;
    uint32_t FilterConfig;
    uint32_t FilterID1;
    uint32_t FilterID2;
} FDCAN_FilterTypeDef;

typedef struct {
    uint32_t LastErrorCode;
    uint32_t Activity;
    uint32_t BusOff;
} FDCAN_ProtocolStatusTypeDef;

typedef struct {
    uint32_t TxErrorCnt;
    uint32_t RxErrorCnt;
} FDCAN_ErrorCountersTypeDef;

extern FDCAN_HandleTypeDef hfdcan1;
extern FDCAN_HandleTypeDef hfdcan2;
extern FDCAN_HandleTypeDef hfdcan3;

#define FDCAN_STANDARD_ID 1U
#define FDCAN_DLC_BYTES_8 8U
#define FDCAN_RX_FIFO0 0U
#define FDCAN_FILTER_RANGE 0U
#define FDCAN_FILTER_TO_RXFIFO0 0U
#define FDCAN_IT_RX_FIFO0_NEW_MESSAGE 0U
#define FDCAN_DATA_FRAME 0U
#define FDCAN_ESI_ACTIVE 0U
#define FDCAN_BRS_OFF 0U
#define FDCAN_CLASSIC_CAN 0U
#define FDCAN_NO_TX_EVENTS 0U
#define DMA_IT_HT 0U

HAL_StatusTypeDef HAL_FDCAN_GetProtocolStatus(FDCAN_HandleTypeDef *fdcan,
                                                FDCAN_ProtocolStatusTypeDef *status);
HAL_StatusTypeDef HAL_FDCAN_GetErrorCounters(FDCAN_HandleTypeDef *fdcan,
                                               FDCAN_ErrorCountersTypeDef *counters);
HAL_StatusTypeDef HAL_FDCAN_ConfigFilter(FDCAN_HandleTypeDef *fdcan,
                                          const FDCAN_FilterTypeDef *filter);
HAL_StatusTypeDef HAL_FDCAN_Start(FDCAN_HandleTypeDef *fdcan);
HAL_StatusTypeDef HAL_FDCAN_ActivateNotification(FDCAN_HandleTypeDef *fdcan,
                                                   uint32_t active_it,
                                                   uint32_t buffer);
uint32_t HAL_FDCAN_GetRxFifoFillLevel(const FDCAN_HandleTypeDef *fdcan,
                                      uint32_t rx_fifo);
HAL_StatusTypeDef HAL_FDCAN_GetRxMessage(FDCAN_HandleTypeDef *fdcan,
                                         uint32_t rx_fifo,
                                         FDCAN_RxHeaderTypeDef *header,
                                         uint8_t data[8]);
HAL_StatusTypeDef HAL_FDCAN_AddMessageToTxFifoQ(FDCAN_HandleTypeDef *fdcan,
                                                 const FDCAN_TxHeaderTypeDef *header,
                                                 const uint8_t data[8]);

#endif
