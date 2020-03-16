/*
    bspLcd.c

    Board support for controlling the ILI9341 LCD on the Adafruit '2.8" TFT LCD w/Cap Touch' via NUCLEO-F401RE MCU

    Developed for University of Washington embedded systems programming certificate
    
    2016/2 Nick Strathy wrote/arranged it
*/

#include "bsp.h"


// Initializes GPIO pins for the ILI9341 LCD device.
void BspLcdInitILI9341()
{
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOB | RCC_AHB1Periph_GPIOC , ENABLE);

    GPIO_InitTypeDef GPIO_InitStruct;
    
    /*-------- Configure CS ChipSelect Pin PB6 --------*/ 
 
    GPIO_InitStruct.GPIO_Pin = LCD_ILI9341_CS_GPIO_Pin;
    GPIO_InitStruct.GPIO_Mode = GPIO_Mode_OUT;
    GPIO_InitStruct.GPIO_Speed = GPIO_Speed_100MHz;
    GPIO_InitStruct.GPIO_OType = GPIO_OType_PP;
    GPIO_InitStruct.GPIO_PuPd = GPIO_PuPd_UP;
     
    GPIO_Init(LCD_ILI9341_CS_GPIO, &GPIO_InitStruct);
    LCD_ILI9341_CS_DEASSERT();
 
    /*-------- Configure DC Data/Command Pin PC7 --------*/ 
 
    GPIO_InitStruct.GPIO_Pin = LCD_ILI9341_DC_GPIO_Pin;
    GPIO_InitStruct.GPIO_Mode = GPIO_Mode_OUT;
    GPIO_InitStruct.GPIO_Speed = GPIO_Speed_100MHz;
    GPIO_InitStruct.GPIO_OType = GPIO_OType_PP;
    GPIO_InitStruct.GPIO_PuPd = GPIO_PuPd_UP;
     
    GPIO_Init(LCD_ILI9341_DC_GPIO, &GPIO_InitStruct);
    
    /*-------- Configure DC Data/Command Pin PB4 --------*/ 
    // RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOB, ENABLE); // already enabled
    GPIO_InitStruct.GPIO_Pin = GPIO_Pin_4; // PB4/D5 
    GPIO_InitStruct.GPIO_Mode = GPIO_Mode_IN;
    GPIO_InitStruct.GPIO_Speed = GPIO_Speed_2MHz; // why this clock rate?
    GPIO_InitStruct.GPIO_OType = GPIO_OType_OD;
    GPIO_InitStruct.GPIO_PuPd = GPIO_PuPd_NOPULL;
    GPIO_Init(GPIOB, &GPIO_InitStruct);
    EXTI_InitTypeDef EXTI_InitStruct;
    EXTI_InitStruct.EXTI_Line = EXTI_Line4;
    EXTI_InitStruct.EXTI_Mode = EXTI_Mode_Interrupt;
    EXTI_InitStruct.EXTI_Trigger = EXTI_Trigger_Falling;
    EXTI_InitStruct.EXTI_LineCmd = ENABLE;
    EXTI_Init(&EXTI_InitStruct);
    // Line4 == BP4 so set external interrupt to GPIOB Line 4
    SYSCFG_EXTILineConfig(EXTI_PortSourceGPIOB, 4);
    // Enable IRQ on line4 for NVIC
    NVIC_EnableIRQ(EXTI4_IRQn);
}