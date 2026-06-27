#include "fault_handler.h"

#include "stm32f3xx_hal.h"

#include <stdbool.h>
#include <stddef.h>

static const char *fault_type_names[] = {
    [FAULT_TYPE_HARD_FAULT] = "HARD FAULT",
    [FAULT_TYPE_MEM_MANAGE] = "MEM MANAGE",
    [FAULT_TYPE_BUS_FAULT] = "BUS FAULT",
    [FAULT_TYPE_USAGE_FAULT] = "USAGE FAULT",
};

// NOLINTBEGIN(performance-no-int-to-ptr)
static bool uart_ready(void)
{
    if ((FAULT_HANDLER_UART_CLK_EN_REG & FAULT_HANDLER_UART_CLK_EN_MSK) == 0U)
    {
        return false;
    }

    if ((FAULT_HANDLER_UART->CR1 & USART_CR1_UE) == 0U)
    {
        return false;
    }

    return true;
}
// NOLINTEND(performance-no-int-to-ptr)

// NOLINTBEGIN(performance-no-int-to-ptr)
static void uart_send_byte(char c)
{
    while ((FAULT_HANDLER_UART->ISR & USART_ISR_TXE) == 0U)
    {
    }

    FAULT_HANDLER_UART->TDR = (uint8_t)c;
}
// NOLINTEND(performance-no-int-to-ptr)

static void uart_send_str(const char *s)
{
    while (*s != '\0')
    {
        uart_send_byte(*s);
        s++;
    }
}

static void uart_send_hex32(uint32_t val)
{
    static const char hex_digits[] = "0123456789ABCDEF";

    for (int shift = 28; shift >= 0; shift -= 4)
    {
        uart_send_byte(hex_digits[(val >> shift) & 0xFU]);
    }
}

static bool stack_frame_is_valid(uint32_t pc)
{
    return (pc >= FLASH_BASE && pc < (FLASH_BASE + 512U * 1024U)) ||
           (pc >= SRAM_BASE && pc < (SRAM_BASE + 80U * 1024U));
}

static void dump_stacked_frame(const uint32_t *frame, bool is_fpu_stacked)
{
    (void)is_fpu_stacked;

    uart_send_str("R0 :");
    uart_send_hex32(frame[0]);
    uart_send_str("  R1 :");
    uart_send_hex32(frame[1]);
    uart_send_str("  R2 :");
    uart_send_hex32(frame[2]);
    uart_send_str("  R3 :");
    uart_send_hex32(frame[3]);
    uart_send_str("\r\n");

    uart_send_str("R12:");
    uart_send_hex32(frame[4]);
    uart_send_str("  LR :");
    uart_send_hex32(frame[5]);
    uart_send_str("  PC :");
    uart_send_hex32(frame[6]);
    uart_send_str("  PSR:");
    uart_send_hex32(frame[7]);
    uart_send_str("\r\n");
}

typedef struct
{
    uint32_t mask;
    const char *text;
} FaultReasonBit;

static void dump_reason_bits(uint32_t status, const FaultReasonBit *bits, size_t count)
{
    for (size_t i = 0; i < count; i++)
    {
        if (status & bits[i].mask)
        {
            uart_send_str(bits[i].text);
        }
    }
}

static void dump_usage_fault_reason(uint32_t cfsr)
{
    static const FaultReasonBit bits[] = {
        {SCB_CFSR_UNDEFINSTR_Msk, "  - UNDEFINSTR (undefined instruction)\r\n"},
        {SCB_CFSR_INVSTATE_Msk, "  - INVSTATE (invalid state / EPSR)\r\n"},
        {SCB_CFSR_INVPC_Msk, "  - INVPC (invalid PC load)\r\n"},
        {SCB_CFSR_NOCP_Msk, "  - NOCP (no coprocessor)\r\n"},
        {SCB_CFSR_UNALIGNED_Msk, "  - UNALIGNED (unaligned access)\r\n"},
        {SCB_CFSR_DIVBYZERO_Msk, "  - DIVBYZERO (divide by zero)\r\n"},
    };

    dump_reason_bits(cfsr, bits, sizeof(bits) / sizeof(bits[0]));
}

