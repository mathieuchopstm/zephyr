/*
 *
 */

#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/kernel.h>
#include <zephyr/platform/hooks.h>
#include <zephyr/sys/clock.h>
#include <zephyr/sys/util.h>

#include "codec.h"

#define CODEC_I2C_NAME		i2c4
#define CODEC_I2C_ADDRESS	0x4a /* 7-bit format for 0x94 */
#define CODEC_RESET_GPIO	gpioi
#define CODEC_RESET_PIN		11

const struct device *codec_i2c = DEVICE_DT_GET(DT_NODELABEL(CODEC_I2C_NAME));

static int hard_reset_codec(void)
{
	const struct device *gpio = DEVICE_DT_GET(DT_NODELABEL(CODEC_RESET_GPIO));
	const gpio_pin_t pin = CODEC_RESET_PIN;
	int res;

	/*
	 * Generate a low pulse on nRESET pin to reset the codec:
	 *  - configure nRESET pin as output + set low level
	 *  - wait until minimum low pulse width has elapsed
	 *  - set high level
	 */
	res = gpio_pin_configure(gpio, pin, GPIO_OUTPUT | GPIO_OUTPUT_INIT_LOW);
	if (res < 0) {
		printk("nRESET pin configuration failed: %d\n", res);
		return res;
	}

	/* Minimum nRESET low pulse width is 1 ms per CS42L51 datasheet */
	k_busy_wait(USEC_PER_MSEC);

	return gpio_pin_set_raw(gpio, pin, 1);
}

/* codec I/O functions */
static void codec_read_reg(uint8_t reg, uint8_t *val)
{
	__maybe_unused int res;

	res = i2c_burst_read(codec_i2c, CODEC_I2C_ADDRESS, reg, val, sizeof(*val));
	__ASSERT(res == 0, "i2c_read(reg=0x%02hhX) failed %d", reg, res);
}

static void codec_write_reg(uint8_t reg, uint8_t val)
{
	__maybe_unused int res;

	res = i2c_burst_write(codec_i2c, CODEC_I2C_ADDRESS, reg, &val, sizeof(val));
	__ASSERT(res == 0, "i2c_write(reg=0x%02hhX, val=0x%02hhX) failed %d", reg, val, res);
}

/* driver functions */
static void codec_start_playback(void) /* = CS42L51_Play() */
{
	uint8_t tmp;
	/* Unmute the output first: CS42L51_SetMute(MUTE_OFF) */
		codec_read_reg(DAC_OUTPUT_CTRL, &tmp);
		/* CS42L51_MUTE_OFF Disable the Mute */
		tmp &= 0xFCU;
		codec_write_reg(DAC_OUTPUT_CTRL, tmp);

	/* DAC control : Signal processing to DAC, Freeze off, De-emphasis off, Analog output auto mute off,
	DAC soft ramp */
	codec_write_reg(DAC_CTRL, 0x42U);

	/* Power control 1 : PDN_DACA, PDN_DACB disable. */
	codec_read_reg(PWR_CTRL_1, &tmp);
	tmp &= 0x9FU;
	codec_write_reg(PWR_CTRL_1, tmp);

	/* Power control : Exit standby (PDN = 0) */
	codec_read_reg(PWR_CTRL_1, &tmp);
	tmp &= 0xFEU;
	codec_write_reg(PWR_CTRL_1, tmp);
}

/* board init hook */

