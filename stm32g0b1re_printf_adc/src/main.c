#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>

#include <zephyr/devicetree.h>
#include <zephyr/drivers/adc.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/util.h>

#include <stm32g0xx_ll_adc.h>

#define APP_NODE DT_PATH(zephyr_user)
#define ADC_VREFINT_NODE DT_IO_CHANNELS_CTLR_BY_IDX(APP_NODE, 1)
#define ADC_VREFINT_BASE ((ADC_TypeDef *)DT_REG_ADDR(ADC_VREFINT_NODE))

#if !DT_NODE_EXISTS(APP_NODE) || !DT_NODE_HAS_PROP(APP_NODE, io_channels)
#error "zephyr,user io-channels must be defined in the board overlay"
#endif

static const struct adc_dt_spec adc_pa0 = ADC_DT_SPEC_GET_BY_IDX(APP_NODE, 0);
static const struct adc_dt_spec adc_vrefint = ADC_DT_SPEC_GET_BY_IDX(APP_NODE, 1);

static void enable_vrefint_channel(void)
{
	uint32_t path = LL_ADC_GetCommonPathInternalCh(__LL_ADC_COMMON_INSTANCE(ADC_VREFINT_BASE));

	LL_ADC_SetCommonPathInternalCh(__LL_ADC_COMMON_INSTANCE(ADC_VREFINT_BASE),
				       path | LL_ADC_PATH_INTERNAL_VREFINT);
#ifdef LL_ADC_DELAY_VREFINT_STAB_US
	k_usleep(LL_ADC_DELAY_VREFINT_STAB_US);
#endif
}

static void disable_vrefint_channel(void)
{
	uint32_t path = LL_ADC_GetCommonPathInternalCh(__LL_ADC_COMMON_INSTANCE(ADC_VREFINT_BASE));

	LL_ADC_SetCommonPathInternalCh(__LL_ADC_COMMON_INSTANCE(ADC_VREFINT_BASE),
				       path & ~LL_ADC_PATH_INTERNAL_VREFINT);
}

static int read_adc_raw(const struct adc_dt_spec *spec, int16_t *raw)
{
	struct adc_sequence sequence = {
		.buffer = raw,
		.buffer_size = sizeof(*raw),
	};

	*raw = 0;
	adc_sequence_init_dt(spec, &sequence);
	return adc_read_dt(spec, &sequence);
}

static uint32_t adc_full_scale(const struct adc_dt_spec *spec)
{
	return BIT(spec->resolution) - 1U;
}

static uint32_t calc_vdda_mv(uint16_t vrefint_raw)
{
	if (vrefint_raw == 0U) {
		return 0U;
	}

	/*
	 * ST formula:
	 * VDDA(mV) = VREFINT_CAL_VREF(mV) * VREFINT_CAL / VREFINT_RAW
	 */
	return (((uint32_t)(*VREFINT_CAL_ADDR) * VREFINT_CAL_VREF) + (vrefint_raw / 2U))
	       / vrefint_raw;
}

static uint32_t calc_input_mv(uint16_t input_raw, uint32_t vdda_mv, uint32_t full_scale)
{
	if (full_scale == 0U) {
		return 0U;
	}

	/*
	 * Input(mV) = RAW * VDDA(mV) / ((2 ^ resolution) - 1)
	 */
	return (((uint32_t)input_raw * vdda_mv) + (full_scale / 2U)) / full_scale;
}

int main(void)
{
	int err;
	int16_t pa0_raw;
	int16_t vrefint_raw;

	printf("STM32G0 ADC PA0 monitor\r\n");
	printf("Board: %s\r\n", CONFIG_BOARD_TARGET);
	printf("Formula: VDDA = 3000mV * VREFINT_CAL / VREFINT_RAW\r\n");
	printf("Formula: PA0  = PA0_RAW * VDDA / 4095\r\n");

	if (!adc_is_ready_dt(&adc_pa0) || !adc_is_ready_dt(&adc_vrefint)) {
		printf("ADC device is not ready\r\n");
		return 0;
	}

	err = adc_channel_setup_dt(&adc_pa0);
	if (err < 0) {
		printf("Failed to setup PA0 ADC channel: %d\r\n", err);
		return 0;
	}

	err = adc_channel_setup_dt(&adc_vrefint);
	if (err < 0) {
		printf("Failed to setup VREFINT ADC channel: %d\r\n", err);
		return 0;
	}

	while (1) {
		uint32_t vdda_mv;
		uint32_t pa0_mv;

		enable_vrefint_channel();
		err = read_adc_raw(&adc_vrefint, &vrefint_raw);
		disable_vrefint_channel();
		if (err < 0) {
			printf("Failed to read VREFINT: %d\r\n", err);
			k_sleep(K_SECONDS(1));
			continue;
		}

		err = read_adc_raw(&adc_pa0, &pa0_raw);
		if (err < 0) {
			printf("Failed to read PA0: %d\r\n", err);
			k_sleep(K_SECONDS(1));
			continue;
		}

		vdda_mv = calc_vdda_mv((uint16_t)vrefint_raw);
		pa0_mv = calc_input_mv((uint16_t)pa0_raw, vdda_mv, adc_full_scale(&adc_pa0));

		printf("PA0 raw=%" PRIu16 ", VREFINT raw=%" PRIu16
		       ", VDDA=%" PRIu32 " mV, PA0=%" PRIu32 ".%03" PRIu32 " V\r\n",
		       (uint16_t)pa0_raw, (uint16_t)vrefint_raw,
		       vdda_mv, pa0_mv / 1000U, pa0_mv % 1000U);

		k_sleep(K_SECONDS(1));
	}

	return 0;
}
