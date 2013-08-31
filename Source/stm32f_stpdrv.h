/*=============================================================================
  
	@file    stm32f_stpdrv.h
   @author  Paulo de Almeida
   @version V1.3.0
   @date    02/08/2013
   @brief   Stepper Driver for STM32F family
  
   This Software is released under no garanty.
	You may use this software for personal use.
	Use for commercial and/or profit applications is strictly prohibited.
	
	If you change this code, please send an email to the author.
    
  	COPYRIGHT (C) 2013 Paulo de Almeida
   Contact the author at:  pa@astrosite.net
  
   Atention: This header MUST NOT BE REMOVED
  
   Compiled under C99 (ISO/IEC 9899:1999) version
   please use the "--c99" compiler directive
	
   ===================================================================
	                    Description (in portuguese) 
   ===================================================================
	- 	STM32Fx compatível (usa "StdPeriph Lib" da ST - não testado com o STM32F0xx)
	- 	Funciona com stepper ICs que usam a interface SD (translator, ou com inputs "Step" e "Dir")
	- 	Controla 2 motores totalmente independentes 
	- 	Rampa de aceleração e desaceleração configurável e independente para cada motor (um pode estar a
		acelerar e o outro a desacelerar)
	-  Velocidades de 2 a 1000 passos por segundo (pode ser alterado)
	-  Velocidade constante (Move) ou posicionamento (Goto) independente e simultânea para os dois motores
	- 	Contador com a posição actual do motor (respeita a direcção dos movimentos)
	- 	Direcção CW (clockwise) ou CCW (counterclockwise )
	- 	Usa somente um TIMER (TIMER3, pode ser alterado) 
	- 	Permite assignar qualquer pino IO para DIR e STEP
	- 	E mais umas cenas ...


   ===================================================================
                       How to use this driver
   ===================================================================       
	1 - Editar o ficheiro stm32f_stpdrv.h e definir o hardware e as preferencias
	2 - Chamar a função STPDRV_Init() para inicializar o wardware e a API
	3 - Chamar a função STPDRV_SetRamp(...) para definir os parametros de cada motor
	4 - Usar as funções da API para controlar os motores
	
	
   ===================================================================
                               API
   ===================================================================       
	void STPDRV_Init(void)
			Descri: Inicializa a API e o hardware
			 Parms: 	none
			Return: 	none


	void STPDRV_SetRamp(int16_t motor, int16_t rampspeed)
			Descri: 	Define os parâmetros da curva de aceleração/desaceleração 
			 Parms: 	motor - motor em questão, MOTOR1 ou MOTOR2
						rampspeed - velocidade da aceleração/desaceleração em steps/sec/sec
			Return:  none


	int32_t STPDRV_GetPos(int16_t motor)
			Descri: 	Para obter a posição (contador de passos) actual
						O valor da posição aumenta sempre que o motor avança um passo na
						direcção dir_CW e diminui se o motor é movido na direcção dir_CCW
			 Parms: 	motor - motor em questão, MOTOR1 ou MOTOR2
			Return:  um int32 com o valor da posição


	int32_t STPDRV_GetDir(int16_t motor)
			Descri: 	Para obter a direcção de movimento actual ou a ultima usada se o motor
						estiver parado
			 Parms: 	motor - motor em questão, MOTOR1 ou MOTOR2
			Return:  dir_CW (clockwise) ou dir_CCW (counterclockwise)


	mstate_t	STPDRV_GetState(int16_t motor)
			Descri: 	Para obter o estado actual do motor
			 Parms: 	motor - motor em questão, MOTOR1 ou MOTOR2
			Return:  mstat_Stop = motor está parado, mstat_Move = motor está em movimento
						mstat_GoTo  = motor está em movimento para uma determinada posição em
						consequência de um comando "STPDRV_Goto(...)"


	void STPDRV_Move(int16_t motor, mdir_t direction, int16_t speed)
   		Descri: 	Para mover o motor. Depois de executado este comando o motor fica a rodar
						no sentido indicado á velocidade indicada
			 Parms: 	motor - motor em questão, MOTOR1 ou MOTOR2
						direction - sentido em que o motor deve rodar, pode ser dir_CW ou dir_CCW
						speed - velocidade de rotação em steps/sec
			Return:  none


	void STPDRV_Goto(int16_t motor, int32_t position, int16_t speed, mdir_t movedir)
   		Descri: 	Move o motor para uma determinada posição. Depois de executado este comando
						o motor move á velocidade indicada até ser atingida a posição indicada
			 Parms: 	motor - motor em questão, MOTOR1 ou MOTOR2
						position - posição onde o motor deve parar
						speed - velocidade de rotação maxima em steps/sec
						movedir - sentido em que o motor deve rodar, pode ser dir_CW ou dir_CCW ou
						ainda dir_ANY para usar o sentido mais curto
			Return:  none


	void STPDRV_Stop(int16_t motor, int16_t hardstop)
			Descri: 	Para parar o motor 
			 Parms: 	motor - motor em questão, MOTOR1 ou MOTOR2
						hardstop - se "1" pára o motor imediatamente, se "0" pára o motor com desaceleração
			Return: none
	
	
==============================================================================*/
#ifndef  __stm32f_stpdrv_h    // DO NOT CHANGE
#define  __stm32f_stpdrv_h    // DO NOT CHANGE

