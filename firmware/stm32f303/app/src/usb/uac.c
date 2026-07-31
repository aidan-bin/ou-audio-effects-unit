#include "usb/uac.h"
#include "usb/audio_stream.h"
#include "usb/uac_feedback.h"
#include "usbd_ctlreq.h"
#include "usbd_ioreq.h"

#include <stddef.h>
#include <string.h>

// NOLINTBEGIN(readability-identifier-naming)

/* UAC1 endpoint/interface control request codes (audio10.pdf, 5.2.1) */
#define UAC_REQ_SET_CUR 0x01U
#define UAC_REQ_GET_CUR 0x81U
#define UAC_REQ_SET_MIN 0x02U
#define UAC_REQ_GET_MIN 0x82U
#define UAC_REQ_SET_MAX 0x03U
#define UAC_REQ_GET_MAX 0x83U
#define UAC_REQ_SET_RES 0x04U
#define UAC_REQ_GET_RES 0x84U

/* Matches bRefresh (2^3 = 8ms) in the descriptor, composite.c. */
#define UAC_FEEDBACK_PERIOD_MS 8U

/* in_ring fill level (samples) the feedback controller steers toward. */
#define UAC_IN_RING_TARGET_FILL (UAC_SAMPLES_PER_MS * 8U)

typedef struct
{
    bool initialized;
    bool as_out_active;
    bool as_in_active;

    uint8_t out_rx_buf[UAC_AS_MAX_PACKET_BYTES];
    uint8_t in_tx_buf[UAC_AS_MAX_PACKET_BYTES];
    uint8_t feedback_buf[UAC_FEEDBACK_PACKET_BYTES];

    uint32_t as_in_frac_accum;
    uint32_t sof_count;
    volatile bool as_in_armed;

    uint8_t freq_scratch[3];

    struct UsbAudioStream *stream;

    /* Diagnostics; not touched by hot-path unless needed. */
    volatile uint32_t diag_isr_total;
    volatile uint32_t diag_pushed;
    volatile uint32_t diag_stream_bad;
    volatile uint32_t diag_size_zero;
    volatile uint32_t diag_size_oversz;
    volatile uint32_t diag_last_received;
    volatile uint32_t diag_as_in_tx;
    volatile uint32_t diag_as_in_tx_err;
    volatile uint32_t diag_as_in_datain;
    volatile uint32_t diag_as_in_sof_active;
    volatile uint32_t diag_as_in_tx_long;
    volatile uint32_t diag_as_in_underrun;
    volatile uint16_t diag_setalt_out_0;
    volatile uint16_t diag_setalt_out_1;
    volatile uint16_t diag_setalt_in_0;
    volatile uint16_t diag_setalt_in_1;
} UsbdUacHandleTypeDef;

static UsbdUacHandleTypeDef usbd_uac_handle;

/* Independent copy of the registered stream pointer. Kept in a separate .bss
 * slot so a buffer overflow that scribbles usbd_uac_handle.stream is very
 * unlikely to also touch this. Every hot-path check goes through
 * effective_stream(), which repairs h->stream on the fly and counts how often
 * repair was needed. */
static struct UsbAudioStream *registered_stream;
static volatile uint32_t diag_stream_repair_count;

static struct UsbAudioStream *effective_stream(UsbdUacHandleTypeDef *h)
{
    if (h->stream != registered_stream)
    {
        diag_stream_repair_count++;
        h->stream = registered_stream;
    }
    return h->stream;
}

void usbd_uac_register_stream(struct UsbAudioStream *stream)
{
    registered_stream = stream;
    usbd_uac_handle.stream = stream;
}

uint8_t usbd_uac_class_init(USBD_HandleTypeDef *pdev)
{
    (void)pdev;
    UsbdUacHandleTypeDef *h = &usbd_uac_handle;

    // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
    memset(h, 0, sizeof(*h));
    h->stream = registered_stream;
    h->initialized = true;

    usb_audio_stream_reset(registered_stream);

    return 0U;
}

uint8_t usbd_uac_class_deinit(USBD_HandleTypeDef *pdev)
{
    UsbdUacHandleTypeDef *h = &usbd_uac_handle;

    if (h->as_out_active)
    {
        USBD_LL_CloseEP(pdev, UAC_AS_OUT_EP);
    }
    if (h->as_in_active)
    {
        USBD_LL_CloseEP(pdev, UAC_AS_IN_EP);
    }

    h->as_out_active = false;
    h->as_in_active = false;
    h->initialized = false;
    return 0U;
}

