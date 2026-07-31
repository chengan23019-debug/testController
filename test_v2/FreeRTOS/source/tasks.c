/*
 * FreeRTOS Kernel V10.4.6
 *
 * Source file tasks.c
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "FreeRTOS.h"
#include "task.h"
#include "timers.h"

/* TCB definition. */
typedef struct tskTaskControlBlock
{
	volatile StackType_t	*pxTopOfStack;	/*<< Points to the location of the last item placed on the tasks stack. */
	ListItem_t				xStateListItem;	/*<< The list that the state list item is in references the state of the task. */
	ListItem_t				xEventListItem;	/*<< Used to reference a task from an event list. */
	UBaseType_t				uxPriority;		/*<< The priority of the task. */
	StackType_t				*pxStack;		/*<< Points to the start of the stack. */
	char					pcTaskName[ configMAX_TASK_NAME_LEN ]; /*<< Descriptive name given to the task. */
	#if ( configUSE_MUTEXES == 1 )
		UBaseType_t			uxBasePriority;	/*<< The priority last assigned to the task. */
		UBaseType_t			uxMutexesHeld;
	#endif
	#if ( configUSE_TASK_NOTIFICATIONS == 1 )
		volatile uint32_t	ulNotifiedValue[ configTASK_NOTIFICATION_ARRAY_ENTRIES ];
		volatile uint8_t	ucNotifyState[ configTASK_NOTIFICATION_ARRAY_ENTRIES ];
	#endif
} TCB_t;

/* Global Task Variables */
PRIVILEGED_DATA TCB_t * volatile pxCurrentTCB = NULL;

PRIVILEGED_DATA static List_t pxReadyTasksLists[ configMAX_PRIORITIES ];
PRIVILEGED_DATA static List_t xDelayedTaskList1;
PRIVILEGED_DATA static List_t xDelayedTaskList2;
PRIVILEGED_DATA static List_t * volatile pxDelayedTaskList = &xDelayedTaskList1;
PRIVILEGED_DATA static List_t * volatile pxOverflowDelayedTaskList = &xDelayedTaskList2;
PRIVILEGED_DATA static List_t xPendingReadyList;

PRIVILEGED_DATA static volatile UBaseType_t uxCurrentNumberOfTasks = ( UBaseType_t ) 0U;
PRIVILEGED_DATA static volatile TickType_t xTickCount = ( TickType_t ) 0U;
PRIVILEGED_DATA static volatile UBaseType_t uxTopReadyPriority = tskIDLE_PRIORITY;
PRIVILEGED_DATA static volatile BaseType_t xSchedulerRunning = pdFALSE;
PRIVILEGED_DATA static volatile UBaseType_t uxPendedTicks = ( UBaseType_t ) 0U;
PRIVILEGED_DATA static volatile BaseType_t xYieldPending = pdFALSE;
PRIVILEGED_DATA static volatile BaseType_t xNumOfOverflows = ( BaseType_t ) 0;
PRIVILEGED_DATA static volatile TickType_t xNextTaskUnblockTime = ( TickType_t ) 0U;
PRIVILEGED_DATA static volatile UBaseType_t uxSchedulerSuspended = ( UBaseType_t ) pdFALSE;
PRIVILEGED_DATA static TaskHandle_t xIdleTaskHandle = NULL;

/* Macro Definitions */
#define prvAddTaskToReadyList( pxTCB )																\
	traceMOVED_TASK_TO_READY_STATE( pxTCB );														\
	taskRECORD_READY_PRIORITY( ( pxTCB )->uxPriority );												\
	vListInsertEnd( &( pxReadyTasksLists[ ( pxTCB )->uxPriority ] ), &( ( pxTCB )->xStateListItem ) );

#define taskRECORD_READY_PRIORITY( uxPriority )														\
	if( ( uxPriority ) > uxTopReadyPriority )														\
	{																								\
		uxTopReadyPriority = ( uxPriority );														\
	}

#define taskRESET_READY_PRIORITY( uxPriority )

#define prvGetTCBFromHandle( pxHandle ) ( ( ( pxHandle ) == NULL ) ? pxCurrentTCB : ( pxHandle ) )

static void prvInitialiseTaskLists( void )
{
	UBaseType_t uxPriority;

	for( uxPriority = ( UBaseType_t ) 0U; uxPriority < ( UBaseType_t ) configMAX_PRIORITIES; uxPriority++ )
	{
		vListInitialise( &( pxReadyTasksLists[ uxPriority ] ) );
	}

	vListInitialise( &xDelayedTaskList1 );
	vListInitialise( &xDelayedTaskList2 );
	vListInitialise( &xPendingReadyList );

	pxDelayedTaskList = &xDelayedTaskList1;
	pxOverflowDelayedTaskList = &xDelayedTaskList2;
}

