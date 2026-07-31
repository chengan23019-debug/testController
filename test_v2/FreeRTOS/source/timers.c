/*
 * FreeRTOS Kernel V10.4.6
 *
 * Source file timers.c
 */

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "timers.h"

#if ( configUSE_TIMERS == 1 )

typedef struct tmrTimerControl
{
	const char *pcTimerName;
	ListItem_t xTimerListItem;
	TickType_t xTimerPeriodInTicks;
	UBaseType_t uxAutoReload;
	void *pvTimerID;
	TimerCallbackFunction_t pxCallbackFunction;
} tmrTimerControl;

TimerHandle_t xTimerCreate( const char * const pcTimerName, const TickType_t xTimerPeriodInTicks, const UBaseType_t uxAutoReload, void * const pvTimerID, TimerCallbackFunction_t pxCallbackFunction )
{
	tmrTimerControl *pxNewTimer;

	pxNewTimer = ( tmrTimerControl * ) pvPortMalloc( sizeof( tmrTimerControl ) );

	if( pxNewTimer != NULL )
	{
		pxNewTimer->pcTimerName = pcTimerName;
		pxNewTimer->xTimerPeriodInTicks = xTimerPeriodInTicks;
		pxNewTimer->uxAutoReload = uxAutoReload;
		pxNewTimer->pvTimerID = pvTimerID;
		pxNewTimer->pxCallbackFunction = pxCallbackFunction;
		vListInitialiseItem( &( pxNewTimer->xTimerListItem ) );
	}

	return ( TimerHandle_t ) pxNewTimer;
}

void * pvTimerGetTimerID( const TimerHandle_t xTimer )
{
	tmrTimerControl * const pxTimer = ( tmrTimerControl * ) xTimer;
	return pxTimer->pvTimerID;
}

void vTimerSetTimerID( TimerHandle_t xTimer, void * pvNewID )
{
	tmrTimerControl * const pxTimer = ( tmrTimerControl * ) xTimer;
	pxTimer->pvTimerID = pvNewID;
}

BaseType_t xTimerGenericCommand( TimerHandle_t xTimer, const BaseType_t xCommandID, const TickType_t xOptionalValue, BaseType_t * const pxHigherPriorityTaskWoken, const TickType_t xTicksToWait )
{
	( void ) xTimer;
	( void ) xCommandID;
	( void ) xOptionalValue;
	( void ) pxHigherPriorityTaskWoken;
	( void ) xTicksToWait;
	return pdPASS;
}

#endif /* configUSE_TIMERS */
