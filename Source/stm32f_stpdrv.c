/*=============================================================================
  
    @file    stm32f_stpdrv.c
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

   Description and Usage: See stm32f_stpdrv.h

==============================================================================*/
#include "stm32f_stpdrv.h"

/* ===========================================================================*/
/* Private structs and vars - DO NOT CHANGE !											*/
/* ===========================================================================*/

//---- Motor struct
typedef struct {
    int32_t		Pos;				// Actual position, in steps count
    mdir_t		Dir;				// Actual or last direction used
    mstate_t		State;			// Actual motor state (see mstate_t)

    // control fields - IGNORE THIS FIELDS
    uint16_t			CurDelay;         // Delay actual a ser carregado para o CCR
    uint16_t 		RampDelay;      	// Acel/Deacel rate (calculado a partir dos steps/sec/sec passados na função "STPDRV_SetRamp"
    uint16_t 		RampSlop;    		// Slope increment/decrement for each RampDelay
    __IO uint16_t	TargetSpeed;		// Velocidade a atingir em STEPS/SEC, se ZERO indica que não existe nada para atingir.
    // Se for maior que ZERO indica o valor para o qual o sistema deve progredir, o incremento
    // ou decremento de CurDelay é efectuado na rotina IRQ do timer que controla  as acelerações
    // e desacelerações
    uint16_t			TargetSpeed2;		// Executado depois de TargetSpeed quando a Dir pretendida é diferente e é necessário parar
    // motor primeiro. Se maior que ZERO inverter a direcção após terminar TargetSpeed e executar
    // novamente com este valor
    __IO uint16_t	TargetCurSpeed; 	// Velocidade em STEPS/SEC actual da aceleração/desaceleração
    mstate_t			TargetState;		// Estado a estabelecer DEPOIS de CurDelay ter alcançado TargetDelay
} TMotor;


//---- Motor control vars
TMotor Motors[2] = {{(int32_t)0x0, (mdir_t)0, (mstate_t)0, (uint16_t) 0xffff },
                    {(int32_t)0x0, (mdir_t)0, (mstate_t)0, (uint16_t) 0xffff }};


//----- Private Function Prototypes - DO NOT USE
static void 		__MotorOff(int16_t mt);
static void 		__MotorOn(int16_t mt);
static void 		__MotorSetDir(int16_t mt, mdir_t _dir);
static void 		__ResetTargetSpeed(int16_t mt);
static void 		__TargetSpeedDone(int16_t mt);
static void 		__SetTargetSpeed(int16_t mt, uint16_t _speed, mdir_t _dir, mstate_t _state);
static void 		__OnRampTimer(int16_t mt);
static uint32_t 	__GPIO2AHB1Periph(GPIO_TypeDef *_qual);

