//=============================================================================
//
//   @file    MAIN projecto file - STM32F Stepper Driver test
//   @author  Paulo de Almeida
//   @version V1.0.0
//   @date    03/08/2013
//   @brief   Main entry file
//
//	  COPYRIGHT (C) 2013 Paulo de Almeida
//
//   for ASM output (.txt file) use  =>  --asm --interleave on compiler options
//   
//==============================================================================
#define  __MAIN_C
#include "stm32f_stpdrv.h"

//==============================================================================
//	descri:   delay em milisegundos SEM CHAMAR Dispatcher
//	params:	 numero de milisegundos
//	return:	 none
//
// Esta rotina foi testada e é exacta para um STM32F4 a 168Mhz
//
#define DELAY_ms_LOOP(x) ((SystemCoreClock/4000)*x)  
void _delay(uint32_t _ms)
{  
	uint32_t i = 0;
	for ( i = DELAY_ms_LOOP(_ms); i; i--) __nop();
}
//==============================================================================


//==============================================================================
//	descri:   Main entry point func
//	params:	 none
//	return:	 none
//
int main(void)
{                   
	
	// Initialize Stepper Driver Firmware
	STPDRV_Init();

	// STM32F4_DISCOVERY stuf ... if used
#ifdef __STM32F4_DISCOVERY_H
	STM32F4_Discovery_LEDInit(LED3);
	STM32F4_Discovery_LEDInit(LED4);
	STM32F4_Discovery_LEDInit(LED5);
	STM32F4_Discovery_PBInit(BUTTON_USER, BUTTON_MODE_GPIO);
#endif
	
	
	// Test Stepper Driver
   STPDRV_Move(MOTOR1, dir_CW, 100);
	
	while (1) {
		_delay(1000);
	
#ifdef __STM32F4_DISCOVERY_H
		if (STM32F4_Discovery_PBGetState(BUTTON_USER)==Bit_SET) {
			STM32F4_Discovery_LEDOn(LED5);
			STPDRV_Move(MOTOR1, dir_CW, 30);
		}
#endif
	};
		
}
//==============================================================================

//==============================================================================
#ifdef  USE_FULL_ASSERT
void assert_failed(uint8_t * file, uint32_t line) {
  while (1)
  {}
}
#endif
//==============================================================================

//=============================================================================
// EOF  main.c 
