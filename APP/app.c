#include "app.h"

#include <stdio.h>
#include <string.h>

#include "freqmeter.h"
#include "key.h"
#include "my_ADC.h"
#include "ohmmeter.h"
#include "oled.h"
#include "voltmeter.h"


typedef struct {
	uint8_t active;
	uint32_t start_tick;
	uint32_t duration_ms;
	char from_text[20];
	char to_text[20];
} AppTransition;

/* DC auto range hysteresis thresholds (V). */
#define DC_AUTO_TO_20V_THRESHOLD_V      1.95
#define DC_AUTO_TO_2000MV_THRESHOLD_V   1.70

/* AC auto range hysteresis thresholds (Vrms). */
#define AC_AUTO_TO_20V_THRESHOLD_V      1.95
#define AC_AUTO_TO_2000MV_THRESHOLD_V   1.70

AppMode g_mode = APP_MODE_DCV;
static uint8_t g_range_idx[APP_MODE_COUNT] = {0U, 0U, 2U, 0U, 0U};
static uint8_t g_auto_enable[APP_MODE_COUNT] = {0U, 0U, 0U, 0U, 0U};
static AppTransition g_transition = {0U, 0U, 280U, {0}, {0}};

static uint8_t App_GetRangeCount(AppMode mode)
{
	if (mode == APP_MODE_DCV) {
		return 2U;
	}

	if (mode == APP_MODE_ACV) {
		return 2U;
	}

	if (mode == APP_MODE_OHM) {
		return 4U;
	}

	if (mode == APP_MODE_DIODE) {
		return 1U;
	}

	return 5U;
}

static void App_GetStatusText(char *buf, size_t len)
{
	if (g_mode == APP_MODE_DCV) {
		if (g_auto_enable[APP_MODE_DCV] != 0U) {
			snprintf(buf, len, "DCV AUTO");
			return;
		}

		if (g_range_idx[APP_MODE_DCV] == 0U) {
			snprintf(buf, len, "DCV 20V");
		} else {
			snprintf(buf, len, "DCV 2000mV");
		}
		return;
	}

	if (g_mode == APP_MODE_ACV) {
		if (g_auto_enable[APP_MODE_ACV] != 0U) {
			snprintf(buf, len, "ACV AUTO");
			return;
		}

		if (g_range_idx[APP_MODE_ACV] == 0U) {
			snprintf(buf, len, "ACV 20V");
		} else {
			snprintf(buf, len, "ACV 2000mV");
		}
		return;
	}

	if (g_mode == APP_MODE_OHM) {
		if (g_auto_enable[APP_MODE_OHM] != 0U) {
			snprintf(buf, len, "OHM AUTO");
			return;
		}

		if (g_range_idx[APP_MODE_OHM] == 0U) {
			snprintf(buf, len, "OHM 200R");
		} else if (g_range_idx[APP_MODE_OHM] == 1U) {
			snprintf(buf, len, "OHM 2k");
		} else if (g_range_idx[APP_MODE_OHM] == 2U) {
			snprintf(buf, len, "OHM 20k");
		} else {
			snprintf(buf, len, "OHM 200k");
		}
		return;
	}

	if (g_mode == APP_MODE_DIODE) {
		snprintf(buf, len, "DIODE");
		return;
	}

	if (g_auto_enable[APP_MODE_FREQ] != 0U) {
		snprintf(buf, len, "FREQ AUTO");
		return;
	}

	if (g_range_idx[APP_MODE_FREQ] == 0U) {
		snprintf(buf, len, "FREQ 20Hz");
	} else if (g_range_idx[APP_MODE_FREQ] == 1U) {
		snprintf(buf, len, "FREQ 200Hz");
	} else if (g_range_idx[APP_MODE_FREQ] == 2U) {
		snprintf(buf, len, "FREQ 2kHz");
	} else if (g_range_idx[APP_MODE_FREQ] == 3U) {
		snprintf(buf, len, "FREQ 20kHz");
	} else {
		snprintf(buf, len, "FREQ 200kHz");
	}
}

