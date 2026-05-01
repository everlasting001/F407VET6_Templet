#include "UartBase.h"
#include <string.h>

/* ==================== 默认虚函数实现 ==================== */

static int UartBase_defaultInit(void *self)
{
    UartBase_t *uart = (UartBase_t *)self;
    if (uart == NULL || uart->huart == NULL || uart->rx_buffer == NULL)
        return -1;

    if (UartBase_StartRx(uart) != 0)
        return -1;

    __HAL_UART_CLEAR_IDLEFLAG(uart->huart);
    return 0;
}

static int UartBase_defaultRun(void *self)
{
    (void)self;
    return 0;
}

static int UartBase_defaultCleanup(void *self)
{
    UartBase_t *uart = (UartBase_t *)self;
    if (uart == NULL || uart->huart == NULL)
        return -1;

    HAL_UART_DMAStop(uart->huart);
    uart->rx_done = 0;
    uart->rx_len  = 0;
    return 0;
}

static const UartVTable_t default_vtable = {
    .init    = UartBase_defaultInit,
    .run     = UartBase_defaultRun,
    .cleanup = UartBase_defaultCleanup,
};

/* ==================== 公有接口实现 ==================== */

void UartBase_Constructor(UartBase_t *self, const char *name,
                          UART_HandleTypeDef *huart,
                          uint8_t *rx_buffer, uint16_t rx_buf_size)
{
    if (self == NULL) return;

    self->vtable       = &default_vtable;
    self->name         = name;
    self->initialized  = 0;
    self->huart        = huart;
    self->rx_buffer    = rx_buffer;
    self->rx_buf_size  = rx_buf_size;
    self->rx_len       = 0;
    self->rx_done      = 0;
    self->tx_busy      = 0;
}

int UartBase_Init(UartBase_t *self)
{
    if (self == NULL || self->vtable == NULL)
        return -1;
    if (self->initialized)
        return 0;

    int ret = 0;
    if (self->vtable->init != NULL)
        ret = self->vtable->init((void *)self);

    if (ret == 0)
        self->initialized = 1;
    return ret;
}

int UartBase_Run(UartBase_t *self)
{
    if (self == NULL || self->vtable == NULL)
        return -1;
    if (!self->initialized)
        return -1;
    if (self->vtable->run != NULL)
        return self->vtable->run((void *)self);
    return 0;
}

int UartBase_Cleanup(UartBase_t *self)
{
    if (self == NULL || self->vtable == NULL)
        return -1;
    if (!self->initialized)
        return 0;

    int ret = 0;
    if (self->vtable->cleanup != NULL)
        ret = self->vtable->cleanup((void *)self);

    self->initialized = 0;
    return ret;
}

int UartBase_SendDMA(UartBase_t *self, const uint8_t *data, uint16_t len)
{
    if (self == NULL || data == NULL || len == 0)
        return -1;
    if (self->tx_busy)
        return -1;
    if (!self->initialized)
        return -1;

    self->tx_busy = 1;
    if (HAL_UART_Transmit_DMA(self->huart, (uint8_t *)data, len) != HAL_OK) {
        self->tx_busy = 0;
        return -1;
    }
    return 0;
}

int UartBase_SendStr(UartBase_t *self, const char *str)
{
    if (self == NULL || str == NULL)
        return -1;
    return UartBase_SendDMA(self, (const uint8_t *)str, (uint16_t)strlen(str));
}

int UartBase_StartRx(UartBase_t *self)
{
    if (self == NULL || self->huart == NULL || self->rx_buffer == NULL)
        return -1;

    self->rx_done = 0;
    self->rx_len  = 0;

    /* 使能 IDLE 中断 (HAL_UARTEx_ReceiveToIdle_DMA 内部也会设置, 此处显式保证) */
    __HAL_UART_ENABLE_IT(self->huart, UART_IT_IDLE);

    /* 确保 NVIC 中断已使能 (MspInit 中已使能, 此处保险) */
    if (self->huart->Instance == USART1) {
        HAL_NVIC_EnableIRQ(USART1_IRQn);
    } else if (self->huart->Instance == USART2) {
        HAL_NVIC_EnableIRQ(USART2_IRQn);
    }

    if (HAL_UARTEx_ReceiveToIdle_DMA(self->huart, self->rx_buffer,
                                     self->rx_buf_size) != HAL_OK) {
        return -1;
    }
    return 0;
}

void UartBase_RxIdleCallback(UartBase_t *self, uint16_t len)
{
    if (self == NULL || len == 0) return;

    self->rx_len  = len;
    self->rx_done = 1;

    /* 重启动 DMA+IDLE 以持续接收下一帧 */
    UartBase_StartRx(self);
}

void UartBase_TxCpltCallback(UartBase_t *self)
{
    if (self == NULL) return;
    self->tx_busy = 0;
}
