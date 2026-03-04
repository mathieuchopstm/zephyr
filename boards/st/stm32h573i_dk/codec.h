
/* Codec registers description */
#define CHIP_ID                     0x01U
#define PWR_CTRL_1                  0x02U
#define MIC_PWR_SPEED_CTRL          0x03U
#define INTERFACE_CTRL              0x04U
#define MIC_CTRL                    0x05U
#define ADC_CTRL                    0x06U
#define ADCX_INPUT_SELECT           0x07U
#define DAC_OUTPUT_CTRL             0x08U
#define DAC_CTRL                    0x09U
#define ALCA_PGAA_CTRL              0x0AU
#define ALCB_PGAB_CTRL              0x0BU
#define ADCA_ATTENUATOR             0x0CU
#define ADCB_ATTENUATOR             0x0DU
#define ADCA_MIXER_VOL_CTRL         0x0EU
#define ADCB_MIXER_VOL_CTRL         0x0FU
#define PCMA_MIXER_VOL_CTRL         0x10U
#define PCMB_MIXER_VOL_CTRL         0x11U
#define BEEP_FREQ_AND_TIMING_CFG    0x12U
#define BEEP_OFF_TIME_AND_VOL       0x13U
#define BEEP_CFG_AND_TONE_CFG       0x14U
#define TONE_CTRL                   0x15U
#define AOUTA_VOL_CTRL              0x16U
#define AOUTB_VOL_CTRL              0x17U
#define ADC_PCM_CHANNEL_MIXER       0x18U
#define LIMITER_THR_SZC_DISABLE     0x19U
#define LIMITER_RELEASE_RATE_REG    0x1AU
#define LIMITER_ATTACK_RATE_REG     0x1BU
#define ALC_ENABLE_AND_ATTACK_RATE  0x1CU
#define ALC_RELEASE_RATE            0x1DU
#define ALC_THR                     0x1EU
#define NOISE_GATE_CFG_AND_MISC     0x1FU
#define STATUS                      0x20U
#define CHARGE_PUMP_FREQ            0x21U

#define CS42L51_ID_MASK 0xF8U
#define CS42L51_ID 0xD8U

#define VOLUME_CONVERT(Volume)    (((Volume) >= 100U) ? 0U : ((uint8_t)((((Volume) * 2U) + 56U))))
