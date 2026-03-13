#include "capmeter.h"

#include <stdio.h>

#include "PGA.h"
#include "comp.h"
#include "current.h"
#include "dac.h"
#include "oled.h"
#include "opamp.h"
#include "power_buffer.h"
#include "stm32g4xx_hal_comp.h"
#include "stm32g4xx_hal_dac.h"
#include "stm32g4xx_hal_gpio.h"
#include "stm32g4xx_hal_tim.h"
#include "tim.h"

typedef struct {
    const char *name;
    double upper_f;
    uint32_t charge_timeout_us;
    uint32_t refresh_ms;
    uint32_t timer_tick_hz;
} CapMeter_Range;

typedef struct {
    double capacitance_f;
    uint8_t valid;
    uint8_t over_range;
} CapMeter_Result;

typedef enum {
    CAP_STAGE_IDLE = 0,
    CAP_STAGE_DISCHARGE,
    CAP_STAGE_CHARGE,
    CAP_STAGE_WAIT_IRQ,
    CAP_STAGE_DONE,
} CapMeter_Stage;

static const CapMeter_Range g_cap_ranges[] = {
    {"20nF", 20e-9, 6000U, 120U, 10000000U},
    {"2uF", 2e-6, 220000U, 200U, 1000000U},
    {"200uF", 200e-6, 4500000U, 700U, 100000U},
};

#define CAP_CURRENT_A                  (500e-6)
#define CAP_DV_V                       (5.3)
#define CAP_COMP_V_PRECHARGE           (0.0)
#define CAP_COMP_V_TRIGGER             (2.0)
#define CAP_TIMER_HZ                   (1000000U)
#define CAP_PATH_SETTLE_MS             (2U)
#define CAP_SWITCH_SETTLE_MS           (1U)
#define CAP_COMP_SETTLE_US             (50U)
#define CAP_OVER_RANGE_RATIO           (1.10)
#define CAP_DEFAULT_TIMER_HZ           (1000000U)

static uint8_t g_cap_range_idx = 1U;
static uint8_t g_cap_auto_mode = 0U;
static const char *g_cap_manual_name = "2uF";
static CapMeter_Result g_last_result = {0.0, 0U, 0U};
static uint32_t g_last_measure_tick = 0U;
static CapMeter_Stage g_last_stage = CAP_STAGE_IDLE;
static uint32_t g_last_elapsed_us = 0U;
static uint32_t g_cap_timer_tick_hz = CAP_DEFAULT_TIMER_HZ;
static uint8_t g_meas_active = 0U;
static uint8_t g_meas_range_idx = 1U;
static uint32_t g_meas_discharge_start_ticks = 0U;
static uint32_t g_meas_discharge_ticks = 0U;
static uint32_t g_meas_timeout_ms = 0U;
static uint32_t g_meas_wait_start_ms = 0U;

static volatile uint8_t g_comp7_measure_active = 0U;
static volatile uint32_t g_comp7_latched_us = 0U;
static volatile uint8_t g_comp7_wait_zero = 0U;
static volatile uint8_t g_comp7_zero_seen = 0U;
static volatile uint8_t g_comp7_second_edge_done = 0U;

static uint32_t CapMeter_GetTIM2ClockHz(void)
{
    uint32_t pclk1 = HAL_RCC_GetPCLK1Freq();

    if ((RCC->CFGR & RCC_CFGR_PPRE1) == RCC_CFGR_PPRE1_DIV1) {
        return pclk1;
    }

    return pclk1 * 2U;
}

