/*
 * FreeRTOS Kernel V10.4.6
 *
 * Source file event_groups.c
 */

#include "FreeRTOS.h"
#include "task.h"
#include "event_groups.h"

typedef struct EventGroupDef
{
	EventBits_t uxEventBits;
	List_t xTasksWaitingForBits;
} EventGroup_t;

EventGroupHandle_t xEventGroupCreate( void )
{
	EventGroup_t *pxEventBits;

	pxEventBits = ( EventGroup_t * ) pvPortMalloc( sizeof( EventGroup_t ) );

	if( pxEventBits != NULL )
	{
		pxEventBits->uxEventBits = 0;
		vListInitialise( &( pxEventBits->xTasksWaitingForBits ) );
	}

	return ( EventGroupHandle_t ) pxEventBits;
}

EventBits_t xEventGroupSetBits( EventGroupHandle_t xEventGroup, const EventBits_t uxBitsToSet )
{
	EventGroup_t *pxEventBits = ( EventGroup_t * ) xEventGroup;

	taskENTER_CRITICAL();
	{
		pxEventBits->uxEventBits |= uxBitsToSet;
	}
	taskEXIT_CRITICAL();

	return pxEventBits->uxEventBits;
}

EventBits_t xEventGroupClearBits( EventGroupHandle_t xEventGroup, const EventBits_t uxBitsToClear )
{
	EventGroup_t *pxEventBits = ( EventGroup_t * ) xEventGroup;

	taskENTER_CRITICAL();
	{
		pxEventBits->uxEventBits &= ~uxBitsToClear;
	}
	taskEXIT_CRITICAL();

	return pxEventBits->uxEventBits;
}

EventBits_t xEventGroupWaitBits( EventGroupHandle_t xEventGroup, const EventBits_t uxBitsToWaitFor, const BaseType_t xClearOnExit, const BaseType_t xWaitForAllBits, TickType_t xTicksToWait )
{
	EventGroup_t *pxEventBits = ( EventGroup_t * ) xEventGroup;
	EventBits_t uxReturn;
	( void ) xWaitForAllBits;

	taskENTER_CRITICAL();
	{
		uxReturn = pxEventBits->uxEventBits & uxBitsToWaitFor;
		if( ( uxReturn == uxBitsToWaitFor ) && ( xClearOnExit != pdFALSE ) )
		{
			pxEventBits->uxEventBits &= ~uxBitsToWaitFor;
		}
	}
	taskEXIT_CRITICAL();

	if( ( uxReturn != uxBitsToWaitFor ) && ( xTicksToWait > 0 ) )
	{
		vTaskDelay( xTicksToWait );
	}

	return uxReturn;
}

void vEventGroupDelete( EventGroupHandle_t xEventGroup )
{
	if( xEventGroup != NULL )
	{
		vPortFree( xEventGroup );
	}
}