static void App_StartTransition(const char *from, const char *to)
{
	g_transition.active = 1U;
	g_transition.start_tick = HAL_GetTick();
	g_transition.duration_ms = 800U;
	snprintf(g_transition.from_text, sizeof(g_transition.from_text), "%s", from);
	snprintf(g_transition.to_text, sizeof(g_transition.to_text), "%s", to);
}

static void App_InitModuleByMode(AppMode mode)
{
	if ((mode == APP_MODE_DCV) || (mode == APP_MODE_ACV)) {
		VoltMeter_Init();
		return;
	}

	if (mode == APP_MODE_FREQ) {
		FreqMeter_Init();
		return;
	}

	OhmMeter_Init();
}

static void App_ApplyCurrentSetting(void)
{
	if (g_mode == APP_MODE_DCV) {
		if (g_range_idx[APP_MODE_DCV] == 0U) {
			VoltMeter_DC20V_Start();
		} else {
			VoltMeter_DC2000mV_Start();
		}
		return;
	}

	if (g_mode == APP_MODE_ACV) {
		if (g_range_idx[APP_MODE_ACV] == 0U) {
			VoltMeter_AC20V_Start();
		} else {
			VoltMeter_AC2000mV_Start();
		}
		return;
	}

	if (g_mode == APP_MODE_OHM) {
		if (g_auto_enable[APP_MODE_OHM] != 0U) {
			OhmMeter_Auto_Start();
			return;
		}

		if (g_range_idx[APP_MODE_OHM] == 0U) {
			OhmMeter_200_Ohm_Start();
		} else if (g_range_idx[APP_MODE_OHM] == 1U) {
			OhmMeter_2k_Ohm_Start();
		} else if (g_range_idx[APP_MODE_OHM] == 2U) {
			OhmMeter_20k_Ohm_Start();
		} else {
			OhmMeter_200k_Ohm_Start();
		}
		return;
	}

	if (g_mode == APP_MODE_DIODE) {
		OhmMeter_Diode_Start();
		return;
	}

	if (g_auto_enable[APP_MODE_FREQ] != 0U) {
		FreqMeter_Auto_Start();
		return;
	}

	if (g_range_idx[APP_MODE_FREQ] == 0U) {
		FreqMeter_20Hz_Start();
	} else if (g_range_idx[APP_MODE_FREQ] == 1U) {
		FreqMeter_200Hz_Start();
	} else if (g_range_idx[APP_MODE_FREQ] == 2U) {
		FreqMeter_2kHz_Start();
	} else if (g_range_idx[APP_MODE_FREQ] == 3U) {
		FreqMeter_20kHz_Start();
	} else {
		FreqMeter_200kHz_Start();
	}
}

static void App_UpdateDCAutoRange(void)
{
	double voltage_abs;
	uint16_t raw;

	if (g_mode != APP_MODE_DCV) {
		return;
	}

	if (g_auto_enable[APP_MODE_DCV] == 0U) {
		return;
	}

	raw = adc_after_filter;
	if ((raw > 64000U) || (raw < 500U)) {
		if (g_range_idx[APP_MODE_DCV] != 0U) {
			g_range_idx[APP_MODE_DCV] = 0U;
			App_ApplyCurrentSetting();
		}
		return;
	}

	if (g_range_idx[APP_MODE_DCV] == 0U) {
		voltage_abs = DC_20V_Calc(raw);
	} else {
		voltage_abs = DC_2000mV_Calc(raw);
	}

	if (voltage_abs < 0.0) {
		voltage_abs = -voltage_abs;
	}

	if ((g_range_idx[APP_MODE_DCV] == 1U) && (voltage_abs > DC_AUTO_TO_20V_THRESHOLD_V)) {
		g_range_idx[APP_MODE_DCV] = 0U;
		App_ApplyCurrentSetting();
		return;
	}

	if ((g_range_idx[APP_MODE_DCV] == 0U) && (voltage_abs < DC_AUTO_TO_2000MV_THRESHOLD_V)) {
		g_range_idx[APP_MODE_DCV] = 1U;
		App_ApplyCurrentSetting();
	}
}