static void usbd_uac_transmit_as_in_packet(USBD_HandleTypeDef *pdev);

static void usbd_uac_open_as_in(USBD_HandleTypeDef *pdev)
{
    UsbdUacHandleTypeDef *h = &usbd_uac_handle;
    if (h->as_in_active)
    {
        return;
    }
    USBD_LL_OpenEP(pdev, UAC_AS_IN_EP, USBD_EP_TYPE_ISOC, UAC_AS_MAX_PACKET_BYTES);
    pdev->ep_in[UAC_AS_IN_EP & 0x7FU].is_used = 1U;
    h->as_in_active = true;
    h->as_in_frac_accum = 0U;
    h->as_in_armed = false;

    /* Prime the endpoint immediately so the host's first IN token gets a
     * packet. From here on, data_in schedules the next packet, so we never
     * overwrite an unsent PMA buffer. */
    usbd_uac_transmit_as_in_packet(pdev);
}

static void usbd_uac_set_as_out_alt(USBD_HandleTypeDef *pdev, uint8_t alt)
{
    UsbdUacHandleTypeDef *h = &usbd_uac_handle;

    if (alt != 0U)
    {
        h->diag_setalt_out_1++;
        if (!h->as_out_active)
        {
            USBD_LL_OpenEP(pdev, UAC_AS_OUT_EP, USBD_EP_TYPE_ISOC, UAC_AS_MAX_PACKET_BYTES);
            pdev->ep_out[UAC_AS_OUT_EP & 0x7FU].is_used = 1U;
            h->as_out_active = true;
            h->sof_count = 0U;
            USBD_LL_PrepareReceive(pdev, UAC_AS_OUT_EP, h->out_rx_buf, UAC_AS_MAX_PACKET_BYTES);
        }
    }
    else if (h->as_out_active)
    {
        h->diag_setalt_out_0++;
        USBD_LL_CloseEP(pdev, UAC_AS_OUT_EP);
        pdev->ep_out[UAC_AS_OUT_EP & 0x7FU].is_used = 0U;
        h->as_out_active = false;
    }
}

static void usbd_uac_set_as_in_alt(USBD_HandleTypeDef *pdev, uint8_t alt)
{
    UsbdUacHandleTypeDef *h = &usbd_uac_handle;

    if (alt != 0U)
    {
        h->diag_setalt_in_1++;
        usbd_uac_open_as_in(pdev);
    }
    else if (h->as_in_active)
    {
        h->diag_setalt_in_0++;
        USBD_LL_CloseEP(pdev, UAC_AS_IN_EP);
        pdev->ep_in[UAC_AS_IN_EP & 0x7FU].is_used = 0U;
        h->as_in_active = false;
    }
}

static uint8_t usbd_uac_setup_endpoint_class(USBD_HandleTypeDef *pdev, USBD_SetupReqTypedef *req)
{
    UsbdUacHandleTypeDef *h = &usbd_uac_handle;
    uint8_t cs = HIBYTE(req->wValue);

    /* Single supported rate: SET_* accepted and ignored, GET_* answer it. */
    if (cs != 0x01U /* SAMPLING_FREQ_CONTROL */)
    {
        USBD_CtlError(pdev, req);
        return USBD_FAIL;
    }

    switch (req->bRequest)
    {
    case UAC_REQ_GET_CUR:
    case UAC_REQ_GET_MIN:
    case UAC_REQ_GET_MAX:
        h->freq_scratch[0] = (uint8_t)(UAC_SAMPLE_RATE_HZ & 0xFFU);
        h->freq_scratch[1] = (uint8_t)((UAC_SAMPLE_RATE_HZ >> 8) & 0xFFU);
        h->freq_scratch[2] = (uint8_t)((UAC_SAMPLE_RATE_HZ >> 16) & 0xFFU);
        USBD_CtlSendData(pdev, h->freq_scratch, 3U);
        break;

    case UAC_REQ_GET_RES:
        h->freq_scratch[0] = 0U;
        h->freq_scratch[1] = 0U;
        h->freq_scratch[2] = 0U;
        USBD_CtlSendData(pdev, h->freq_scratch, 3U);
        break;

    case UAC_REQ_SET_CUR:
    case UAC_REQ_SET_MIN:
    case UAC_REQ_SET_MAX:
    case UAC_REQ_SET_RES:
        USBD_CtlPrepareRx(pdev, h->freq_scratch, 3U);
        break;

    default:
        USBD_CtlError(pdev, req);
        return USBD_FAIL;
    }

    return USBD_OK;
}