static void CapMeter_StartTimerBase(uint32_t target_tick_hz)
{
    uint32_t tim2_clk_hz = CapMeter_GetTIM2ClockHz();
    uint32_t prescaler = 0U;

    if (target_tick_hz == 0U) {
        target_tick_hz = CAP_DEFAULT_TIMER_HZ;
    }

    if (target_tick_hz > tim2_clk_hz) {
        target_tick_hz = tim2_clk_hz;
    }

    if (tim2_clk_hz > target_tick_hz) {
        prescaler = (tim2_clk_hz / target_tick_hz) - 1U;
    }

    HAL_TIM_Base_Stop(&htim2);
    __HAL_TIM_SET_PRESCALER(&htim2, prescaler);
    __HAL_TIM_SET_AUTORELOAD(&htim2, 0xFFFFFFFFU);
    __HAL_TIM_SET_COUNTER(&htim2, 0U);
    htim2.Instance->EGR = TIM_EGR_UG;

    g_cap_timer_tick_hz = tim2_clk_hz / (prescaler + 1U);

    HAL_TIM_Base_Start(&htim2);
}

static void CapMeter_ResetTimerUs(void)
{
    __HAL_TIM_SET_COUNTER(&htim2, 0U);
}

static uint32_t CapMeter_GetElapsedTicks(void)
{
    return __HAL_TIM_GET_COUNTER(&htim2);
}

static uint32_t CapMeter_UsToTicks(uint32_t us)
{
    uint64_t ticks;

    if (g_cap_timer_tick_hz == 0U) {
        return us;
    }

    ticks = ((uint64_t)us * (uint64_t)g_cap_timer_tick_hz + 999999ULL) / 1000000ULL;
    if (ticks == 0ULL) {
        ticks = 1ULL;
    }
    if (ticks > 0xFFFFFFFFULL) {
        ticks = 0xFFFFFFFFULL;
    }

    return (uint32_t)ticks;
}

static uint32_t CapMeter_TicksToUs(uint32_t ticks)
{
    uint64_t us;

    if (g_cap_timer_tick_hz == 0U) {
        return ticks;
    }

    us = ((uint64_t)ticks * 1000000ULL) / (uint64_t)g_cap_timer_tick_hz;
    if (us > 0xFFFFFFFFULL) {
        us = 0xFFFFFFFFULL;
    }

    return (uint32_t)us;
}

static void CapMeter_WaitUs(uint32_t duration_us)
{
    uint32_t start_ticks = CapMeter_GetElapsedTicks();
    uint32_t wait_ticks = CapMeter_UsToTicks(duration_us);

    while ((CapMeter_GetElapsedTicks() - start_ticks) < wait_ticks) {
    }
}

static uint32_t CapMeter_VoltageToDacCode(double voltage_v)
{
    if (voltage_v < 0.0) {
        voltage_v = 0.0;
    }
    if (voltage_v > 3.3) {
        voltage_v = 3.3;
    }

    return (uint32_t)((voltage_v / 3.3) * 4095.0 + 0.5);
}

static void CapMeter_SetCompThreshold(double voltage_v)
{
    uint32_t dac_code = CapMeter_VoltageToDacCode(voltage_v);

    HAL_DAC_Stop(&hdac2, DAC_CHANNEL_1);
    HAL_DAC_SetValue(&hdac2, DAC_CHANNEL_1, DAC_ALIGN_12B_R, dac_code);
    HAL_DAC_Start(&hdac2, DAC_CHANNEL_1);
}

static uint32_t CapMeter_CalcDischargeUsByRange(uint8_t idx)
{
    double t_s;

    if (idx >= (sizeof(g_cap_ranges) / sizeof(g_cap_ranges[0]))) {
        return 20000U;
    }

    t_s = (g_cap_ranges[idx].upper_f * 3.3) / CAP_CURRENT_A;
    t_s *= 1.2;

    if (t_s < 0.00002) {
        t_s = 0.00002;
    }

    return (uint32_t)(t_s * 1000000.0);
}

static double CapMeter_CalcFromElapsedTicks(uint32_t elapsed_ticks)
{
    double elapsed_s;

    if ((elapsed_ticks == 0U) || (g_cap_timer_tick_hz == 0U)) {
        return 0.0;
    }

    elapsed_s = (double)elapsed_ticks / (double)g_cap_timer_tick_hz;
    return (CAP_CURRENT_A * elapsed_s) / CAP_DV_V;
}

