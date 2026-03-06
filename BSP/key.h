#ifndef __KEY_H__
#define __KEY_H__

#include "main.h"

typedef enum {
	KEY_EVENT_NONE = 0,
	KEY_EVENT_SHORT,
	KEY_EVENT_LONG_2S,
	KEY_EVENT_DOUBLE
} KeyEvent;

void Key_Init(void);
void Key_Process(void);
void Key_EXTI_Callback(uint16_t gpio_pin);
KeyEvent Key_GetEvent(void);
uint8_t Key_IsPressed(void);

#endif