void board_late_init_hook(void)
{
	int res;
	uint8_t chip_id;

	if (!device_is_ready(codec_i2c)) {
		printk(STRINGIFY(CODEC_I2C_NAME) " not ready\n");
		return;
	}

	res = hard_reset_codec();
	if (res < 0) {
		printk("Failed to reset CS42L51 codec: %d\n", res);
		return;
	}

	codec_read_reg(CHIP_ID, &chip_id);

	if ((chip_id & CS42L51_ID_MASK) != CS42L51_ID) {
		printk("Invalid codec ID: 0x%02hhx\n", chip_id);
		return;
	} else {
		printk("Codec ID: 0x%02hhX (rev. %c)\n",
			chip_id, 'A' + (chip_id & ~CS42L51_ID_MASK));
	}

/* CS42L51_Init(Frequency=48000 - unused, Volume=95) */
#define VOLUME 95
	uint8_t tmp;

	codec_read_reg(PWR_CTRL_1, &tmp);
	tmp |= 0x01; // PDN: power down the codec
	codec_write_reg(PWR_CTRL_1, tmp);

	// Set all power-down bits
	codec_write_reg(PWR_CTRL_1, 0x7Fu);

	codec_read_reg(MIC_PWR_SPEED_CTRL, &tmp);
	tmp |= 0x0Eu;
	codec_write_reg(MIC_PWR_SPEED_CTRL, tmp);

	/* Mic Power and Speed Control : Auto detect on, Speed mode SSM, tri state off, MCLK divide by 2 off */
	codec_read_reg(MIC_PWR_SPEED_CTRL, &tmp);
	tmp = ((tmp & 0x0EU) | 0xA0U);
	codec_write_reg(MIC_PWR_SPEED_CTRL, tmp);

	/* Interface control : Loopback off, Slave, I2S (SDIN and SOUT), Digital mix off, Mic mix off */
	codec_write_reg(INTERFACE_CTRL, 0x0CU);

	/* Mic control : ADC single volume off, ADCB boost off, ADCA boost off, MicBias on AIN3B/MICIN2 pin,
	MicBias level 0.8xVA, MICB boost 32db, MICA boost 32dB */
	codec_write_reg(MIC_CTRL, 0x03U);

	/* ADC control : ADCB HPF on, ADCB HPF freeze off, ADCA HPF on, ADCA HPF freeze off, Soft ramp B on,
	Zero cross B on, Soft ramp A on, Zero cross A on */
	codec_write_reg(ADC_CTRL, 0xAFU);

	/* DAC output control : HP Gain to 1, Single volume control off, PCM invert signals polarity off,
	DAC channels mute on */
	codec_write_reg(DAC_OUTPUT_CTRL, 0xC3U);

	/* DAC control : Signal processing to DAC, Freeze off, De-emphasis off, Analog output auto mute off, DAC soft ramp */
	codec_write_reg(DAC_CTRL, 0x42U);

	/* ALCA/ALCB and PGAA/PGAB Control : ALCx soft ramp disable on, ALCx zero cross disable on, PGA x Gain +8dB */
	codec_write_reg(ALCA_PGAA_CTRL, 0xD0U);
	codec_write_reg(ALCB_PGAB_CTRL, 0xD0U);

	/* ADCA/ADCB Attenuator : 0dB */
	codec_write_reg(ADCA_ATTENUATOR, 0x00U);
	codec_write_reg(ADCB_ATTENUATOR, 0x00U);

	/* ADCA/ADCB mixer volume control : ADCx mixer channel mute on, ADCx mixer volume 0dB */
	codec_write_reg(ADCA_MIXER_VOL_CTRL, 0x80U);
	codec_write_reg(ADCB_MIXER_VOL_CTRL, 0x80U);

	/* PCMA/PCMB mixer volume control : PCMx mixer channel mute off, PCMx mixer volume 0dB */
	codec_write_reg(PCMA_MIXER_VOL_CTRL, 0x00U);
	codec_write_reg(PCMB_MIXER_VOL_CTRL, 0x00U);

	/* PCM channel mixer : AOUTA Left, AOUTB Right */
	codec_write_reg(ADC_PCM_CHANNEL_MIXER, 0x00U);

	tmp = VOLUME_CONVERT(VOLUME);
	/* AOUTA/AOUTB volume control */
	codec_write_reg(AOUTA_VOL_CTRL, tmp);
	codec_write_reg(AOUTB_VOL_CTRL, tmp);

	/* ALC enable and attack rate : ALCB and ALCA enable, fastest attack */
	codec_write_reg(ALC_ENABLE_AND_ATTACK_RATE, 0x40U);
/* end of CS42L51_Init() */


	codec_start_playback();
}
