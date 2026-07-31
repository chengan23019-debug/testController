/*
 * FreeRTOS Kernel V10.4.6
 *
 * Header file timers.h
 */

#ifndef TIMERS_H
#define TIMERS_H

#ifndef INC_FREERTOS_H
	#error "FreeRTOS.h must be included before timers.h"
#endif

#include "task.h"

#ifdef __cplusplus
extern "C" {
#endif

struct tmrTimerControl;
typedef struct tmrTimerControl * TimerHandle_t;
typedef void (*TimerCallbackFunction_t)( TimerHandle_t xTimer );

TimerHandle_t xTimerCreate( const char * const pcTimerName, const TickType_t xTimerPeriodInTicks, const UBaseType_t uxAutoReload, void * const pvTimerID, TimerCallbackFunction_t pxCallbackFunction ) PRIVILEGED_FUNCTION;
void * pvTimerGetTimerID( const TimerHandle_t xTimer ) PRIVILEGED_FUNCTION;
void vTimerSetTimerID( TimerHandle_t xTimer, void * pvNewID ) PRIVILEGED_FUNCTION;
BaseType_t xTimerIsTimerActive( TimerHandle_t xTimer ) PRIVILEGED_FUNCTION;
TaskHandle_t xTimerGetTimerDaemonTaskHandle( void ) PRIVILEGED_FUNCTION;

#define xTimerStart( xTimer, xTicksToWait ) xTimerGenericCommand( ( xTimer ), tmrCOMMAND_START, ( xTaskGetTickCount() ), NULL, ( xTicksToWait ) )
#define xTimerStop( xTimer, xTicksToWait ) xTimerGenericCommand( ( xTimer ), tmrCOMMAND_STOP, 0U, NULL, ( xTicksToWait ) )
#define xTimerReset( xTimer, xTicksToWait ) xTimerGenericCommand( ( xTimer ), tmrCOMMAND_RESET, ( xTaskGetTickCount() ), NULL, ( xTicksToWait ) )

#define tmrCOMMAND_EXECUTE_CALLBACK_FROM_ISR 	( ( BaseType_t ) -2 )
#define tmrCOMMAND_EXECUTE_CALLBACK				( ( BaseType_t ) -1 )
#define tmrCOMMAND_START_DONT_TRACE				( ( BaseType_t ) 0 )
#define tmrCOMMAND_START					    ( ( BaseType_t ) 1 )
#define tmrCOMMAND_RESET						( ( BaseType_t ) 2 )
#define tmrCOMMAND_STOP							( ( BaseType_t ) 3 )
#define tmrCOMMAND_CHANGE_PERIOD				( ( BaseType_t ) 4 )
#define tmrCOMMAND_DELETE						( ( BaseType_t ) 5 )

BaseType_t xTimerGenericCommand( TimerHandle_t xTimer, const BaseType_t xCommandID, const TickType_t xOptionalValue, BaseType_t * const pxHigherPriorityTaskWoken, const TickType_t xTicksToWait ) PRIVILEGED_FUNCTION;
BaseType_t xTimerCreateTimerTask( void ) PRIVILEGED_FUNCTION;

#ifdef __cplusplus
}
#endif

#endif /* TIMERS_H */