uint8_t usbd_uac_setup(USBD_HandleTypeDef *pdev, USBD_SetupReqTypedef *req)
{
    UsbdUacHandleTypeDef *h = &usbd_uac_handle;
    uint8_t recipient = req->bmRequest & USB_REQ_RECIPIENT_MASK;

    if (!h->initialized)
    {
        USBD_CtlError(pdev, req);
        return USBD_FAIL;
    }

    if ((req->bmRequest & USB_REQ_TYPE_MASK) == USB_REQ_TYPE_CLASS &&
        recipient == USB_REQ_RECIPIENT_ENDPOINT)
    {
        return usbd_uac_setup_endpoint_class(pdev, req);
    }

    if ((req->bmRequest & USB_REQ_TYPE_MASK) == USB_REQ_TYPE_STANDARD &&
        recipient == USB_REQ_RECIPIENT_INTERFACE)
    {
        uint8_t iface = LOBYTE(req->wIndex);
        uint8_t alt = LOBYTE(req->wValue);

        switch (req->bRequest)
        {
        case USB_REQ_SET_INTERFACE:
            if (iface == UAC_IFACE_AS_OUT)
            {
                usbd_uac_set_as_out_alt(pdev, alt);
            }
            else if (iface == UAC_IFACE_AS_IN)
            {
                usbd_uac_set_as_in_alt(pdev, alt);
            }
            return USBD_OK;

        case USB_REQ_GET_INTERFACE:
        {
            uint8_t current_alt = 0U;
            if (iface == UAC_IFACE_AS_OUT)
            {
                current_alt = h->as_out_active ? 1U : 0U;
            }
            else if (iface == UAC_IFACE_AS_IN)
            {
                current_alt = h->as_in_active ? 1U : 0U;
            }
            USBD_CtlSendData(pdev, &current_alt, 1U);
            return USBD_OK;
        }

        default:
            break;
        }
    }

    USBD_CtlError(pdev, req);
    return USBD_FAIL;
}

uint8_t usbd_uac_data_in(USBD_HandleTypeDef *pdev, uint8_t epnum)
{
    UsbdUacHandleTypeDef *h = &usbd_uac_handle;
    if (!h->initialized || !h->as_in_active)
    {
        return USBD_OK;
    }
    if ((epnum & 0x7FU) != (UAC_AS_IN_EP & 0x7FU))
    {
        return USBD_OK;
    }
    h->diag_as_in_datain++;
    /* Host just accepted the previous AS-IN packet: arm the next one now.
     * Data-in-driven scheduling (rather than SOF-driven) guarantees we never
     * overwrite an unsent PMA buffer, which is what caused macOS's usbaudiod
     * to log completeCount=0 overruns and cycle the AS-IN interface. */
    usbd_uac_transmit_as_in_packet(pdev);
    return USBD_OK;
}

/* STM32F303RE SRAM is 64 KB at 0x20000000; CCMRAM is 16 KB at 0x10000000.
 * Any struct pointer we deref must land in one of those. This is a defensive
 * check because BTABLE corruption (see docs / decode-fault trail) has been
 * observed scribbling garbage into h->stream, and a subsequent deref bus-faults. */
static inline bool stream_ptr_is_sane(const void *p)
{
    uintptr_t v = (uintptr_t)p;
    if (v >= 0x20000000U && v < 0x20010000U)
    {
        return true;
    }
    if (v >= 0x10000000U && v < 0x10004000U)
    {
        return true;
    }
    return false;
}

