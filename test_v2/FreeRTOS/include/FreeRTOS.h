/*
 * FreeRTOS Kernel V10.4.6
 *
 * Header file FreeRTOS.h
 */

#ifndef INC_FREERTOS_H
#define INC_FREERTOS_H

#include <stddef.h>
#include <stdint.h>

/* Application specific configuration options. */
#include "FreeRTOSConfig.h"

/* Basic FreeRTOS definitions. */
#include "projdefs.h"

/* Portable layer definitions. */
#include "portable.h"

/* MPU wrappers definitions. */
#include "mpu_wrappers.h"

#ifndef configMINIMAL_STACK_SIZE
	#define configMINIMAL_STACK_SIZE ( ( uint16_t ) 128 )
#endif

#ifndef configMAX_PRIORITIES
	#define configMAX_PRIORITIES ( 5 )
#endif

#ifndef configUSE_PREEMPTION
	#define configUSE_PREEMPTION 1
#endif

#ifndef configUSE_TIME_SLICING
	#define configUSE_TIME_SLICING 1
#endif

#ifndef configUSE_MUTEXES
	#define configUSE_MUTEXES 1
#endif

#ifndef configUSE_COUNTING_SEMAPHORES
	#define configUSE_COUNTING_SEMAPHORES 1
#endif

#ifndef configUSE_RECURSIVE_MUTEXES
	#define configUSE_RECURSIVE_MUTEXES 1
#endif

#ifndef configUSE_QUEUE_SETS
	#define configUSE_QUEUE_SETS 0
#endif

#ifndef configUSE_TASK_NOTIFICATIONS
	#define configUSE_TASK_NOTIFICATIONS 1
#endif

#ifndef configTASK_NOTIFICATION_ARRAY_ENTRIES
	#define configTASK_NOTIFICATION_ARRAY_ENTRIES 1
#endif

#ifndef configMAX_TASK_NAME_LEN
	#define configMAX_TASK_NAME_LEN 16
#endif

#ifndef configLIST_VOLATILE
	#define configLIST_VOLATILE volatile
#endif

#ifndef configUSE_TIMERS
	#define configUSE_TIMERS 0
#endif

#ifndef portCRITICAL_NESTING_IN_TCB
	#define portCRITICAL_NESTING_IN_TCB 0
#endif

#ifndef traceMOVED_TASK_TO_READY_STATE
	#define traceMOVED_TASK_TO_READY_STATE( pxTCB )
#endif

#ifndef tracePOST_MOVED_TASK_TO_READY_STATE
	#define tracePOST_MOVED_TASK_TO_READY_STATE( pxTCB )
#endif

#define taskYIELD() portYIELD()

#define taskENTER_CRITICAL()			portENTER_CRITICAL()
#define taskENTER_CRITICAL_FROM_ISR()	portSET_INTERRUPT_MASK_FROM_ISR()
#define taskEXIT_CRITICAL()				portEXIT_CRITICAL()
#define taskEXIT_CRITICAL_FROM_ISR( x )	portCLEAR_INTERRUPT_MASK_FROM_ISR( x )

#define taskDISABLE_INTERRUPTS()		portDISABLE_INTERRUPTS()
#define taskENABLE_INTERRUPTS()			portENABLE_INTERRUPTS()

/* Includes list.h after defining basic types. */
#include "list.h"

#endif /* INC_FREERTOS_H */
