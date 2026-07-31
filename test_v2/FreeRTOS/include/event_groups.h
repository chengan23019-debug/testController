/*
 * FreeRTOS Kernel V10.4.6
 *
 * Header file event_groups.h
 */

#ifndef EVENT_GROUPS_H
#define EVENT_GROUPS_H

#ifndef INC_FREERTOS_H
	#error "FreeRTOS.h must be included before event_groups.h"
#endif

#include "timers.h"

#ifdef __cplusplus
extern "C" {
#endif

struct EventGroupDef;
typedef struct EventGroupDef * EventGroupHandle_t;
typedef TickType_t EventBits_t;

EventGroupHandle_t xEventGroupCreate( void ) PRIVILEGED_FUNCTION;
EventBits_t xEventGroupWaitBits( EventGroupHandle_t xEventGroup, const EventBits_t uxBitsToWaitFor, const BaseType_t xClearOnExit, const BaseType_t xWaitForAllBits, TickType_t xTicksToWait ) PRIVILEGED_FUNCTION;
EventBits_t xEventGroupClearBits( EventGroupHandle_t xEventGroup, const EventBits_t uxBitsToClear ) PRIVILEGED_FUNCTION;
EventBits_t xEventGroupSetBits( EventGroupHandle_t xEventGroup, const EventBits_t uxBitsToSet ) PRIVILEGED_FUNCTION;
void vEventGroupDelete( EventGroupHandle_t xEventGroup ) PRIVILEGED_FUNCTION;

#ifdef __cplusplus
}
#endif

#endif /* EVENT_GROUPS_H */