uint8_t usbd_uac_data_out(USBD_HandleTypeDef *pdev, uint8_t epnum)
{
    UsbdUacHandleTypeDef *h = &usbd_uac_handle;
    h->diag_isr_total++;

    if (!h->initialized || !h->as_out_active)
    {
        return USBD_FAIL;
    }

    uint32_t received = USBD_LL_GetRxDataSize(pdev, epnum);
    h->diag_last_received = received;
    /* Defensive clamp: the peripheral reports COUNT_RX from BTABLE (a 10-bit
     * field, so up to 1023). If BTABLE has been corrupted, this can be a bogus
     * value larger than out_rx_buf. Bad receives are dropped rather than
     * trusted, so downstream code never reads past the buffer. */
    if (received > (uint32_t)UAC_AS_MAX_PACKET_BYTES)
    {
        h->diag_size_oversz++;
        received = 0U;
    }
    if (received == 0U)
    {
        h->diag_size_zero++;
    }

    struct UsbAudioStream *s = effective_stream(h);
    if (!stream_ptr_is_sane(s))
    {
        h->diag_stream_bad++;
    }
    else if (received > 0U)
    {
        h->diag_pushed++;
        usb_audio_stream_note_as_out_packet(s);
        usb_audio_stream_push_bytes(s, h->out_rx_buf, received);
    }

    USBD_LL_PrepareReceive(pdev, UAC_AS_OUT_EP, h->out_rx_buf, UAC_AS_MAX_PACKET_BYTES);
    return USBD_OK;
}

static void usbd_uac_transmit_as_in_packet(USBD_HandleTypeDef *pdev)
{
    UsbdUacHandleTypeDef *h = &usbd_uac_handle;

    /* UAC_SAMPLE_RATE_HZ isn't a whole multiple of 1000; Bresenham-style
     * accumulator sends the extra sample often enough to track the average
     * rate exactly. */
    uint32_t samples_this_ms = UAC_SAMPLE_RATE_HZ / 1000U;
    h->as_in_frac_accum += UAC_SAMPLE_RATE_HZ % 1000U;
    if (h->as_in_frac_accum >= 1000U)
    {
        h->as_in_frac_accum -= 1000U;
        samples_this_ms++;
        h->diag_as_in_tx_long++;
    }

    size_t popped = 0U;
    struct UsbAudioStream *s = effective_stream(h);
    if (stream_ptr_is_sane(s))
    {
        popped = usb_audio_stream_pop_samples(s, (int16_t *)(void *)h->in_tx_buf,
                                              samples_this_ms);
    }

    /* Zero-fill the tail so we always send a full-length isoc packet. macOS's
     * capture stream retries and gives up (~3 retries) if the device sends
     * zero-length isoc IN packets, and when it gives up on AS-IN it also tears
     * down the AS-OUT stream. Silence bytes are a valid packet; empty is not. */
    if (popped < samples_this_ms)
    {
        h->diag_as_in_underrun++;
        size_t offset = popped * 2U;
        size_t tail_bytes = (samples_this_ms - popped) * 2U;
        (void)memset(h->in_tx_buf + offset, 0, tail_bytes);
    }

    h->diag_as_in_tx++;
    if (USBD_LL_Transmit(pdev, UAC_AS_IN_EP, h->in_tx_buf,
                         (uint16_t)(samples_this_ms * 2U)) != USBD_OK)
    {
        h->diag_as_in_tx_err++;
    }
}

static void usbd_uac_sof_feedback(USBD_HandleTypeDef *pdev)
{
    UsbdUacHandleTypeDef *h = &usbd_uac_handle;

    /* Update the feedback value every UAC_FEEDBACK_PERIOD_MS SOFs. Between
     * updates, feedback_buf keeps the previous value so IN tokens still get
     * meaningful data. */
    if (h->sof_count % UAC_FEEDBACK_PERIOD_MS == 0U)
    {
        uint32_t nominal_samples = (UAC_SAMPLE_RATE_HZ * UAC_FEEDBACK_PERIOD_MS) / 1000U;
        int32_t correction = 0;

        struct UsbAudioStream *s = effective_stream(h);
        if (stream_ptr_is_sane(s))
        {
            int32_t fill = (int32_t)usb_audio_stream_in_available(s);
            /* Report a LOWER rate when the ring is above target (host is sending
             * faster than we consume) and HIGHER when below. Gentle P gain. */
            int32_t deficit = (int32_t)UAC_IN_RING_TARGET_FILL - fill;
            correction = deficit / 16;
        }

        int32_t samples_consumed = (int32_t)nominal_samples + correction;
        if (samples_consumed < 0)
        {
            samples_consumed = 0;
        }

        uint32_t feedback_value = usbd_uac_compute_feedback_value(
            (uint32_t)samples_consumed, UAC_FEEDBACK_PERIOD_MS);

        h->feedback_buf[0] = (uint8_t)(feedback_value & 0xFFU);
        h->feedback_buf[1] = (uint8_t)((feedback_value >> 8) & 0xFFU);
        h->feedback_buf[2] = (uint8_t)((feedback_value >> 16) & 0xFFU);
    }

    /* Always arm the feedback endpoint every SOF. macOS polls every 1 ms and
     * reports AUALockDelay overruns if the peripheral doesn't respond on every
     * IN token, even though the value only changes every bRefresh interval. */
    USBD_LL_Transmit(pdev, UAC_FEEDBACK_EP, h->feedback_buf,
                     UAC_FEEDBACK_PACKET_BYTES);
}