static void dump_bus_fault_reason(uint32_t cfsr)
{
    static const FaultReasonBit bits[] = {
        {SCB_CFSR_IBUSERR_Msk, "  - IBUSERR (instruction bus error)\r\n"},
        {SCB_CFSR_PRECISERR_Msk, "  - PRECISERR (precise data bus error)\r\n"},
        {SCB_CFSR_IMPRECISERR_Msk, "  - IMPRECISERR (imprecise data bus error)\r\n"},
        {SCB_CFSR_UNSTKERR_Msk, "  - UNSTKERR (unstacking error)\r\n"},
        {SCB_CFSR_STKERR_Msk, "  - STKERR (stacking error)\r\n"},
        {SCB_CFSR_LSPERR_Msk, "  - LSPERR (floating-point lazy state preservation)\r\n"},
    };

    dump_reason_bits(cfsr, bits, sizeof(bits) / sizeof(bits[0]));

    if (cfsr & SCB_CFSR_BFARVALID_Msk)
    {
        uart_send_str("  - BFARVALID (BFAR holds valid address)\r\n");
        uart_send_str("BFAR: ");
        uart_send_hex32(SCB->BFAR);
        uart_send_str("\r\n");
    }
}

static void dump_mem_fault_reason(uint32_t cfsr)
{
    static const FaultReasonBit bits[] = {
        {SCB_CFSR_IACCVIOL_Msk, "  - IACCVIOL (instruction access violation)\r\n"},
        {SCB_CFSR_DACCVIOL_Msk, "  - DACCVIOL (data access violation)\r\n"},
        {SCB_CFSR_MUNSTKERR_Msk, "  - MUNSTKERR (unstacking error)\r\n"},
        {SCB_CFSR_MSTKERR_Msk, "  - MSTKERR (stacking error)\r\n"},
        {SCB_CFSR_MLSPERR_Msk, "  - MLSPERR (floating-point lazy state preservation)\r\n"},
    };

    dump_reason_bits(cfsr, bits, sizeof(bits) / sizeof(bits[0]));

    if (cfsr & SCB_CFSR_MMARVALID_Msk)
    {
        uart_send_str("  - MMARVALID (MMFAR holds valid address)\r\n");
        uart_send_str("MMFAR: ");
        uart_send_hex32(SCB->MMFAR);
        uart_send_str("\r\n");
    }
}

static void dump_fault_reasons(uint32_t cfsr, uint32_t hfsr)
{
    static const FaultReasonBit hfsr_bits[] = {
        {SCB_HFSR_FORCED_Msk, "  - FORCED (hard fault escalated from configurable fault)\r\n"},
        {SCB_HFSR_VECTTBL_Msk, "  - VECTTBL (vector table read error)\r\n"},
    };

    uart_send_str("CFSR: ");
    uart_send_hex32(cfsr);
    uart_send_str("  HFSR: ");
    uart_send_hex32(hfsr);
    uart_send_str("\r\n");

    dump_reason_bits(hfsr, hfsr_bits, sizeof(hfsr_bits) / sizeof(hfsr_bits[0]));

    if (cfsr & 0xFFFF0000UL)
    {
        dump_usage_fault_reason(cfsr);
    }
    if (cfsr & 0x0000FF00UL)
    {
        dump_bus_fault_reason(cfsr);
    }
    if (cfsr & 0x000000FFUL)
    {
        dump_mem_fault_reason(cfsr);
    }
}

// NOLINTBEGIN(performance-no-int-to-ptr)
static const uint32_t *find_stacked_frame(uint32_t msp, uint32_t psp,
                                          uint32_t exc_return, bool *out_fpu)
{
    bool used_psp = (exc_return & (1UL << 2)) != 0UL;

    if (!used_psp)
    {
        *out_fpu = (exc_return & (1UL << 4)) == 0UL;
        return (const uint32_t *)msp;
    }

    if (psp != 0U && stack_frame_is_valid(((const uint32_t *)psp)[6]))
    {
        *out_fpu = (exc_return & (1UL << 4)) == 0UL;
        return (const uint32_t *)psp;
    }

    *out_fpu = false;
    return (const uint32_t *)msp;
}
// NOLINTEND(performance-no-int-to-ptr)

__attribute__((noreturn)) void fault_handler_dump(uint32_t msp, uint32_t psp, uint32_t exc_return,
                                                  FaultHandler_FaultType fault_type)
{
    if (uart_ready())
    {
        uart_send_str("\r\n=== ");
        uart_send_str(fault_type_names[fault_type]);
        uart_send_str(" ===\r\n");

        bool fpu_stacked = false;
        const uint32_t *frame =
            find_stacked_frame(msp, psp, exc_return, &fpu_stacked);

        dump_stacked_frame(frame, fpu_stacked);

        uint32_t cfsr = SCB->CFSR;
        uint32_t hfsr = SCB->HFSR;
        dump_fault_reasons(cfsr, hfsr);
    }

    while (1)
    {
    }
}