static uint8_t CapMeter_SuggestRangeIndex(double capacitance_f)
{
    if (capacitance_f <= g_cap_ranges[0].upper_f) {
        return 0U;
    }

    if (capacitance_f <= g_cap_ranges[1].upper_f) {
        return 1U;
    }

    return 2U;
}

static void CapMeter_PrepareHardware(uint8_t idx)
{
    uint32_t timer_hz = CAP_DEFAULT_TIMER_HZ;

    if (idx < (sizeof(g_cap_ranges) / sizeof(g_cap_ranges[0]))) {
        timer_hz = g_cap_ranges[idx].timer_tick_hz;
    }

    HAL_GPIO_WritePin(R_SW_GPIO_Port, R_SW_Pin, GPIO_PIN_SET);
    HAL_OPAMP_Start(&hopamp3);
    PowerBuffer_SetLevel(POWER_BUFFER_LEVEL_LOW);
    PGA_ChangeGain(16U);
    CapMeter_StartTimerBase(timer_hz);
    HAL_Delay(CAP_PATH_SETTLE_MS);
}

static void CapMeter_StopMeasurementPath(void)
{
    g_comp7_measure_active = 0U;
    g_comp7_wait_zero = 0U;
    HAL_COMP_Stop(&hcomp7);
    HAL_TIM_Base_Stop(&htim2);
    SetCurrent(CURRENT_RANGE_N500uA);
}

static void CapMeter_StartMeasurement(uint8_t idx)
{
    uint32_t discharge_us;
    uint32_t timeout_us;
    if (idx >= (sizeof(g_cap_ranges) / sizeof(g_cap_ranges[0]))) {
        return;
    }

    discharge_us = CapMeter_CalcDischargeUsByRange(idx);
    timeout_us = g_cap_ranges[idx].charge_timeout_us;

    CapMeter_PrepareHardware(idx);
    g_meas_active = 1U;
    g_meas_range_idx = idx;
    g_meas_discharge_ticks = CapMeter_UsToTicks(discharge_us);
    g_meas_timeout_ms = (timeout_us / 1000U) + 50U;
    g_meas_wait_start_ms = 0U;

    g_comp7_latched_us = 0U;
    g_comp7_wait_zero = 0U;
    g_comp7_zero_seen = 0U;
    g_comp7_second_edge_done = 0U;
    g_comp7_measure_active = 0U;

    g_last_stage = CAP_STAGE_DISCHARGE;
    SetCurrent(CURRENT_RANGE_N500uA);
    HAL_Delay(CAP_SWITCH_SETTLE_MS);
    CapMeter_ResetTimerUs();
    g_meas_discharge_start_ticks = CapMeter_GetElapsedTicks();
}

static void CapMeter_FinishMeasurement(uint8_t success, uint32_t latched_ticks)
{
    CapMeter_Result result = {0.0, 0U, 0U};

    CapMeter_StopMeasurementPath();
    g_meas_active = 0U;

    if (success == 0U) {
        result.over_range = 1U;
        result.valid = 0U;
        g_last_elapsed_us = 0U;
        g_last_result = result;
        g_last_measure_tick = HAL_GetTick();
        return;
    }

    g_last_elapsed_us = CapMeter_TicksToUs(latched_ticks);
    result.capacitance_f = CapMeter_CalcFromElapsedTicks(latched_ticks);
    if (result.capacitance_f <= 0.0) {
        result.over_range = 1U;
        g_last_result = result;
        g_last_measure_tick = HAL_GetTick();
        return;
    }

    result.valid = 1U;
    result.over_range = (result.capacitance_f > (g_cap_ranges[g_meas_range_idx].upper_f * CAP_OVER_RANGE_RATIO)) ? 1U : 0U;

    if ((g_cap_auto_mode != 0U) && (result.over_range != 0U) && (g_meas_range_idx < 2U)) {
        g_cap_range_idx = (uint8_t)(g_meas_range_idx + 1U);
        CapMeter_StartMeasurement(g_cap_range_idx);
        return;
    }

    if ((g_cap_auto_mode != 0U) && (result.valid != 0U) && (result.over_range == 0U)) {
        g_cap_range_idx = CapMeter_SuggestRangeIndex(result.capacitance_f);
    }

    g_last_stage = CAP_STAGE_DONE;
    g_last_result = result;
    g_last_measure_tick = HAL_GetTick();
}

