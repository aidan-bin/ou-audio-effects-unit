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

/* Matches bRefresh (2^5 = 32ms) in the descriptor, composite.c. */
#define UAC_FEEDBACK_PERIOD_MS 32U

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

    uint8_t freq_scratch[3];

    struct UsbAudioStream *stream;
} UsbdUacHandleTypeDef;

static UsbdUacHandleTypeDef usbd_uac_handle;

void usbd_uac_register_stream(struct UsbAudioStream *stream)
{
    usbd_uac_handle.stream = stream;
}

uint8_t usbd_uac_class_init(USBD_HandleTypeDef *pdev)
{
    (void)pdev;
    UsbdUacHandleTypeDef *h = &usbd_uac_handle;
    struct UsbAudioStream *stream = h->stream;

    // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
    memset(h, 0, sizeof(*h));
    h->stream = stream;
    h->initialized = true;

    usb_audio_stream_reset(stream);

    return 0U;
}

uint8_t usbd_uac_class_deinit(USBD_HandleTypeDef *pdev)
{
    UsbdUacHandleTypeDef *h = &usbd_uac_handle;

    if (h->as_out_active)
    {
        USBD_LL_CloseEP(pdev, UAC_AS_OUT_EP);
        USBD_LL_CloseEP(pdev, UAC_FEEDBACK_EP);
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

static void usbd_uac_set_as_out_alt(USBD_HandleTypeDef *pdev, uint8_t alt)
{
    UsbdUacHandleTypeDef *h = &usbd_uac_handle;

    if (alt != 0U)
    {
        if (!h->as_out_active)
        {
            USBD_LL_OpenEP(pdev, UAC_AS_OUT_EP, USBD_EP_TYPE_ISOC, UAC_AS_MAX_PACKET_BYTES);
            USBD_LL_OpenEP(pdev, UAC_FEEDBACK_EP, USBD_EP_TYPE_ISOC, UAC_FEEDBACK_PACKET_BYTES);
            pdev->ep_out[UAC_AS_OUT_EP & 0x7FU].is_used = 1U;
            pdev->ep_in[UAC_FEEDBACK_EP & 0x7FU].is_used = 1U;
            h->as_out_active = true;
            h->sof_count = 0U;
            USBD_LL_PrepareReceive(pdev, UAC_AS_OUT_EP, h->out_rx_buf, UAC_AS_MAX_PACKET_BYTES);
        }
    }
    else if (h->as_out_active)
    {
        USBD_LL_CloseEP(pdev, UAC_AS_OUT_EP);
        USBD_LL_CloseEP(pdev, UAC_FEEDBACK_EP);
        pdev->ep_out[UAC_AS_OUT_EP & 0x7FU].is_used = 0U;
        pdev->ep_in[UAC_FEEDBACK_EP & 0x7FU].is_used = 0U;
        h->as_out_active = false;
    }
}

static void usbd_uac_set_as_in_alt(USBD_HandleTypeDef *pdev, uint8_t alt)
{
    UsbdUacHandleTypeDef *h = &usbd_uac_handle;

    if (alt != 0U)
    {
        if (!h->as_in_active)
        {
            USBD_LL_OpenEP(pdev, UAC_AS_IN_EP, USBD_EP_TYPE_ISOC, UAC_AS_MAX_PACKET_BYTES);
            pdev->ep_in[UAC_AS_IN_EP & 0x7FU].is_used = 1U;
            h->as_in_active = true;
            h->as_in_frac_accum = 0U;
        }
    }
    else if (h->as_in_active)
    {
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
    (void)pdev;
    (void)epnum;
    /* Isoc IN packets are staged once per SOF in usbd_uac_sof(). */
    return USBD_OK;
}

uint8_t usbd_uac_data_out(USBD_HandleTypeDef *pdev, uint8_t epnum)
{
    UsbdUacHandleTypeDef *h = &usbd_uac_handle;
    if (!h->initialized || !h->as_out_active)
    {
        return USBD_FAIL;
    }

    uint32_t received = USBD_LL_GetRxDataSize(pdev, epnum);
    if (h->stream != NULL && received > 0U)
    {
        usb_audio_stream_push_bytes(h->stream, h->out_rx_buf, received);
    }

    USBD_LL_PrepareReceive(pdev, UAC_AS_OUT_EP, h->out_rx_buf, UAC_AS_MAX_PACKET_BYTES);
    return USBD_OK;
}

static void usbd_uac_sof_as_in(USBD_HandleTypeDef *pdev)
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
    }

    size_t popped = 0U;
    if (h->stream != NULL)
    {
        popped = usb_audio_stream_pop_samples(h->stream, (int16_t *)(void *)h->in_tx_buf,
                                              samples_this_ms);
    }

    USBD_LL_Transmit(pdev, UAC_AS_IN_EP, h->in_tx_buf, (uint16_t)(popped * 2U));
}

static void usbd_uac_sof_feedback(USBD_HandleTypeDef *pdev)
{
    UsbdUacHandleTypeDef *h = &usbd_uac_handle;

    if (h->sof_count % UAC_FEEDBACK_PERIOD_MS != 0U)
    {
        return;
    }

    uint32_t nominal_samples = (UAC_SAMPLE_RATE_HZ * UAC_FEEDBACK_PERIOD_MS) / 1000U;
    int32_t correction = 0;

    if (h->stream != NULL)
    {
        int32_t fill = (int32_t)usb_audio_stream_in_available(h->stream);
        int32_t error = fill - (int32_t)UAC_IN_RING_TARGET_FILL;
        /* Proportional control: gentle correction, no overreaction to jitter. */
        correction = error / 16;
    }

    int32_t samples_consumed = (int32_t)nominal_samples + correction;
    if (samples_consumed < 0)
    {
        samples_consumed = 0;
    }

    uint32_t feedback_value =
        usbd_uac_compute_feedback_value((uint32_t)samples_consumed, UAC_FEEDBACK_PERIOD_MS);

    h->feedback_buf[0] = (uint8_t)(feedback_value & 0xFFU);
    h->feedback_buf[1] = (uint8_t)((feedback_value >> 8) & 0xFFU);
    h->feedback_buf[2] = (uint8_t)((feedback_value >> 16) & 0xFFU);

    USBD_LL_Transmit(pdev, UAC_FEEDBACK_EP, h->feedback_buf, UAC_FEEDBACK_PACKET_BYTES);
}

uint8_t usbd_uac_sof(USBD_HandleTypeDef *pdev)
{
    UsbdUacHandleTypeDef *h = &usbd_uac_handle;
    if (!h->initialized)
    {
        return USBD_OK;
    }

    if (h->as_in_active)
    {
        usbd_uac_sof_as_in(pdev);
    }
    if (h->as_out_active)
    {
        usbd_uac_sof_feedback(pdev);
    }

    h->sof_count++;
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
// NOLINTEND(readability-identifier-naming)
