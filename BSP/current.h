#include "main.h"

typedef enum {
    CURRENT_RANGE_10uA = 0,
    CURRENT_RANGE_15uA, //200K欧姆档，最大可测3.3V/10uA=330K-600欧姆
    CURRENT_RANGE_150uA,  //20K档，最大可测3.3V/150uA=22000-600欧姆
    CURRENT_RANGE_400uA,
    CURRENT_RANGE_500uA, //2K欧姆档，最大可测3.3V/500uA=6600-600欧姆
    CURRENT_RANGE_N500uA//反向
} Current_Type;


void SetCurrent(Current_Type uA);
