#include "key.h"

#define KEY_DEBOUNCE_MS       30U
#define KEY_LONG_PRESS_MS     1500U
#define KEY_DOUBLE_CLICK_MS   300U

#define KEY_FLAG_SHORT        (1U << 0)
#define KEY_FLAG_LONG         (1U << 1)
#define KEY_FLAG_DOUBLE       (1U << 2)

static volatile uint8_t g_pressed = 0U;
static volatile uint8_t g_long_reported = 0U;
static volatile uint8_t g_click_count = 0U;
static volatile uint8_t g_event_flags = 0U;

static volatile uint32_t g_last_irq_tick = 0U;
static volatile uint32_t g_press_tick = 0U;
static volatile uint32_t g_release_tick = 0U;
static volatile uint32_t g_first_click_tick = 0U;

static uint8_t Key_ReadPressedLevel(void)
{
	return (HAL_GPIO_ReadPin(KEY_GPIO_Port, KEY_Pin) == GPIO_PIN_RESET) ? 1U : 0U;
}

void Key_Init(void)
{
	g_pressed = Key_ReadPressedLevel();
	g_long_reported = 0U;
	g_click_count = 0U;
	g_event_flags = 0U;
	g_last_irq_tick = HAL_GetTick();
	g_press_tick = g_last_irq_tick;
	g_release_tick = g_last_irq_tick;
	g_first_click_tick = g_last_irq_tick;
}

void Key_EXTI_Callback(uint16_t gpio_pin)
{
	uint32_t now;
	uint8_t pressed_now;

	if (gpio_pin != KEY_Pin) {
		return;
	}

	now = HAL_GetTick();
	if ((now - g_last_irq_tick) < KEY_DEBOUNCE_MS) {
		return;
	}

	g_last_irq_tick = now;
	pressed_now = Key_ReadPressedLevel();

	if (pressed_now == g_pressed) {
		return;
	}

	g_pressed = pressed_now;
	if (g_pressed != 0U) {
		g_press_tick = now;
		g_long_reported = 0U;
		return;
	}

	g_release_tick = now;
	if (g_long_reported != 0U) {
		g_click_count = 0U;
		return;
	}

	if (g_click_count == 0U) {
		g_click_count = 1U;
		g_first_click_tick = now;
		return;
	}

	if ((now - g_first_click_tick) <= KEY_DOUBLE_CLICK_MS) {
		g_event_flags |= KEY_FLAG_DOUBLE;
		g_click_count = 0U;
		return;
	}

	g_click_count = 1U;
	g_first_click_tick = now;
}

void Key_Process(void)
{
	uint32_t now = HAL_GetTick();

	if ((g_pressed != 0U) && (g_long_reported == 0U) && ((now - g_press_tick) >= KEY_LONG_PRESS_MS)) {
		g_event_flags |= KEY_FLAG_LONG;
		g_long_reported = 1U;
		g_click_count = 0U;
	}

	if ((g_click_count == 1U) && ((now - g_release_tick) >= KEY_DOUBLE_CLICK_MS)) {
		g_event_flags |= KEY_FLAG_SHORT;
		g_click_count = 0U;
	}
}

KeyEvent Key_GetEvent(void)
{
	uint8_t flags;

	__disable_irq();
	flags = g_event_flags;
	if ((flags & KEY_FLAG_DOUBLE) != 0U) {
		g_event_flags = (uint8_t)(flags & (uint8_t)(~KEY_FLAG_DOUBLE));
		__enable_irq();
		return KEY_EVENT_DOUBLE;
	}

	if ((flags & KEY_FLAG_LONG) != 0U) {
		g_event_flags = (uint8_t)(flags & (uint8_t)(~KEY_FLAG_LONG));
		__enable_irq();
		return KEY_EVENT_LONG_2S;
	}

	if ((flags & KEY_FLAG_SHORT) != 0U) {
		g_event_flags = (uint8_t)(flags & (uint8_t)(~KEY_FLAG_SHORT));
		__enable_irq();
		return KEY_EVENT_SHORT;
	}

	__enable_irq();
	return KEY_EVENT_NONE;
}

uint8_t Key_IsPressed(void)
{
	return g_pressed;
}

void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
	Key_EXTI_Callback(GPIO_Pin);
}
