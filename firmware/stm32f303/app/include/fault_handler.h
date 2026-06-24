#ifndef FAULT_HANDLER_H
#define FAULT_HANDLER_H

#include <stdint.h>

// Overwrite for your platform if needed
#ifndef FAULT_HANDLER_UART
#define FAULT_HANDLER_UART USART2
#endif

#ifndef FAULT_HANDLER_UART_CLK_EN_REG
#define FAULT_HANDLER_UART_CLK_EN_REG RCC->APB1ENR
#endif

#ifndef FAULT_HANDLER_UART_CLK_EN_MSK
#define FAULT_HANDLER_UART_CLK_EN_MSK RCC_APB1ENR_USART2EN
#endif

typedef enum
{
    FAULT_TYPE_HARD_FAULT = 0,
    FAULT_TYPE_MEM_MANAGE = 1,
    FAULT_TYPE_BUS_FAULT = 2,
    FAULT_TYPE_USAGE_FAULT = 3,
} FaultHandler_FaultType;

__attribute__((noreturn)) void fault_handler_dump(uint32_t msp, uint32_t psp, uint32_t exc_return,
                                                  FaultHandler_FaultType fault_type);

#endif