//==============================================================================
//
void STPDRV_Init(void)
{
    GPIO_InitTypeDef 				GPIO_InitStructure;
    NVIC_InitTypeDef 				NVIC_InitStructure;
    TIM_TimeBaseInitTypeDef  	TIM_TimeBaseStructure;
    TIM_OCInitTypeDef  			TIM_OCInitStructure;

    SystemCoreClockUpdate();

    //----- GPIO AHB1Periph clock enable
    /*RCC_AHB1PeriphClockCmd(__GPIO2AHB1Periph(MOTOR1_STEP_PORT) , ENABLE);
    RCC_AHB1PeriphClockCmd(__GPIO2AHB1Periph(MOTOR1_DIR_PORT)  , ENABLE);
    RCC_AHB1PeriphClockCmd(__GPIO2AHB1Periph(MOTOR2_STEP_PORT) , ENABLE);
    RCC_AHB1PeriphClockCmd(__GPIO2AHB1Periph(MOTOR2_DIR_PORT)  , ENABLE);*/
    RCC_APB2PeriphClockCmd(__GPIO2AHB1Periph(MOTOR1_STEP_PORT) , ENABLE);
    RCC_APB2PeriphClockCmd(__GPIO2AHB1Periph(MOTOR1_DIR_PORT)  , ENABLE);
    RCC_APB2PeriphClockCmd(__GPIO2AHB1Periph(MOTOR2_STEP_PORT) , ENABLE);
    RCC_APB2PeriphClockCmd(__GPIO2AHB1Periph(MOTOR2_DIR_PORT)  , ENABLE);

    // GPIO Configuration - Step PINs & DIR PINs
    //GPIO_InitStructure.GPIO_Mode = GPIO_Mode_OUT;
    //GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    //GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
    //GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_DOWN;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;

    GPIO_InitStructure.GPIO_Pin = MOTOR1_STEP_PIN;
    GPIO_Init(MOTOR1_STEP_PORT, &GPIO_InitStructure);
    GPIO_InitStructure.GPIO_Pin = MOTOR1_DIR_PIN;
    GPIO_Init(MOTOR1_DIR_PORT, &GPIO_InitStructure);

    GPIO_InitStructure.GPIO_Pin = MOTOR2_STEP_PIN;
    GPIO_Init(MOTOR2_STEP_PORT, &GPIO_InitStructure);
    GPIO_InitStructure.GPIO_Pin = MOTOR2_DIR_PIN;
    GPIO_Init(MOTOR2_DIR_PORT, &GPIO_InitStructure);


    //----- API struc  INIT (after GPIO init)
    Motors[0].CurDelay = 0xffff;
    __ResetTargetSpeed(0);
    __MotorSetDir(0, dir_CW);
    STPDRV_SetRamp(0, 4);
    Motors[1].CurDelay = 0xffff;
    __MotorSetDir(1, dir_CW);
    __ResetTargetSpeed(1);
    STPDRV_SetRamp(1, 4);


    //----- TIM Periph clock enable
    RCC_APB1PeriphClockCmd(STPDRV_TIM_APB , ENABLE);

    //--- TIMx Configuration: Output Compare Timing Mode -----------------------
    // TIM3 input clock (TIM3CLK) is set to 2 * APB1 clock (PCLK1), since APB1 prescaler is different from 1.
    //	TIM3CLK = 2 * PCLK1
    //	PCLK1 = HCLK / 4
    //	=> TIM3CLK = 2 * (HCLK / 4) = HCLK / 2 = SystemCoreClock / 2
    //
    // Para um "TIMx counter clock" de 100KHz (exemplo com resolução de 10uS) o "Prescaler" deve ser calculado assim:
    // Prescaler = (uint16_t) (TIMxCLK / 100000) - 1;
    //
    //	O "Period" neste caso não interessa pois a IRQ (TIMx_IRQHandler) dos CCR (capture/compare register) controla o
    // togles dos estados HiGH e LOW dos PINs do STEP
    //
    // A resolução do sistema é definida em "stm32f_stpdrv.h" no define STPDRV_TIMFREQ
    //
    TIM_TimeBaseStructure.TIM_Period = 65535;
    TIM_TimeBaseStructure.TIM_Prescaler = (uint16_t) ((SystemCoreClock / 2) / (STPDRV_TIMFREQ * 2)) - 1;
    TIM_TimeBaseStructure.TIM_ClockDivision = TIM_CKD_DIV1;
    TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up;
    TIM_TimeBaseStructure.TIM_RepetitionCounter = 0x0000;
    TIM_TimeBaseInit(STPDRV_TIM, &TIM_TimeBaseStructure);
    TIM_UpdateDisableConfig(STPDRV_TIM, ENABLE);  // deve ser ENABLE pois para desactiver o Update Event o bit deve ser 1
    /* Prescaler configuration */
    //TIM_PrescalerConfig(STPDRV_TIM, PrescalerValue, TIM_PSCReloadMode_Immediate);

    // Output Compare Toggle Mode configuration: All Channels
    TIM_OCStructInit(&TIM_OCInitStructure);
    TIM_OCInitStructure.TIM_OCMode = TIM_OCMode_Timing; //TIM_OCMode_Toggle
    TIM_OCInitStructure.TIM_OutputState = TIM_OutputState_Disable; // Disable por defeito, API liga quando necessario
    TIM_OCInitStructure.TIM_Pulse = 0;
    TIM_OCInitStructure.TIM_OCPolarity = TIM_OCPolarity_High;

    // Output Compare Toggle Mode configuration: Channel1 - MOTOR 1
    TIM_OC1Init(STPDRV_TIM, &TIM_OCInitStructure);
    TIM_OC1PreloadConfig(STPDRV_TIM, TIM_OCPreload_Disable);

    // Output Compare Toggle Mode configuration: Channel2 - MOTOR 2
    TIM_OC2Init(STPDRV_TIM, &TIM_OCInitStructure);
    TIM_OC2PreloadConfig(STPDRV_TIM, TIM_OCPreload_Disable);

    // Output Compare Toggle Mode configuration: Channel3 - MOTOR 1 ACELL e DECEL
    TIM_OC3Init(STPDRV_TIM, &TIM_OCInitStructure);
    TIM_OC3PreloadConfig(STPDRV_TIM, TIM_OCPreload_Disable);

    // Output Compare Toggle Mode configuration: Channel4 - MOTOR 2 ACELL e DECEL
    TIM_OC4Init(STPDRV_TIM, &TIM_OCInitStructure);
    TIM_OC4PreloadConfig(STPDRV_TIM, TIM_OCPreload_Disable);


    // Enable the TIM gloabal Interrupt
    NVIC_InitStructure.NVIC_IRQChannel = TIM3_IRQn;   // USER EDIT - Mudar o nome do IRQ se o TIMER for alterado
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = IRQ_STPDRV_PrePriority;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = IRQ_STPDRV_Priority;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);

    // TIM INT enable
    // A interrupt é activada na função Motor_On()
    //TIM_ITConfig(STPDRV_TIM, TIM_IT_CC1 | TIM_IT_CC2 | TIM_IT_CC3 | TIM_IT_CC4, ENABLE);

    // TIM enable counter
    TIM_Cmd(STPDRV_TIM, ENABLE);
}
//==============================================================================

