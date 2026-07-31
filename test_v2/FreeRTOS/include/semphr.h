/*
 * FreeRTOS Kernel V10.4.6
 *
 * Header file semphr.h
 */

#ifndef SEMAPHORE_H
#define SEMAPHORE_H

#ifndef INC_FREERTOS_H
	#error "FreeRTOS.h must be included before semphr.h"
#endif

#include "queue.h"

typedef QueueHandle_t SemaphoreHandle_t;

#define semBINARY_SEMAPHORE_QUEUE_LENGTH	( ( uint8_t ) 1U )
#define semSEMAPHORE_QUEUE_ITEM_LENGTH		( ( uint8_t ) 0U )

#define vSemaphoreCreateBinary( xSemaphore )																\
	{																										\
		( xSemaphore ) = xQueueGenericCreate( ( UBaseType_t ) 1, semSEMAPHORE_QUEUE_ITEM_LENGTH, queueQUEUE_TYPE_BINARY_SEMAPHORE ); \
		if( ( xSemaphore ) != NULL )																		\
		{																									\
			( void ) xSemaphoreGive( ( xSemaphore ) );														\
		}																									\
	}

#define xSemaphoreCreateBinary() xQueueGenericCreate( ( UBaseType_t ) 1, semSEMAPHORE_QUEUE_ITEM_LENGTH, queueQUEUE_TYPE_BINARY_SEMAPHORE )

#define xSemaphoreTake( xSemaphore, xBlockTime )		xQueueReceive( ( QueueHandle_t ) ( xSemaphore ), NULL, ( xBlockTime ) )

#define xSemaphoreGive( xSemaphore )					xQueueGenericSend( ( QueueHandle_t ) ( xSemaphore ), NULL, ( TickType_t ) 0, queueSEND_TO_BACK )

#define xSemaphoreGiveFromISR( xSemaphore, pxHigherPriorityTaskWoken )	xQueueGiveFromISR( ( QueueHandle_t ) ( xSemaphore ), ( pxHigherPriorityTaskWoken ) )

#define xSemaphoreTakeFromISR( xSemaphore, pxHigherPriorityTaskWoken )	xQueueReceiveFromISR( ( QueueHandle_t ) ( xSemaphore ), NULL, ( pxHigherPriorityTaskWoken ) )

#define xSemaphoreCreateMutex()							xQueueCreateMutex( queueQUEUE_TYPE_MUTEX )

#define xSemaphoreCreateCounting( uxMaxCount, uxInitialCount ) xQueueCreateCountingSemaphore( ( uxMaxCount ), ( uxInitialCount ) )

#define xSemaphoreCreateRecursiveMutex()				xQueueCreateMutex( queueQUEUE_TYPE_RECURSIVE_MUTEX )

#define xSemaphoreTakeRecursive( xMutex, xBlockTime )	xQueueTakeMutexRecursive( ( xMutex ), ( xBlockTime ) )

#define xSemaphoreGiveRecursive( xMutex )				xQueueGiveMutexRecursive( ( xMutex ) )

#define vSemaphoreDelete( xSemaphore )					vQueueDelete( ( QueueHandle_t ) ( xSemaphore ) )

#define xSemaphoreGetMutexHolder( xSemaphore )			xQueueGetMutexHolder( ( xSemaphore ) )

#endif /* SEMAPHORE_H */