static void App_UpdateACAutoRange(void)
{
	double voltage_abs;

	if (g_mode != APP_MODE_ACV) {
		return;
	}

	if (g_auto_enable[APP_MODE_ACV] == 0U) {
		return;
	}

	if (g_range_idx[APP_MODE_ACV] == 0U) {
		voltage_abs = VoltMeter_AC20V_CalcValue();
	} else {
		voltage_abs = VoltMeter_AC2000mV_CalcValue() / 1000.0;
	}

	if (voltage_abs < 0.0) {
		voltage_abs = -voltage_abs;
	}

	if ((g_range_idx[APP_MODE_ACV] == 1U) && (voltage_abs > AC_AUTO_TO_20V_THRESHOLD_V)) {
		g_range_idx[APP_MODE_ACV] = 0U;
		App_ApplyCurrentSetting();
		return;
	}

	if ((g_range_idx[APP_MODE_ACV] == 0U) && (voltage_abs < AC_AUTO_TO_2000MV_THRESHOLD_V)) {
		g_range_idx[APP_MODE_ACV] = 1U;
		App_ApplyCurrentSetting();
	}
}

static void App_RenderMeasurement(void)
{
	if (g_mode == APP_MODE_DCV) {
		App_UpdateDCAutoRange();

		if (g_range_idx[APP_MODE_DCV] == 0U) {
			VoltMeter_DC20V_Display();
		} else {
			VoltMeter_DC2000mV_Display();
		}
		return;
	}

	if (g_mode == APP_MODE_ACV) {
		App_UpdateACAutoRange();

		if (g_range_idx[APP_MODE_ACV] == 0U) {
			VoltMeter_AC20V_Display();
		} else {
			VoltMeter_AC2000mV_Display();
		}
		return;
	}

	if (g_mode == APP_MODE_OHM) {
		if (g_auto_enable[APP_MODE_OHM] != 0U) {
			OhmMeter_Auto_Display();
			return;
		}

		if (g_range_idx[APP_MODE_OHM] == 0U) {
			OhmMeter_200_Ohm_Display();
		} else if (g_range_idx[APP_MODE_OHM] == 1U) {
			OhmMeter_2k_Ohm_Display();
		} else if (g_range_idx[APP_MODE_OHM] == 2U) {
			OhmMeter_20k_Ohm_Display();
		} else {
			OhmMeter_200k_Ohm_Display();
		}
		return;
	}

	if (g_mode == APP_MODE_DIODE) {
		OhmMeter_Diode_Display();
		return;
	}

	if (g_auto_enable[APP_MODE_FREQ] != 0U) {
		FreqMeter_Auto_Display();
		return;
	}

	if (g_range_idx[APP_MODE_FREQ] == 0U) {
		FreqMeter_20Hz_Display();
	} else if (g_range_idx[APP_MODE_FREQ] == 1U) {
		FreqMeter_200Hz_Display();
	} else if (g_range_idx[APP_MODE_FREQ] == 2U) {
		FreqMeter_2kHz_Display();
	} else if (g_range_idx[APP_MODE_FREQ] == 3U) {
		FreqMeter_20kHz_Display();
	} else {
		FreqMeter_200kHz_Display();
	}
}

static void App_RenderTransition(void)
{
	uint32_t elapsed = HAL_GetTick() - g_transition.start_tick;
	uint8_t split_x;

	if (elapsed >= g_transition.duration_ms) {
		g_transition.active = 0U;
		return;
	}

	split_x = (uint8_t)((elapsed * 128U) / g_transition.duration_ms);

	OLED_NewFrame();
	OLED_PrintString(0, 0, g_transition.from_text, &font16x16, OLED_COLOR_NORMAL);
	OLED_PrintString(0, 24, "-->", &font16x16, OLED_COLOR_NORMAL);
	OLED_PrintString(0, 48, g_transition.to_text, &font16x16, OLED_COLOR_NORMAL);

	if (split_x < 128U) {
		OLED_DrawFilledRectangle(split_x, 0, (uint8_t)(128U - split_x), 64U, OLED_COLOR_REVERSED);
	}

	OLED_ShowFrame();
}

