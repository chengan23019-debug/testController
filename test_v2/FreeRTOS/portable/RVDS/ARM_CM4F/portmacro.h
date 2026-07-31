/*
 * FreeRTOS Kernel V10.4.6
 *
 * Header file portmacro.h for ARM Cortex-M4F Keil/RVDS Compiler
 */

#ifndef PORTMACRO_H
#define PORTMACRO_H

#ifdef __cplusplus
extern "C" {
#endif

/* Type definitions. */
#define portCHAR		char
#define portFLOAT		float
#define portDOUBLE		double
#define portLONG		long
#define portSHORT		short
#define portSTACK_TYPE	uint32_t
#define portBASE_TYPE	long

typedef portSTACK_TYPE StackType_t;
typedef long BaseType_t;
typedef unsigned long UBaseType_t;

#if( configUSE_16_BIT_TICKS == 1 )
	typedef uint16_t TickType_t;
	#define portMAX_DELAY ( TickType_t ) 0xffff
#else
	typedef uint32_t TickType_t;
	#define portMAX_DELAY ( TickType_t ) 0xffffffffUL
	#define portTICK_TYPE_IS_ATOMIC 1
#endif

/* Architecture specifics. */
#define portSTACK_GROWTH			( -1 )
#define portTICK_PERIOD_MS			( ( TickType_t ) 1000 / configTICK_RATE_HZ )
#define portBYTE_ALIGNMENT			8
#define portBYTE_ALIGNMENT_MASK		( 0x0007 )
#define portDONT_DISCARD            __attribute__((used))

#define portNVIC_INT_CTRL_REG		( * ( ( volatile uint32_t * ) 0xe000ed04 ) )
#define portVECTACTIVE_MASK			( 0xFFUL )

/* Scheduler utilities. */
#define portYIELD()					\
{									\
	/* Set a PendSV to request a context switch. */ \
	*( ( volatile uint32_t * ) 0xe000ed04 ) = ( 1UL << 28UL ); \
	__dsb( 15 );					\
	__isb( 15 );					\
}

#define portEND_SWITCHING_ISR( xSwitchRequired ) if( xSwitchRequired != pdFALSE ) portYIELD()
#define portYIELD_FROM_ISR( x ) portEND_SWITCHING_ISR( x )

/* Critical section management. */
extern void vPortEnterCritical( void );
extern void vPortExitCritical( void );

#define portDISABLE_INTERRUPTS()				vPortRaiseBASEPRI()
#define portENABLE_INTERRUPTS()					vPortSetBASEPRI( 0 )
#define portENTER_CRITICAL()					vPortEnterCritical()
#define portEXIT_CRITICAL()						vPortExitCritical()
#define portSET_INTERRUPT_MASK_FROM_ISR()		ulPortRaiseBASEPRI()
#define portCLEAR_INTERRUPT_MASK_FROM_ISR(x)	vPortSetBASEPRI(x)

/* Inline assembly helpers for BASEPRI. */
__attribute__( ( always_inline ) ) static __inline void vPortRaiseBASEPRI( void )
{
	uint32_t ulNewBASEPRI = configMAX_SYSCALL_INTERRUPT_PRIORITY;
	__asm
	{
		msr basepri, ulNewBASEPRI
		dsb
		isb
	}
}

__attribute__( ( always_inline ) ) static __inline uint32_t ulPortRaiseBASEPRI( void )
{
	uint32_t ulOriginalBASEPRI, ulNewBASEPRI = configMAX_SYSCALL_INTERRUPT_PRIORITY;
	__asm
	{
		mrs ulOriginalBASEPRI, basepri
		msr basepri, ulNewBASEPRI
		dsb
		isb
	}
	return ulOriginalBASEPRI;
}

__attribute__( ( always_inline ) ) static __inline void vPortSetBASEPRI( uint32_t ulNewMaskValue )
{
	__asm
	{
		msr basepri, ulNewMaskValue
	}
}

/* Task function macros as required by several ports. */
#define portTASK_FUNCTION_PROTO( vFunction, pvParameters ) void vFunction( void *pvParameters )
#define portTASK_FUNCTION( vFunction, pvParameters ) void vFunction( void *pvParameters )

#define portNOP()

#define portMEMORY_BARRIER() __asm volatile ( "" ::: "memory" )

#ifdef __cplusplus
}
#endif

#endif /* PORTMACRO_H */