static void CapMeter_ServiceMeasurement(void)
{
    if (g_meas_active == 0U) {
        return;
    }

    if (g_last_stage == CAP_STAGE_DISCHARGE) {
        uint32_t elapsed = CapMeter_GetElapsedTicks() - g_meas_discharge_start_ticks;
        if (elapsed < g_meas_discharge_ticks) {
            return;
        }

        CapMeter_SetCompThreshold(CAP_COMP_V_PRECHARGE);
        g_comp7_latched_us = 0U;
        g_comp7_measure_active = 0U;
        g_comp7_wait_zero = 1U;
        g_comp7_zero_seen = 0U;
        g_comp7_second_edge_done = 0U;
        HAL_COMP_Stop(&hcomp7);
        HAL_COMP_Start(&hcomp7);
        CapMeter_WaitUs(CAP_COMP_SETTLE_US);

        g_last_stage = CAP_STAGE_CHARGE;
        HAL_TIM_Base_Stop(&htim2);
        CapMeter_ResetTimerUs();
        g_comp7_measure_active = 1U;
        SetCurrent(CURRENT_RANGE_500uA);

        g_last_stage = CAP_STAGE_WAIT_IRQ;
        g_meas_wait_start_ms = HAL_GetTick();
        return;
    }

    if (g_last_stage == CAP_STAGE_WAIT_IRQ) {
        if (g_comp7_second_edge_done != 0U) {
            g_comp7_second_edge_done = 0U;
            CapMeter_FinishMeasurement(1U, g_comp7_latched_us);
            return;
        }

        if ((HAL_GetTick() - g_meas_wait_start_ms) > g_meas_timeout_ms) {
            CapMeter_FinishMeasurement(0U, 0U);
        }
    }
}

static uint8_t CapMeter_ShouldRefresh(void)
{
    uint32_t refresh_ms = g_cap_ranges[g_cap_range_idx].refresh_ms;

    if (g_last_result.valid == 0U) {
        return 1U;
    }

    if ((HAL_GetTick() - g_last_measure_tick) >= refresh_ms) {
        return 1U;
    }

    return 0U;
}

static void CapMeter_EnsureMeasurement(void)
{
    if (g_meas_active != 0U) {
        CapMeter_ServiceMeasurement();
        return;
    }

    if (CapMeter_ShouldRefresh() == 0U) {
        return;
    }

    CapMeter_StartMeasurement(g_cap_range_idx);
    CapMeter_ServiceMeasurement();
}

static void CapMeter_DisplayValue(const CapMeter_Result *result)
{
    char disp_str[24];

    if ((result == NULL) || (result->valid == 0U) || (result->over_range != 0U)) {
        snprintf(disp_str, sizeof(disp_str), " 0L");
        OLED_PrintString(0, 20, disp_str, &font16x16, OLED_COLOR_NORMAL);
        snprintf(disp_str, sizeof(disp_str), "S%d t:%lums", (int)g_last_stage,
                 (unsigned long)(g_last_elapsed_us / 1000U));
        OLED_PrintString(0, 40, disp_str, &font16x16, OLED_COLOR_NORMAL);
        return;
    }

    if (result->capacitance_f >= 1e-6) {
        snprintf(disp_str, sizeof(disp_str), " %.3f uF", result->capacitance_f * 1e6);
    } else if (result->capacitance_f >= 1e-9) {
        snprintf(disp_str, sizeof(disp_str), " %.2f nF", result->capacitance_f * 1e9);
    } else {
        snprintf(disp_str, sizeof(disp_str), " %.1f pF", result->capacitance_f * 1e12);
    }
    OLED_PrintString(0, 20, disp_str, &font16x16, OLED_COLOR_NORMAL);

    snprintf(disp_str, sizeof(disp_str), "t:%lums", (unsigned long)(g_last_elapsed_us / 1000U));
    OLED_PrintString(0, 40, disp_str, &font16x16, OLED_COLOR_NORMAL);
}