void APP_Init()
{
    OLED_Init();
	Key_Init();

	g_mode = APP_MODE_DCV;
	g_range_idx[APP_MODE_DCV] = 0U;
	g_range_idx[APP_MODE_ACV] = 0U;
	g_range_idx[APP_MODE_FREQ] = 2U;
	g_range_idx[APP_MODE_OHM] = 0U;
	g_range_idx[APP_MODE_DIODE] = 0U;
	g_auto_enable[APP_MODE_DCV] = 0U;
	g_auto_enable[APP_MODE_ACV] = 0U;
	g_auto_enable[APP_MODE_FREQ] = 0U;
	g_auto_enable[APP_MODE_OHM] = 0U;
	g_auto_enable[APP_MODE_DIODE] = 0U;

	App_InitModuleByMode(g_mode);
	App_ApplyCurrentSetting();
}

void ChangMode()
{
	char from_text[20];
	char to_text[20];

	App_GetStatusText(from_text, sizeof(from_text));

	g_mode = (AppMode)((g_mode + 1U) % APP_MODE_COUNT);
	g_auto_enable[g_mode] = 0U;
	App_InitModuleByMode(g_mode);
	App_ApplyCurrentSetting();

	App_GetStatusText(to_text, sizeof(to_text));
	App_StartTransition(from_text, to_text);
}

void ChangRange()
{
	uint8_t range_count;
	char from_text[20];
	char to_text[20];
	uint8_t old_idx;

	App_GetStatusText(from_text, sizeof(from_text));

	if ((g_mode == APP_MODE_DCV) || (g_mode == APP_MODE_ACV) || (g_mode == APP_MODE_FREQ) || (g_mode == APP_MODE_OHM)) {
		g_auto_enable[g_mode] = 0U;
	}

	range_count = App_GetRangeCount(g_mode);
	old_idx = g_range_idx[g_mode];
	g_range_idx[g_mode] = (uint8_t)((g_range_idx[g_mode] + 1U) % range_count);
	if (g_range_idx[g_mode] == old_idx) {
		return;
	}

	App_ApplyCurrentSetting();

	App_GetStatusText(to_text, sizeof(to_text));
	App_StartTransition(from_text, to_text);
}

static void App_EnableAuto(void)
{
	char from_text[20];
	char to_text[20];

	if ((g_mode != APP_MODE_DCV) && (g_mode != APP_MODE_ACV) && (g_mode != APP_MODE_FREQ) && (g_mode != APP_MODE_OHM)) {
		return;
	}

	App_GetStatusText(from_text, sizeof(from_text));

	g_auto_enable[g_mode] = 1U;
	if (g_mode == APP_MODE_DCV) {
		g_range_idx[APP_MODE_DCV] = 0U;
	} else if (g_mode == APP_MODE_ACV) {
		g_range_idx[APP_MODE_ACV] = 0U;
	} else if (g_mode == APP_MODE_OHM) {
		g_range_idx[APP_MODE_OHM] = 1U;
	}
	App_ApplyCurrentSetting();

	App_GetStatusText(to_text, sizeof(to_text));
	App_StartTransition(from_text, to_text);
}

void DispProc()
{
	OLED_NewFrame();
	App_RenderMeasurement();
	OLED_ShowFrame();
}

void APP_Proc()
{
	KeyEvent event;

	Key_Process();
	event = Key_GetEvent();

	if (event == KEY_EVENT_LONG_2S) {
		ChangMode();
	} else if (event == KEY_EVENT_SHORT) {
		ChangRange();
	} else if (event == KEY_EVENT_DOUBLE) {
		App_EnableAuto();
	}

	if (g_transition.active != 0U) {
		App_RenderTransition();
	} else {
		DispProc();
	}
}


