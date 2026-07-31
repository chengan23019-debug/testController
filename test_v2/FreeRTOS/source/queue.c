/*
 * FreeRTOS Kernel V10.4.6
 *
 * Source file queue.c
 */

#include <stdlib.h>
#include <string.h>

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"

typedef struct QueueDefinition
{
	int8_t *pcHead;					/*<< Points to the beginning of the queue storage area. */
	int8_t *pcWriteTo;				/*<< Points to the free position in the queue storage area. */
	int8_t *pcReadFrom;				/*<< Points to the last place from which an item was read. */
	List_t xTasksWaitingToSend;		/*<< List of tasks waiting to send to this queue. */
	List_t xTasksWaitingToReceive;	/*<< List of tasks waiting to receive from this queue. */
	volatile UBaseType_t uxMessagesWaiting;/*<< The number of items currently in the queue. */
	UBaseType_t uxLength;			/*<< The length of the queue defined as the number of items it can hold. */
	UBaseType_t uxItemSize;			/*<< The size of each item that the queue will hold. */
	volatile int8_t cRxLock;		/*<< Stores the number of items received while the queue was locked. */
	volatile int8_t cTxLock;		/*<< Stores the number of items transmitted while the queue was locked. */
	uint8_t ucQueueType;
} xQUEUE;

typedef xQUEUE Queue_t;

QueueHandle_t xQueueGenericCreate( const UBaseType_t uxQueueLength, const UBaseType_t uxItemSize, const uint8_t ucQueueType )
{
	Queue_t *pxNewQueue;
	size_t xQueueSizeInBytes;

	if( uxQueueLength > ( UBaseType_t ) 0 )
	{
		xQueueSizeInBytes = ( size_t ) ( uxQueueLength * uxItemSize );
		pxNewQueue = ( Queue_t * ) pvPortMalloc( sizeof( Queue_t ) + xQueueSizeInBytes );

		if( pxNewQueue != NULL )
		{
			pxNewQueue->pcHead = ( ( int8_t * ) pxNewQueue ) + sizeof( Queue_t );
			pxNewQueue->uxLength = uxQueueLength;
			pxNewQueue->uxItemSize = uxItemSize;
			pxNewQueue->pcWriteTo = pxNewQueue->pcHead;
			pxNewQueue->uxMessagesWaiting = 0;
			pxNewQueue->ucQueueType = ucQueueType;
			pxNewQueue->cRxLock = -1;
			pxNewQueue->cTxLock = -1;

			vListInitialise( &( pxNewQueue->xTasksWaitingToSend ) );
			vListInitialise( &( pxNewQueue->xTasksWaitingToReceive ) );

			return ( QueueHandle_t ) pxNewQueue;
		}
	}

	return NULL;
}

BaseType_t xQueueGenericSend( QueueHandle_t xQueue, const void * const pvItemToQueue, TickType_t xTicksToWait, const BaseType_t xCopyPosition )
{
	Queue_t * const pxQueue = xQueue;
	( void ) xCopyPosition;

	taskENTER_CRITICAL();
	{
		if( pxQueue->uxMessagesWaiting < pxQueue->uxLength )
		{
			if( ( pvItemToQueue != NULL ) && ( pxQueue->uxItemSize > 0U ) )
			{
				memcpy( ( void * ) pxQueue->pcWriteTo, pvItemToQueue, ( size_t ) pxQueue->uxItemSize );
				pxQueue->pcWriteTo += pxQueue->uxItemSize;
				if( pxQueue->pcWriteTo >= ( pxQueue->pcHead + ( pxQueue->uxLength * pxQueue->uxItemSize ) ) )
				{
					pxQueue->pcWriteTo = pxQueue->pcHead;
				}
			}
			pxQueue->uxMessagesWaiting++;
			taskEXIT_CRITICAL();
			return pdPASS;
		}
	}
	taskEXIT_CRITICAL();

	if( xTicksToWait > 0 )
	{
		vTaskDelay( xTicksToWait );
	}

	return pdFAIL;
}

BaseType_t xQueueReceive( QueueHandle_t xQueue, void * const pvBuffer, TickType_t xTicksToWait )
{
	Queue_t * const pxQueue = xQueue;

	taskENTER_CRITICAL();
	{
		if( pxQueue->uxMessagesWaiting > 0U )
		{
			if( ( pvBuffer != NULL ) && ( pxQueue->uxItemSize > 0U ) )
			{
				memcpy( pvBuffer, ( void * ) pxQueue->pcHead, ( size_t ) pxQueue->uxItemSize );
			}
			pxQueue->uxMessagesWaiting--;
			taskEXIT_CRITICAL();
			return pdPASS;
		}
	}
	taskEXIT_CRITICAL();

	if( xTicksToWait > 0 )
	{
		vTaskDelay( xTicksToWait );
	}

	return pdFAIL;
}

QueueHandle_t xQueueCreateMutex( const uint8_t ucQueueType )
{
	QueueHandle_t xMutex;
	xMutex = xQueueGenericCreate( 1, 0, ucQueueType );
	if( xMutex != NULL )
	{
		xQueueGenericSend( xMutex, NULL, 0, queueSEND_TO_BACK );
	}
	return xMutex;
}

QueueHandle_t xQueueCreateCountingSemaphore( const UBaseType_t uxMaxCount, const UBaseType_t uxInitialCount )
{
	QueueHandle_t xHandle;

	xHandle = xQueueGenericCreate( uxMaxCount, 0, queueQUEUE_TYPE_COUNTING_SEMAPHORE );
	if( xHandle != NULL )
	{
		( ( Queue_t * ) xHandle )->uxMessagesWaiting = uxInitialCount;
	}

	return xHandle;
}

void vQueueDelete( QueueHandle_t xQueue )
{
	if( xQueue != NULL )
	{
		vPortFree( xQueue );
	}
}