static void prvAddNewTaskToReadyList( TCB_t *pxNewTCB )
{
	taskENTER_CRITICAL();
	{
		uxCurrentNumberOfTasks++;
		if( pxCurrentTCB == NULL )
		{
			pxCurrentTCB = pxNewTCB;

			if( uxCurrentNumberOfTasks == ( UBaseType_t ) 1 )
			{
				prvInitialiseTaskLists();
			}
		}
		else
		{
			if( xSchedulerRunning == pdFALSE )
			{
				if( pxCurrentTCB->uxPriority <= pxNewTCB->uxPriority )
				{
					pxCurrentTCB = pxNewTCB;
				}
			}
		}

		prvAddTaskToReadyList( pxNewTCB );
	}
	taskEXIT_CRITICAL();

	if( xSchedulerRunning != pdFALSE )
	{
		if( pxCurrentTCB->uxPriority < pxNewTCB->uxPriority )
		{
			taskYIELD();
		}
	}
}

BaseType_t xTaskCreate(	TaskFunction_t pxTaskCode,
						const char * const pcName,
						const uint16_t usStackDepth,
						void * const pvParameters,
						UBaseType_t uxPriority,
						TaskHandle_t * const pxCreatedTask )
{
	TCB_t *pxNewTCB;
	StackType_t *pxStack;
	BaseType_t xReturn = errCOULD_NOT_ALLOCATE_REQUIRED_MEMORY;

	if( uxPriority >= ( UBaseType_t ) configMAX_PRIORITIES )
	{
		uxPriority = ( UBaseType_t ) configMAX_PRIORITIES - ( UBaseType_t ) 1U;
	}

	pxStack = ( StackType_t * ) pvPortMalloc( ( ( size_t ) usStackDepth ) * sizeof( StackType_t ) );

	if( pxStack != NULL )
	{
		pxNewTCB = ( TCB_t * ) pvPortMalloc( sizeof( TCB_t ) );

		if( pxNewTCB != NULL )
		{
			memset( pxNewTCB, 0, sizeof( TCB_t ) );
			pxNewTCB->pxStack = pxStack;

			/* Store task name. */
			if( pcName != NULL )
			{
				strncpy( pxNewTCB->pcTaskName, pcName, ( size_t ) configMAX_TASK_NAME_LEN - 1U );
				pxNewTCB->pcTaskName[ configMAX_TASK_NAME_LEN - 1U ] = '\0';
			}

			pxNewTCB->uxPriority = uxPriority;
			#if ( configUSE_MUTEXES == 1 )
			{
				pxNewTCB->uxBasePriority = uxPriority;
			}
			#endif

			vListInitialiseItem( &( pxNewTCB->xStateListItem ) );
			vListInitialiseItem( &( pxNewTCB->xEventListItem ) );

			listSET_LIST_ITEM_OWNER( &( pxNewTCB->xStateListItem ), pxNewTCB );
			listSET_LIST_ITEM_OWNER( &( pxNewTCB->xEventListItem ), pxNewTCB );

			listSET_LIST_ITEM_VALUE( &( pxNewTCB->xEventListItem ), ( TickType_t ) configMAX_PRIORITIES - ( TickType_t ) uxPriority );

			pxNewTCB->pxTopOfStack = pxPortInitialiseStack( pxStack + usStackDepth, pxTaskCode, pvParameters );

			if( pxCreatedTask != NULL )
			{
				*pxCreatedTask = ( TaskHandle_t ) pxNewTCB;
			}

			prvAddNewTaskToReadyList( pxNewTCB );
			xReturn = pdPASS;
		}
		else
		{
			vPortFree( pxStack );
		}
	}

	return xReturn;
}

static portTASK_FUNCTION( prvIdleTask, pvParameters )
{
	( void ) pvParameters;

	for( ;; )
	{
		/* Idle task loop */
		#if ( configUSE_PREEMPTION == 1 )
		{
			taskYIELD();
		}
		#endif
	}
}

void vTaskStartScheduler( void )
{
	BaseType_t xReturn;

	xReturn = xTaskCreate( prvIdleTask, "IDLE", configMINIMAL_STACK_SIZE, ( void * ) NULL, tskIDLE_PRIORITY, &xIdleTaskHandle );

	if( xReturn == pdPASS )
	{
		portDISABLE_INTERRUPTS();

		xNextTaskUnblockTime = portMAX_DELAY;
		xSchedulerRunning = pdTRUE;
		xTickCount = ( TickType_t ) 0U;

		if( xPortStartScheduler() != pdFALSE )
		{
			/* Should not reach here. */
		}
	}
}

