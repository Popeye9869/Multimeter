#include "main.h"


typedef enum {
	APP_MODE_DCV = 0,
	APP_MODE_ACV,
	APP_MODE_FREQ,
	APP_MODE_OHM,
	APP_MODE_DIODE,
	APP_MODE_CONT,
	APP_MODE_COUNT
} AppMode;


extern AppMode g_mode;


void APP_Init();

void DispProc();
void ChangMode();
void ChangRange();

void APP_Proc();
