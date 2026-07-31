/*
 * usbd_uac diagnostics interface — safe to include without pulling in the
 * ST USBD stack headers (usbd_def.h et al), so host tests can link.
 */

#ifndef USBD_UAC_DIAG_H
#define USBD_UAC_DIAG_H

#include <stdbool.h>
#include <stdint.h>

typedef struct
{
    uint32_t data_out_isr_total;   /* Every entry to usbd_uac_data_out */
    uint32_t data_out_pushed;      /* Reached push_bytes (past all guards) */
    uint32_t data_out_stream_bad;  /* Stream ptr failed range sanity check */
    uint32_t data_out_size_zero;   /* Received size was 0 or clamped to 0 */
    uint32_t data_out_size_oversz; /* Received size > max packet (clamped) */
    uint32_t sof_count;            /* SOF ticks (=ms since AS-OUT alt open) */
    uint32_t last_received;        /* Last raw COUNT_RX value from BTABLE */
    uint32_t stream_repair_count;  /* Times h->stream was self-repaired */
    uint32_t as_in_tx;             /* USBD_LL_Transmit calls on AS-IN */
    uint32_t as_in_tx_err;         /* USBD_LL_Transmit non-OK returns on AS-IN */
    uint32_t as_in_datain;         /* CTR_TX (data_in) callbacks for AS-IN */
    uint32_t as_in_sof_active;     /* SOFs elapsed while AS-IN was open */
    uint32_t as_in_tx_long;        /* AS-IN packets carrying the extra sample */
    uint32_t as_in_underrun;       /* AS-IN packets that needed zero-fill */
    uint16_t setalt_out_1;         /* SET_INTERFACE iface=AS-OUT alt=1 count */
    uint16_t setalt_out_0;         /* SET_INTERFACE iface=AS-OUT alt=0 count */
    uint16_t setalt_in_1;          /* SET_INTERFACE iface=AS-IN  alt=1 count */
    uint16_t setalt_in_0;          /* SET_INTERFACE iface=AS-IN  alt=0 count */
    bool as_out_active;
    bool as_in_active;
} UsbdUacDiag;

void usbd_uac_get_diag(UsbdUacDiag *out);
void usbd_uac_reset_diag(void);

#endif /* USBD_UAC_DIAG_H */
