#include "effects_control_logic.h"

void effects_state_apply_switches(EffectsState *state, bool switchAEnabled, bool switchBEnabled,
    bool switchCEnabled)
{
    if (state == NULL)
    {
        return;
    }

    state->isEnabled[state->ordered[0]] = switchAEnabled;
    state->isEnabled[state->ordered[1]] = switchBEnabled;
    state->isEnabled[state->ordered[2]] = switchCEnabled;
}

bool effects_params_apply_pot_sample(EffectsParams *params, Effect activeEffect, uint8_t potIndex,
    uint32_t adcValue, uint32_t adcMax)
{
    if (params == NULL)
    {
        return false;
    }

    switch (activeEffect)
    {
        case OVERDRIVE:
            switch (potIndex)
            {
                case 0:
                    params->overdrive.gain =
                        map_adc_to_param(adcValue, params->overdriveMin.gain, params->overdriveMax.gain, adcMax);
                    return true;
                case 1:
                    params->overdrive.level = map_adc_to_param(
                        adcValue, params->overdriveMin.level, params->overdriveMax.level, adcMax);
                    return true;
                case 2:
                    params->overdrive.tone =
                        map_adc_to_param(adcValue, params->overdriveMin.tone, params->overdriveMax.tone, adcMax);
                    return true;
                case 3:
                    params->overdrive.mix =
                        map_adc_to_param(adcValue, params->overdriveMin.mix, params->overdriveMax.mix, adcMax);
                    return true;
                default:
                    return false;
            }

        case ECHO:
            switch (potIndex)
            {
                case 0:
                    params->echo.pre_delay = map_adc_to_param(
                        adcValue, params->echoMin.pre_delay, params->echoMax.pre_delay, adcMax);
                    return true;
                case 1:
                    params->echo.density =
                        map_adc_to_param(adcValue, params->echoMin.density, params->echoMax.density, adcMax);
                    return true;
                case 2:
                    params->echo.attack =
                        map_adc_to_param(adcValue, params->echoMin.attack, params->echoMax.attack, adcMax);
                    return true;
                case 3:
                    params->echo.decay =
                        map_adc_to_param(adcValue, params->echoMin.decay, params->echoMax.decay, adcMax);
                    return true;
                default:
                    return false;
            }

        case COMPRESSION:
            switch (potIndex)
            {
                case 0:
                    params->compression.threshold = map_adc_to_param(
                        adcValue, params->compressionMin.threshold, params->compressionMax.threshold, adcMax);
                    return true;
                case 1:
                    params->compression.ratio = map_adc_to_param(
                        adcValue, params->compressionMin.ratio, params->compressionMax.ratio, adcMax);
                    return true;
                case 2:
                case 3:
                    return true;
                default:
                    return false;
            }

        default:
            return false;
    }
}
