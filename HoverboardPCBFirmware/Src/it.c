//defines  interrupts and timers

#include "gd32f1x0.h"
#include "../Inc/it.h"
#include "../Inc/defines.h"
#include "../Inc/config.h"
#include "../Inc/bldc.h"
#include "../Inc/led.h"
#include "../Inc/commsMasterSlave.h"
#include "../Inc/commsRemote.h"
#include "../Inc/commsSteeringPWM.h"
#include "../Inc/commsInterlocks.h"
#include "../Inc/commsActuator.h"
#include "../Inc/commsBluetooth.h"
#include "../Inc/commsAccelerometer.h"

uint32_t msTicks;
uint32_t timeoutCounter_ms = 0;
FlagStatus timedOut = SET;

#ifdef SLAVE
uint32_t hornCounter_ms = 0;
#endif

extern int32_t steer;
extern int32_t speed;
extern FlagStatus activateWeakening;
extern FlagStatus beepsBackwards;

uint8_t tmpLoopCounter=0;
uint8_t tmpLoopCounter2=0;
//----------------------------------------------------------------------------
// SysTick_Handler
//----------------------------------------------------------------------------
void SysTick_Handler(void)
{
  msTicks++;
}

//----------------------------------------------------------------------------
// Resets the timeout to zero
//----------------------------------------------------------------------------
void ResetTimeout(void)
{
  timeoutCounter_ms = 0;
}

//----------------------------------------------------------------------------
// Timer13_Update_Handler
// Is called when upcounting of timer13 is finished and the UPDATE-flag is set
// -> period of timer13 running with 2khz-> interrupt every 0,5msec (Florian wrote 1kHz -> interrupt every 1ms but i think he was wrong)

//----------------------------------------------------------------------------
void TIMER13_IRQHandler(void)
{	
	if (timeoutCounter_ms > TIMEOUT_MS)
	{
		// First timeout reset all process values
		if (timedOut == RESET)
		{
#ifdef MASTER
			steer = 0;
			speed = 0;
			beepsBackwards = RESET;
#endif
#ifdef SLAVE
			SetPWM(0);
#endif
		}
		
		timedOut = SET;
	}
	else
	{
		timedOut = RESET;
		timeoutCounter_ms++;
	}

#ifdef SLAVE
	if (hornCounter_ms >= 2000)
	{
		// Avoid horn to be activated longer than 2 seconds
		SetUpperLEDMaster(RESET);
	}
	else if (hornCounter_ms < 2000)
	{
		hornCounter_ms++;
	}
	
	// Update LED program
	CalculateLEDProgram();
#endif
tmpLoopCounter++;
if (tmpLoopCounter == 200){	
	TIMER_100ms();
	tmpLoopCounter=0;
}

tmpLoopCounter2++;
if (tmpLoopCounter2 == 40){	
	TIMER_20ms();
	tmpLoopCounter2=0;
}
	
	
	// Clear timer update interrupt flag
	timer_interrupt_flag_clear(TIMER13, TIMER_INT_UP);
}

void TIMER_100ms(void){
	#ifdef MASTER
			//READ interlocks
			checkInterlockInputs();
	#endif

}

void TIMER_20ms(void){
	#ifdef MASTER
			GetAccelerometerData();
	#endif

}
//----------------------------------------------------------------------------
// Timer0_Update_Handler
// Is called when upcouting of timer0 is finished and the UPDATE-flag is set
// AND when downcouting of timer0 is finished and the UPDATE-flag is set
// -> pwm of timer0 running with 16kHz -> interrupt every 31,25us
//----------------------------------------------------------------------------
void TIMER0_BRK_UP_TRG_COM_IRQHandler(void)
{
	// Start ADC conversion
	adc_software_trigger_enable(ADC_REGULAR_CHANNEL);
	#ifdef MASTER
		// check pwm SPEED input signal
		#ifdef REMOTE_CONTROL_PWM
			CheckPWMRemoteControlInput();
		#endif
	UpdateActuatorOutput();
	#endif
	
	
	
	// Clear timer update interrupt flag
	timer_interrupt_flag_clear(TIMER_BLDC, TIMER_INT_UP);
}

