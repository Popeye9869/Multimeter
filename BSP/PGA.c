#include "PGA.h"
#include "opamp.h"
#include "stm32g4xx_hal_opamp.h"

OPAMP_HandleTypeDef *PGA = &hopamp2; // 定义一个指向OPAMP的指针，方便后续操作

HAL_StatusTypeDef PGA_Init()
{
    //启动运放PGA模式opamp2
    return HAL_OPAMP_Start(PGA);
}

HAL_StatusTypeDef PGA_ChangeGain(uint32_t gain)
{
    HAL_OPAMP_Stop(PGA); // 先停止OPAMP以便修改增益设置

    HAL_OPAMP_DeInit(PGA); // 反初始化OPAMP以重置配置


    if(gain ==1)
    {
        PGA->Init.Mode = OPAMP_FOLLOWER_MODE; // 如果增益为1，则设置为跟随器模式
        HAL_OPAMP_Init(PGA);
        return  PGA_Init();
    }
    else
    {
        PGA->Init.Mode = OPAMP_PGA_MODE; // 否则设置为PGA模式
    }


    //增益设置函数
    if (gain == 2)
    {
        PGA->Init.PgaGain = OPAMP_PGA_GAIN_2_OR_MINUS_1;
    }
    else if (gain == 4)
    {
        PGA->Init.PgaGain = OPAMP_PGA_GAIN_4_OR_MINUS_3;
    }
    else if (gain == 8)
    {
        PGA->Init.PgaGain = OPAMP_PGA_GAIN_8_OR_MINUS_7;
    }
    else if (gain == 16)
    {
        PGA->Init.PgaGain = OPAMP_PGA_GAIN_16_OR_MINUS_15;
    }
    else if(gain == 32)
    {
        PGA->Init.PgaGain = OPAMP_PGA_GAIN_32_OR_MINUS_31;
    }
    else if (gain == 64)
    {
        PGA->Init.PgaGain = OPAMP_PGA_GAIN_64_OR_MINUS_63;
    }
    else
    {
        return HAL_ERROR; // 如果增益值不合法，返回错误
    }

    // 重新初始化OPAMP以应用新的增益设置
    HAL_OPAMP_Init(PGA);

    return  PGA_Init();
}
