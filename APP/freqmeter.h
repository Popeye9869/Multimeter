#ifndef __FREQMETER_H__
#define __FREQMETER_H__

#include "main.h"

void FreqMeter_Init(void);

void FreqMeter_20Hz_Start(void);
void FreqMeter_200Hz_Start(void);
void FreqMeter_2kHz_Start(void);
void FreqMeter_20kHz_Start(void);
void FreqMeter_200kHz_Start(void);
void FreqMeter_Auto_Start(void);

void FreqMeter_20Hz_Display(void);
void FreqMeter_200Hz_Display(void);
void FreqMeter_2kHz_Display(void);
void FreqMeter_20kHz_Display(void);
void FreqMeter_200kHz_Display(void);
void FreqMeter_Auto_Display(void);

#endif