static void CapMeter_DisplayCommon(void)
{
    char disp_str[24];

    CapMeter_EnsureMeasurement();

    if (g_cap_auto_mode != 0U) {
        snprintf(disp_str, sizeof(disp_str), "CAP A %s", g_cap_ranges[g_cap_range_idx].name);
    } else {
        snprintf(disp_str, sizeof(disp_str), "CAP %s", g_cap_manual_name);
    }
    OLED_PrintString(0, 0, disp_str, &font16x16, OLED_COLOR_NORMAL);

    CapMeter_DisplayValue(&g_last_result);
}

void CapMeter_Init(void)
{
    g_cap_auto_mode = 0U;
    g_cap_range_idx = 1U;
    g_cap_manual_name = "2uF";
    g_last_result.valid = 0U;
    g_last_result.over_range = 0U;
    g_last_result.capacitance_f = 0.0;
    g_last_measure_tick = 0U;
    g_last_stage = CAP_STAGE_IDLE;
    g_last_elapsed_us = 0U;
    g_meas_active = 0U;

    CapMeter_PrepareHardware(g_cap_range_idx);
    SetCurrent(CURRENT_RANGE_N500uA);
    CapMeter_SetCompThreshold(CAP_COMP_V_PRECHARGE);
    HAL_COMP_Stop(&hcomp7);
}

void CapMeter_20nF_Start(void)
{
    g_cap_auto_mode = 0U;
    g_cap_range_idx = 0U;
    g_cap_manual_name = "20nF";
    g_last_result.valid = 0U;
    g_meas_active = 0U;
}

void CapMeter_2uF_Start(void)
{
    g_cap_auto_mode = 0U;
    g_cap_range_idx = 1U;
    g_cap_manual_name = "2uF";
    g_last_result.valid = 0U;
    g_meas_active = 0U;
}

void CapMeter_200uF_Start(void)
{
    g_cap_auto_mode = 0U;
    g_cap_range_idx = 2U;
    g_cap_manual_name = "200uF";
    g_last_result.valid = 0U;
    g_meas_active = 0U;
}

void CapMeter_Auto_Start(void)
{
    g_cap_auto_mode = 1U;
    g_cap_range_idx = 1U;
    g_last_result.valid = 0U;
    g_meas_active = 0U;
}

void CapMeter_20nF_Display(void)
{
    CapMeter_DisplayCommon();
}

void CapMeter_2uF_Display(void)
{
    CapMeter_DisplayCommon();
}

void CapMeter_200uF_Display(void)
{
    CapMeter_DisplayCommon();
}

void CapMeter_Auto_Display(void)
{
    g_cap_auto_mode = 1U;
    CapMeter_DisplayCommon();
}

void HAL_COMP_TriggerCallback(COMP_HandleTypeDef *hcomp)
{
    if ((hcomp == NULL) || (hcomp->Instance != COMP7)) {
        return;
    }

    if (g_comp7_measure_active == 0U) {
        return;
    }

    if (g_comp7_wait_zero != 0U) {
        g_comp7_zero_seen = 1U;
        g_comp7_wait_zero = 0U;
        CapMeter_ResetTimerUs();
        HAL_TIM_Base_Start(&htim2);
        CapMeter_SetCompThreshold(CAP_COMP_V_TRIGGER);
        return;
    }

    g_comp7_latched_us = CapMeter_GetElapsedTicks();
    g_comp7_second_edge_done = 1U;
    g_comp7_measure_active = 0U;
}