//==============================================================================
//	descri:   Função para o timer do STEP dos MOTORES
// USER EDIT - Mudar o nome do IRQ se o TIMER for alterado 
void TIM3_IRQHandler(void)
{
    // Channel 1 -  MOTOR 1
    if ((STPDRV_TIM->SR & TIM_IT_CC1) && (STPDRV_TIM->DIER & TIM_IT_CC1)) {
        STPDRV_TIM->CCR1 += Motors[0].CurDelay;
        if (MOTOR1_STEP_PORT->IDR & MOTOR1_STEP_PIN) {
            //MOTOR1_STEP_PORT->BSRRH = MOTOR1_STEP_PIN;
            MOTOR1_STEP_PORT->BRR = MOTOR1_STEP_PIN;
#ifdef __STM32F4_DISCOVERY_H
            STM32F4_Discovery_LEDOff(LED3);
#endif			
        } else {
            //MOTOR1_STEP_PORT->BSRRL = MOTOR1_STEP_PIN;
            MOTOR1_STEP_PORT->BSRR = MOTOR1_STEP_PIN;
#ifdef __STM32F4_DISCOVERY_H
            STM32F4_Discovery_LEDOn(LED3);
#endif			
            if (Motors[0].Dir == dir_CW)
                Motors[0].Pos++;
            else
                Motors[0].Pos--;
        }
        STPDRV_TIM->SR = ~TIM_IT_CC1;
    }

    // Channel 2 -  MOTOR 2
    if ((STPDRV_TIM->SR & TIM_IT_CC2) && (STPDRV_TIM->DIER & TIM_IT_CC2)) {
        STPDRV_TIM->CCR2 += Motors[1].CurDelay;
        if (MOTOR2_STEP_PORT->IDR & MOTOR2_STEP_PIN) {
            //MOTOR2_STEP_PORT->BSRRH = MOTOR2_STEP_PIN;
            MOTOR2_STEP_PORT->BRR = MOTOR1_STEP_PIN;
        } else {
            //MOTOR2_STEP_PORT->BSRRL = MOTOR2_STEP_PIN;
            MOTOR2_STEP_PORT->BSRR = MOTOR2_STEP_PIN;
            if (Motors[1].Dir == dir_CW)
                Motors[1].Pos++;
            else
                Motors[1].Pos--;
        }
        STPDRV_TIM->SR = ~TIM_IT_CC2;
    }

    // Channel 3 -  MOTOR 1 ACELL / DECCEL
    if ((STPDRV_TIM->SR & TIM_IT_CC3) && (STPDRV_TIM->DIER & TIM_IT_CC3)) {
        STPDRV_TIM->CCR3 += Motors[0].RampDelay;
        __OnRampTimer(0);
        STPDRV_TIM->SR = ~TIM_IT_CC3;
    }

    // Channel 4 -  MOTOR 2 ACELL / DECCEL
    if ((STPDRV_TIM->SR & TIM_IT_CC4) && (STPDRV_TIM->DIER & TIM_IT_CC4)) {
        STPDRV_TIM->CCR4 += Motors[1].RampDelay;
        __OnRampTimer(1);
        STPDRV_TIM->SR = ~TIM_IT_CC4;
    }
}
//==============================================================================