// USER EDIT - Edit the line below to reflect your hardware
//#include "stm32f4xx.h"
//#include "stm32f3xx.h"
//#include "stm32f2xx.h"
#include "stm32f10x.h"

// USER EDIT - Comment the line below if not using STM32F4Discovey board
//#include "stm32f4_discovery.h"


// USER EDIT - Edit the 8 lines below to reflect your hardware
#define MOTOR1_STEP_PORT			GPIOE				// IO port were the "step" pin is connected
#define MOTOR1_STEP_PIN         	GPIO_Pin_9     // PIN that is connected to the "STEP input" of the stepper IC
#define MOTOR1_DIR_PORT         	GPIOE				// IO port were the "dir" pin is connected
#define MOTOR1_DIR_PIN          	GPIO_Pin_9     // PIN that is connected to the "DIR input" of the stepper IC

#define MOTOR2_STEP_PORT        	GPIOE				// IO port were the "step" pin is connected
#define MOTOR2_STEP_PIN         	GPIO_Pin_11		// PIN that is connected to the "STEP input" of the stepper IC
#define MOTOR2_DIR_PORT         	GPIOE				// IO port were the "dir" pin is connected
#define MOTOR2_DIR_PIN          	GPIO_Pin_9		// PIN that is connected to the "DIR input" of the stepper IC


// USER EDIT - If you use NVIC Preemption Priority Bits edit de 2 lines below to
//					reflect your configuration, if you do not use the NVIC Priority
//					just leave it as is
#define IRQ_STPDRV_PrePriority	0x00
#define IRQ_STPDRV_Priority      0x00



/* ===========================================================================*/
/* STOP ! - Private structs and vars - DO NOT CHANGE FROM THIS POINT ON 		*/
/* ===========================================================================*/

//---- API enums
typedef enum 	{dir_CW = (int8_t) 0, dir_CCW = (int8_t) 1, dir_ANY = (int8_t) 2}  mdir_t;
typedef enum 	{mstat_Stop  = (int8_t) 0, mstat_Move  = (int8_t) 1, mstat_GoTo  = (int8_t) 2} mstate_t;
#define MOTOR1  0
#define MOTOR2  1

//-----------------------------------------------------------------------------
// Motors
#define STPDRV_TIM          	TIM3
#define STPDRV_TIMFREQ        100000   // 200Khz reais uma vez que funciona em "togle", resolução final de 10us entre steps
#define STPDRV_MINSETPSEC     2        // minimo de steps/sec, deve satisfazer a condição: STPDRV_TIMFREQ / STPDRV_MINSETPSEC < 65535
#define STPDRV_MAXSETPSEC     1000     // maximo de steps/sec, deve satisfazer a condição: STPDRV_TIMFREQ / STPDRV_MAXSETPSEC > 100
#define STPDRV_TIM_APB      	RCC_APB1Periph_TIM3 // APB clock do timer usado


//-----------------------------------------------------------------------------
// Exported API Funcs
void 		STPDRV_Init(void);
void 		STPDRV_SetRamp(int16_t motor, int16_t rampspeed);
int32_t 	STPDRV_GetPos(int16_t motor);
mdir_t 	STPDRV_GetDir(int16_t motor);
mstate_t	STPDRV_GetState(int16_t motor);
void 		STPDRV_Move(int16_t motor, mdir_t direction, int16_t speed);
void 		STPDRV_Goto(int16_t motor, int32_t position, int16_t speed, mdir_t movedir);
void 		STPDRV_Stop(int16_t motor, int16_t hardstop);

#endif  // __stm32f_stpdrv_h
				  
//=============================================================================
// EOF stm32f_stpdrv.h


