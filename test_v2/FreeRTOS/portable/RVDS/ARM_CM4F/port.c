/*
 * FreeRTOS Kernel V10.4.6
 *
 * Source file port.c for ARM Cortex-M4F Keil/RVDS Compiler
 */

#include "FreeRTOS.h"
#include "task.h"

/* Constants required to manipulate the core. */
#define portNVIC_SYSTICK_CTRL_REG			( * ( ( volatile uint32_t * ) 0xe000e010 ) )
#define portNVIC_SYSTICK_LOAD_REG			( * ( ( volatile uint32_t * ) 0xe000e014 ) )
#define portNVIC_SYSTICK_CURRENT_VALUE_REG	( * ( ( volatile uint32_t * ) 0xe000e018 ) )
#define portNVIC_SHPR3_REG					( * ( ( volatile uint32_t * ) 0xe000ed20 ) )
#define portNVIC_SYSTICK_CLK_BIT			( 1UL << 2UL )
#define portNVIC_SYSTICK_INT_BIT			( 1UL << 1UL )
#define portNVIC_SYSTICK_ENABLE_BIT			( 1UL << 0UL )
#define portNVIC_PENDSV_PRI					( ( ( uint32_t ) configKERNEL_INTERRUPT_PRIORITY ) << 16UL )
#define portNVIC_SYSTICK_PRI				( ( ( uint32_t ) configKERNEL_INTERRUPT_PRIORITY ) << 24UL )

#define portFPCCR							( * ( ( volatile uint32_t * ) 0xe000ef34 ) )
#define portASPEN_AND_LSPEN_BITS			( 0x3UL << 30UL )

#define portINITIAL_XPSR					( 0x01000000 )
#define portINITIAL_EXC_RETURN				( 0xfffffffdUL )

static UBaseType_t uxCriticalNesting = 0xaaaaaaaa;

/*
 * Setup the timer to generate the tick interrupt.
 */
void vPortSetupTimerInterrupt( void )
{
	/* Stop and reset SysTick. */
	portNVIC_SYSTICK_CTRL_REG = 0UL;
	portNVIC_SYSTICK_CURRENT_VALUE_REG = 0UL;

	/* Configure SysTick to interrupt at requested rate. */
	portNVIC_SYSTICK_LOAD_REG = ( configCPU_CLOCK_HZ / configTICK_RATE_HZ ) - 1UL;
	portNVIC_SYSTICK_CTRL_REG = ( portNVIC_SYSTICK_CLK_BIT | portNVIC_SYSTICK_INT_BIT | portNVIC_SYSTICK_ENABLE_BIT );
}

/*
 * Initialise the stack of a task to look exactly as if a call to
 * portSAVE_CONTEXT() had been made.
 */
StackType_t *pxPortInitialiseStack( StackType_t *pxTopOfStack, TaskFunction_t pxCode, void *pvParameters )
{
	pxTopOfStack--;
	*pxTopOfStack = portINITIAL_XPSR;	/* xPSR */
	pxTopOfStack--;
	*pxTopOfStack = ( StackType_t ) pxCode;	/* PC */
	pxTopOfStack--;
	*pxTopOfStack = ( StackType_t ) 0;	/* LR */

	pxTopOfStack -= 5;	/* R12, R3, R2, R1 */
	*pxTopOfStack = ( StackType_t ) pvParameters;	/* R0 */

	pxTopOfStack--;
	*pxTopOfStack = portINITIAL_EXC_RETURN;	/* EXC_RETURN */

	pxTopOfStack -= 8;	/* R11, R10, R9, R8, R7, R6, R5, R4 */

	return pxTopOfStack;
}

__asm void vPortEnableVFP( void )
{
	PRESERVE8

	ldr r0, =0xE000ED88
	ldr r1, [r0]
	orr r1, r1, #(0xF << 20)
	str r1, [r0]
	bx lr
}

__asm void vPortStartFirstTask( void )
{
	PRESERVE8

	ldr r0, =0xE000ED08
	ldr r0, [r0]
	ldr r0, [r0]
	msr msp, r0
	cpsie i
	cpsie f
	dsb
	isb
	svc 0
	nop
	nop
}

BaseType_t xPortStartScheduler( void )
{
	portNVIC_SHPR3_REG |= portNVIC_PENDSV_PRI;
	portNVIC_SHPR3_REG |= portNVIC_SYSTICK_PRI;

	/* Enable FPU hardware coprocessor. */
	portFPCCR |= portASPEN_AND_LSPEN_BITS;
	vPortEnableVFP();

	/* Initialise critical section nesting variable. */
	uxCriticalNesting = 0;

	/* Start timer for tick interrupts. */
	vPortSetupTimerInterrupt();

	/* Start the first task. */
	vPortStartFirstTask();

	return 0;
}

void vPortEndScheduler( void )
{
	configASSERT( uxCriticalNesting == 1000UL );
}

void vPortEnterCritical( void )
{
	portDISABLE_INTERRUPTS();
	uxCriticalNesting++;
	if( uxCriticalNesting == 1 )
	{
		configASSERT( ( portNVIC_INT_CTRL_REG & portVECTACTIVE_MASK ) == 0 );
	}
}

void vPortExitCritical( void )
{
	configASSERT( uxCriticalNesting );
	uxCriticalNesting--;
	if( uxCriticalNesting == 0 )
	{
		portENABLE_INTERRUPTS();
	}
}

__asm void vPortSVCHandler( void )
{
	PRESERVE8

	ldr r3, =pxCurrentTCB
	ldr r1, [r3]
	ldr r0, [r1]
	ldmia r0!, {r4-r11, r14}
	msr psp, r0
	isb
	mov r0, #0
	msr basepri, r0
	bx r14
}

__asm void xPortPendSVHandler( void )
{
	extern vTaskSwitchContext
	extern pxCurrentTCB

	PRESERVE8

	mrs r0, psp
	isb

	ldr r3, =pxCurrentTCB
	ldr r2, [r3]

	tst r14, #0x10
	it eq
	vstmdbeq r0!, {s16-s31}

	stmdb r0!, {r4-r11, r14}
	str r0, [r2]

	stmdb sp!, {r3}
	mov r0, #configMAX_SYSCALL_INTERRUPT_PRIORITY
	msr basepri, r0
	dsb
	isb
	bl vTaskSwitchContext
	mov r0, #0
	msr basepri, r0
	ldmia sp!, {r3}

	ldr r1, [r3]
	ldr r0, [r1]
	ldmia r0!, {r4-r11, r14}

	tst r14, #0x10
	it eq
	vldmiaeq r0!, {s16-s31}

	msr psp, r0
	isb
	bx r14
}