uint8_t usbd_uac_sof(USBD_HandleTypeDef *pdev)
{
    UsbdUacHandleTypeDef *h = &usbd_uac_handle;
    if (!h->initialized)
    {
        return USBD_OK;
    }

    /* AS-IN is scheduled entirely from usbd_uac_data_in. Feedback endpoint
     * removed: AS-OUT declared synchronous. sof_feedback retained for
     * reference but not invoked. */
    (void)usbd_uac_sof_feedback;

    h->sof_count++;
    if (h->as_in_active)
    {
        h->diag_as_in_sof_active++;
    }
    return USBD_OK;
}

bool usbd_uac_owns_ep(uint8_t epnum)
{
    uint8_t addr = epnum & 0x7FU;
    return addr == (UAC_AS_IN_EP & 0x7FU) || addr == (UAC_AS_OUT_EP & 0x7FU) ||
           addr == (UAC_FEEDBACK_EP & 0x7FU);
}

bool usbd_uac_owns_iface(uint8_t iface)
{
    return iface == UAC_IFACE_AC || iface == UAC_IFACE_AS_OUT || iface == UAC_IFACE_AS_IN;
}

void usbd_uac_get_diag(UsbdUacDiag *out)
{
    if (out == NULL)
    {
        return;
    }
    const UsbdUacHandleTypeDef *h = &usbd_uac_handle;
    out->data_out_isr_total = h->diag_isr_total;
    out->data_out_pushed = h->diag_pushed;
    out->data_out_stream_bad = h->diag_stream_bad;
    out->data_out_size_zero = h->diag_size_zero;
    out->data_out_size_oversz = h->diag_size_oversz;
    out->sof_count = h->sof_count;
    out->last_received = h->diag_last_received;
    out->stream_repair_count = diag_stream_repair_count;
    out->as_in_tx = h->diag_as_in_tx;
    out->as_in_tx_err = h->diag_as_in_tx_err;
    out->as_in_datain = h->diag_as_in_datain;
    out->as_in_sof_active = h->diag_as_in_sof_active;
    out->as_in_tx_long = h->diag_as_in_tx_long;
    out->as_in_underrun = h->diag_as_in_underrun;
    out->setalt_out_1 = h->diag_setalt_out_1;
    out->setalt_out_0 = h->diag_setalt_out_0;
    out->setalt_in_1 = h->diag_setalt_in_1;
    out->setalt_in_0 = h->diag_setalt_in_0;
    out->as_out_active = h->as_out_active;
    out->as_in_active = h->as_in_active;
}

void usbd_uac_reset_diag(void)
{
    UsbdUacHandleTypeDef *h = &usbd_uac_handle;
    h->diag_isr_total = 0U;
    h->diag_pushed = 0U;
    h->diag_stream_bad = 0U;
    h->diag_size_zero = 0U;
    h->diag_size_oversz = 0U;
    h->diag_last_received = 0U;
    h->diag_as_in_tx = 0U;
    h->diag_as_in_tx_err = 0U;
    h->diag_as_in_datain = 0U;
    h->diag_as_in_sof_active = 0U;
    h->diag_as_in_tx_long = 0U;
    h->diag_as_in_underrun = 0U;
    h->diag_setalt_out_0 = 0U;
    h->diag_setalt_out_1 = 0U;
    h->diag_setalt_in_0 = 0U;
    h->diag_setalt_in_1 = 0U;
    diag_stream_repair_count = 0U;
}
// NOLINTEND(readability-identifier-naming)
