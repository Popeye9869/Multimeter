#include "PGA.h"
#include "opamp.h"
#include "stm32g4xx_hal_opamp.h"

HAL_StatusTypeDef PGA_Init()
{
    //启动运放PGA模式opamp2
    return HAL_OPAMP_Start(&hopamp2);
}

HAL_StatusTypeDef PGA_ChangeGain(uint32_t gain)
{
    HAL_OPAMP_Stop(&hopamp2); // 先停止OPAMP以便修改增益设置

    HAL_OPAMP_DeInit(&hopamp2); // 反初始化OPAMP以重置配置


    if(gain ==1)
    {
        hopamp2.Init.Mode = OPAMP_FOLLOWER_MODE; // 如果增益为1，则设置为跟随器模式
    }
    else
    {
        hopamp2.Init.Mode = OPAMP_PGA_MODE; // 否则设置为PGA模式
    }


    //增益设置函数
    if (gain == 2)
    {
        hopamp2.Init.PgaGain = OPAMP_PGA_GAIN_2_OR_MINUS_1;
    }
    else if (gain == 4)
    {
        hopamp2.Init.PgaGain = OPAMP_PGA_GAIN_4_OR_MINUS_3;
    }
    else if (gain == 8)
    {
        hopamp2.Init.PgaGain = OPAMP_PGA_GAIN_8_OR_MINUS_7;
    }
    else if (gain == 16)
    {
        hopamp2.Init.PgaGain = OPAMP_PGA_GAIN_16_OR_MINUS_15;
    }

    // 重新初始化OPAMP以应用新的增益设置
    HAL_OPAMP_Init(&hopamp2);

    return  PGA_Init();
}