//----------------------------------------------------------------------------
// This function handles DMA_Channel0_IRQHandler interrupt
// Is called, when the ADC scan sequence is finished
// -> ADC is triggered from timer0-update-interrupt -> every 31,25us
//----------------------------------------------------------------------------
void DMA_Channel0_IRQHandler(void)
{
	// Calculate motor PWMs
	CalculateBLDC();
	
	#ifdef SLAVE
	// Calculates RGB LED
	CalculateLEDPWM();
	#endif
	
	if (dma_interrupt_flag_get(DMA_CH0, DMA_INT_FLAG_FTF))
	{
		dma_interrupt_flag_clear(DMA_CH0, DMA_INT_FLAG_FTF);        
	}
}


//----------------------------------------------------------------------------
// This function handles DMA_Channel1_2_IRQHandler interrupt
// Is asynchronously called when USART0 RX finished
//----------------------------------------------------------------------------
void DMA_Channel1_2_IRQHandler(void)
{
	// USART steer/bluetooth RX
	if (dma_interrupt_flag_get(DMA_CH2, DMA_INT_FLAG_FTF))
	{
#ifdef MASTER
		// Update USART steer input mechanism
		UpdateUSART_REMOTE_MASTER_BOARD_Input();
#endif
#ifdef SLAVE
		// Update USART bluetooth input mechanism
		UpdateUSARTBluetoothInput();
#endif
		dma_interrupt_flag_clear(DMA_CH2, DMA_INT_FLAG_FTF);        
	}
}


//----------------------------------------------------------------------------
// This function handles DMA_Channel3_4_IRQHandler interrupt
// Is asynchronously called when USART_SLAVE RX finished
//----------------------------------------------------------------------------
void DMA_Channel3_4_IRQHandler(void)
{
	// USART master slave RX
	if (dma_interrupt_flag_get(DMA_CH4, DMA_INT_FLAG_FTF))
	{
		// Update USART master slave input mechanism
		UpdateUSARTMasterSlaveInput();
		
		dma_interrupt_flag_clear(DMA_CH4, DMA_INT_FLAG_FTF);        
	}
}

//----------------------------------------------------------------------------
// Returns number of milliseconds since system start
//----------------------------------------------------------------------------
uint32_t millis()
{
	return msTicks;
}

//----------------------------------------------------------------------------
// Delays number of tick Systicks (happens every 10 ms)
//----------------------------------------------------------------------------
void Delay (uint32_t dlyTicks)
{
  uint32_t curTicks;

  curTicks = msTicks;
  while ((msTicks - curTicks) < dlyTicks)
	{
		__NOP();
	}
}

//----------------------------------------------------------------------------
// This function handles Non maskable interrupt.
//----------------------------------------------------------------------------
void NMI_Handler(void)
{
}

//----------------------------------------------------------------------------
// This function handles Hard fault interrupt.
//----------------------------------------------------------------------------
void HardFault_Handler(void)
{
  while(1) {}
}

//----------------------------------------------------------------------------
// This function handles Memory management fault.
//----------------------------------------------------------------------------
void MemManage_Handler(void)
{
  while(1) {}
}

//----------------------------------------------------------------------------
// This function handles Prefetch fault, memory access fault.
//----------------------------------------------------------------------------
void BusFault_Handler(void)
{
  while(1) {}
}

//----------------------------------------------------------------------------
// This function handles Undefined instruction or illegal state.
//----------------------------------------------------------------------------
void UsageFault_Handler(void)
{
  while(1) {}
}

//----------------------------------------------------------------------------
// This function handles System service call via SWI instruction.
//----------------------------------------------------------------------------
void SVC_Handler(void)
{
}

//----------------------------------------------------------------------------
// This function handles Debug monitor.
//----------------------------------------------------------------------------
void DebugMon_Handler(void)
{
}

//----------------------------------------------------------------------------
// This function handles Pendable request for system service.
//----------------------------------------------------------------------------
void PendSV_Handler(void)
{
}