//==============================================================================
//
void STPDRV_SetRamp(int16_t motor, int16_t rampspeed)
{
    Motors[motor].RampSlop  = 2;  // este valor pode ser alterado se necessário, experimentar o motor em questão
    // um numero maior faz com que a rampa seja mais brusca

    Motors[motor].RampDelay = (STPDRV_TIMFREQ * 2) / (rampspeed / Motors[motor].RampSlop);
}
//==============================================================================

//==============================================================================
//
void STPDRV_Move(int16_t motor, mdir_t direction, int16_t speed)
{
    __SetTargetSpeed(motor, speed, direction, mstat_Move);
}
//==============================================================================

//==============================================================================
//
void STPDRV_Goto(int16_t motor, int32_t position, int16_t speed, mdir_t movedir)
{
    // em implementação ... contactar o autor
}
//==============================================================================

//==============================================================================
//
void TPDRV_Stop(int16_t motor, int16_t hardstop)
{
    if (hardstop)
        __SetTargetSpeed(motor, STPDRV_MINSETPSEC, Motors[motor].Dir, mstat_Stop);
    else {
        __ResetTargetSpeed(motor);
        __MotorOff(motor);
        Motors[motor].State = mstat_Stop;
    }
}
//==============================================================================

//==============================================================================
//
int32_t 	STPDRV_GetPos(int16_t motor)
{
    return Motors[motor].Pos;
}
//==============================================================================

//==============================================================================
//
mdir_t  	STPDRV_GetDir(int16_t motor)
{
    return Motors[motor].Dir;
}
//==============================================================================

//==============================================================================
//
mstate_t	STPDRV_GetState(int16_t motor)
{
    return Motors[motor].State;
}
//==============================================================================

//==============================================================================
//
static void __MotorOff(int16_t mt)
{
    if (mt == (int16_t) 0x0)
        STPDRV_TIM->DIER &= ~TIM_IT_CC1;
    else
        STPDRV_TIM->DIER &= ~TIM_IT_CC2;
    Motors[mt].CurDelay	= 0xFFFF;

    // USER EDIT - Add your stepper IC disable command here
}
//
static void __MotorOn(int16_t mt)
{
    if (mt == (int16_t) 0x0) {
        if ((STPDRV_TIM->DIER & TIM_IT_CC1) == (uint16_t) 0x0) {
            STPDRV_TIM->CCR1  = STPDRV_TIM->CNT + Motors[mt].CurDelay;
            STPDRV_TIM->DIER |= TIM_IT_CC1;
            // A proxima linha força um IRQ se for necessário um arranque imediato, depende em parte do IC do driver usado.
            //STPDRV_TIM->EGR	= TIM_EGR_CC1G;

            // USER EDIT - Add your stepper IC enable command here
        }
    } else if ((STPDRV_TIM->DIER & TIM_IT_CC2) == (uint16_t) 0x0) {
        STPDRV_TIM->CCR2  = STPDRV_TIM->CNT + Motors[mt].CurDelay;
        STPDRV_TIM->DIER |= TIM_IT_CC2;
        // A proxima linha força um IRQ se for necessário um arranque imediato, depende em parte do IC do driver usado.
        //STPDRV_TIM->EGR	= TIM_EGR_CC2G;

        // USER EDIT - Add your stepper IC enable command here
    }
}
//==============================================================================