void vTaskDelay( const TickType_t xTicksToDelay )
{
	TCB_t *pxTCB;

	if( xTicksToDelay > ( TickType_t ) 0U )
	{
		vTaskSuspendAll();
		{
			pxTCB = pxCurrentTCB;
			if( uxListRemove( &( pxTCB->xStateListItem ) ) == ( UBaseType_t ) 0 )
			{
				taskRESET_READY_PRIORITY( pxTCB->uxPriority );
			}

			listSET_LIST_ITEM_VALUE( &( pxTCB->xStateListItem ), xTickCount + xTicksToDelay );
			vListInsert( pxDelayedTaskList, &( pxTCB->xStateListItem ) );
		}
		( void ) xTaskResumeAll();
	}
}

TickType_t xTaskGetTickCount( void )
{
	TickType_t xTicks;

	taskENTER_CRITICAL();
	{
		xTicks = xTickCount;
	}
	taskEXIT_CRITICAL();

	return xTicks;
}

TickType_t xTaskGetTickCountFromISR( void )
{
	return xTickCount;
}

void vTaskIncrementTick( void )
{
	TCB_t * pxTCB;
	TickType_t xItemValue;

	if( xSchedulerRunning != pdFALSE )
	{
		const TickType_t xConstTickCount = xTickCount + ( TickType_t ) 1U;
		xTickCount = xConstTickCount;

		if( listLIST_IS_EMPTY( pxDelayedTaskList ) == pdFALSE )
		{
			for( ;; )
			{
				if( listLIST_IS_EMPTY( pxDelayedTaskList ) != pdFALSE )
				{
					break;
				}

				pxTCB = ( TCB_t * ) listGET_OWNER_OF_HEAD_ENTRY( pxDelayedTaskList );
				xItemValue = listGET_LIST_ITEM_VALUE( &( pxTCB->xStateListItem ) );

				if( xConstTickCount < xItemValue )
				{
					break;
				}

				( void ) uxListRemove( &( pxTCB->xStateListItem ) );
				if( pxTCB->xEventListItem.pxContainer != NULL )
				{
					( void ) uxListRemove( &( pxTCB->xEventListItem ) );
				}
				prvAddTaskToReadyList( pxTCB );
			}
		}
	}
}

void vTaskSwitchContext( void )
{
	if( xSchedulerRunning != pdFALSE )
	{
		while( listLIST_IS_EMPTY( &( pxReadyTasksLists[ uxTopReadyPriority ] ) ) )
		{
			--uxTopReadyPriority;
		}

		listGET_OWNER_OF_NEXT_ENTRY( pxCurrentTCB, &( pxReadyTasksLists[ uxTopReadyPriority ] ) );
	}
}

void vTaskSuspendAll( void )
{
	uxSchedulerSuspended = pdTRUE;
}

BaseType_t xTaskResumeAll( void )
{
	uxSchedulerSuspended = pdFALSE;
	return pdTRUE;
}

BaseType_t xTaskGetSchedulerState( void )
{
	BaseType_t xReturn;

	if( xSchedulerRunning == pdFALSE )
	{
		xReturn = taskSCHEDULER_NOT_STARTED;
	}
	else
	{
		if( uxSchedulerSuspended == pdFALSE )
		{
			xReturn = taskSCHEDULER_RUNNING;
		}
		else
		{
			xReturn = taskSCHEDULER_SUSPENDED;
		}
	}

	return xReturn;
}

TaskHandle_t xTaskGetCurrentTaskHandle( void )
{
	return ( TaskHandle_t ) pxCurrentTCB;
}

#if ( configUSE_TASK_NOTIFICATIONS == 1 )
BaseType_t xTaskNotifyGive( TaskHandle_t xTaskToNotify )
{
	TCB_t *pxTCB = ( TCB_t * ) xTaskToNotify;

	if( pxTCB == NULL )
	{
		pxTCB = ( TCB_t * ) pxCurrentTCB;
	}

	taskENTER_CRITICAL();
	{
		pxTCB->ulNotifiedValue[ 0 ]++;
		pxTCB->ucNotifyState[ 0 ] = 1;
	}
	taskEXIT_CRITICAL();

	return pdPASS;
}

uint32_t ulTaskNotifyTake( BaseType_t xClearCountOnExit, TickType_t xTicksToWait )
{
	uint32_t ulReturn;
	TCB_t *pxTCB = pxCurrentTCB;

	taskENTER_CRITICAL();
	{
		if( pxTCB->ulNotifiedValue[ 0 ] == 0U )
		{
			pxTCB->ucNotifyState[ 0 ] = 0;
			if( xTicksToWait > 0U )
			{
				vTaskDelay( xTicksToWait );
			}
		}

		ulReturn = pxTCB->ulNotifiedValue[ 0 ];
		if( ulReturn != 0U )
		{
			if( xClearCountOnExit != pdFALSE )
			{
				pxTCB->ulNotifiedValue[ 0 ] = 0U;
			}
			else
			{
				pxTCB->ulNotifiedValue[ 0 ]--;
			}
		}
	}
	taskEXIT_CRITICAL();

	return ulReturn;
}
#endif
