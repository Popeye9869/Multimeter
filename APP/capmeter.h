#ifndef __CAPMETER_H__
#define __CAPMETER_H__

#include "main.h"

void CapMeter_Init(void);

void CapMeter_20nF_Start(void);
void CapMeter_2uF_Start(void);
void CapMeter_200uF_Start(void);
void CapMeter_Auto_Start(void);

void CapMeter_20nF_Display(void);
void CapMeter_2uF_Display(void);
void CapMeter_200uF_Display(void);
void CapMeter_Auto_Display(void);

#endif