//==============================================================================
//
static void __MotorSetDir(int16_t mt, mdir_t _dir)
{
    if (_dir == dir_CCW) {
        if (mt == (int16_t) 0x0)
            //MOTOR1_DIR_PORT->BSRRH = MOTOR1_DIR_PIN;
            MOTOR1_DIR_PORT->BRR = MOTOR1_DIR_PIN;
        else
            //MOTOR2_DIR_PORT->BSRRH = MOTOR2_DIR_PIN;
            MOTOR2_DIR_PORT->BRR = MOTOR2_DIR_PIN;
    } else {
        if (mt == (int16_t) 0x0)
            //MOTOR1_DIR_PORT->BSRRL = MOTOR1_DIR_PIN;
            MOTOR1_DIR_PORT->BSRR = MOTOR1_DIR_PIN;
        else
            //MOTOR2_DIR_PORT->BSRRL = MOTOR2_DIR_PIN;
            MOTOR2_DIR_PORT->BSRR = MOTOR2_DIR_PIN;
    }
    Motors[mt].Dir = _dir;
}
//==============================================================================

//==============================================================================
//
static void __ResetTargetSpeed(int16_t mt)
{
    if (mt == (int16_t) 0x0)
        STPDRV_TIM->DIER &= ~TIM_IT_CC3;
    else
        STPDRV_TIM->DIER &= ~TIM_IT_CC4;
    Motors[mt].TargetSpeed		= 0;
    Motors[mt].TargetSpeed2		= 0;
    Motors[mt].TargetCurSpeed	= 0;
    Motors[mt].TargetState		= mstat_Stop;
}
//==============================================================================

//==============================================================================
// 
static void __TargetSpeedDone(int16_t mt)
{
    if (Motors[mt].TargetSpeed2 > 0) {
        __MotorSetDir(mt, Motors[mt].Dir == dir_CW ? dir_CCW : dir_CW);
        Motors[mt].TargetSpeed = Motors[mt].TargetSpeed2;
        Motors[mt].TargetSpeed2 = 0;
    } else {
        Motors[mt].State = Motors[mt].TargetState;
        __ResetTargetSpeed(mt);

        if ((Motors[mt].State == mstat_Stop) || (Motors[mt].CurDelay == 0))
            __MotorOff(mt);
    }
}
//=============================================================================

//==============================================================================
//	descri:  Inicializa as variáveis de controle para acell/decell do motor e liga 
//				os respectivos timers		
//	params:	mt - motor a acell/deacell
//          speed - target em steps/sec
//          dir - direcção
//          state - estado final desejado
//	return:	nada
//
static void __SetTargetSpeed(int16_t mt, uint16_t _speed, mdir_t _dir, mstate_t _state)
{
    if ((_speed < STPDRV_MINSETPSEC) || (_speed > STPDRV_MAXSETPSEC))
        return;

    // para evitar multiplas reentradas
    if ((Motors[mt].TargetSpeed==_speed) && (Motors[mt].TargetState==_state) && (Motors[mt].Dir==_dir))
        return;

    // se direcção actual for diferente então parar o motor primeiro, TargetSpeed2 será executado depois
    if (Motors[mt].Dir != _dir) {
        Motors[mt].TargetSpeed 	= STPDRV_MINSETPSEC;  // para evitar o stall do motor
        Motors[mt].TargetSpeed2	= _speed;
    } else {
        __MotorSetDir(mt, _dir);
        Motors[mt].TargetSpeed 	= _speed;
        Motors[mt].TargetSpeed2	= 0;
    }

    Motors[mt].TargetState = _state;
    Motors[mt].TargetCurSpeed = (STPDRV_TIMFREQ / Motors[mt].CurDelay) + 1;  	// dá os steps/sec actuais

    __MotorOn(mt);
    if (mt == (int16_t) 0x0) {
        if ((STPDRV_TIM->DIER & TIM_IT_CC3) == (uint16_t) 0x0) {		// só se estiver mesmo desligado
            STPDRV_TIM->DIER |= TIM_IT_CC3;
            STPDRV_TIM->CCR3 	= STPDRV_TIM->CNT; 	// + RampDelay
            STPDRV_TIM->EGR 	= TIM_EGR_CC3G;  		// forçar um Interrupt
        }
    } else {
        if ((STPDRV_TIM->DIER & TIM_IT_CC4) == (uint16_t) 0x0) {		// só se estiver mesmo desligado
            STPDRV_TIM->DIER |= TIM_IT_CC4;
            STPDRV_TIM->CCR4  = STPDRV_TIM->CNT;	// + RampDelay;
            STPDRV_TIM->EGR 	= TIM_EGR_CC4G;  		// forçar um Interrupt
        }
    }
}
//==============================================================================

//==============================================================================
//
static uint32_t __GPIO2AHB1Periph(GPIO_TypeDef *_qual)
{
    uint32_t ret = (uint32_t) 0x00;

    if (_qual == GPIOA)
        //ret = RCC_AHB1Periph_GPIOA;
        ret = RCC_APB2Periph_GPIOA;
    else if (_qual == GPIOB)
        //ret = RCC_AHB1Periph_GPIOB;
        ret = RCC_APB2Periph_GPIOB;
    else if (_qual == GPIOC)
        //ret = RCC_AHB1Periph_GPIOC;
        ret = RCC_APB2Periph_GPIOC;
    else if (_qual == GPIOD)
        //ret = RCC_AHB1Periph_GPIOD;
        ret = RCC_APB2Periph_GPIOD;
    else if (_qual == GPIOE)
        //ret = RCC_AHB1Periph_GPIOE;
        ret = RCC_APB2Periph_GPIOE;
    else if (_qual == GPIOF)
        //ret = RCC_AHB1Periph_GPIOF;
        ret = RCC_APB2Periph_GPIOA;
#if defined(STM32F2XX) || defined(STM32F40XX) || defined(STM32F427X) || defined (STM32F429X)
    else if (_qual == GPIOG)
        ret = RCC_AHB1Periph_GPIOG;
    else if (_qual == GPIOH)
        ret = RCC_AHB1Periph_GPIOH;
    else if (_qual == GPIOI)
        ret = RCC_AHB1Periph_GPIOI;
#endif	
    return ret;
}
//==============================================================================

//==============================================================================
//
static void __OnRampTimer(int16_t mt)
{
    if (Motors[mt].TargetSpeed > Motors[mt].TargetCurSpeed) {
        // Acell fase
        Motors[mt].TargetCurSpeed += Motors[mt].RampSlop;
        if (Motors[mt].TargetCurSpeed > Motors[mt].TargetSpeed)
            Motors[mt].TargetCurSpeed = Motors[mt].TargetSpeed;
        Motors[mt].CurDelay = STPDRV_TIMFREQ / Motors[mt].TargetCurSpeed;
    } else if (Motors[mt].TargetSpeed < Motors[mt].TargetCurSpeed) {
        // Decel fase
        Motors[mt].TargetCurSpeed -= Motors[mt].RampSlop;
        if (Motors[mt].TargetCurSpeed < Motors[mt].TargetSpeed)
            Motors[mt].TargetCurSpeed = Motors[mt].TargetSpeed;
        Motors[mt].CurDelay = STPDRV_TIMFREQ / Motors[mt].TargetCurSpeed;
    } else
        __TargetSpeedDone(mt);
}
//==============================================================================

//=============================================================================
// EOF stm32f_stpdrv.c
