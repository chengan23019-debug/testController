/*
 * FreeRTOS Kernel V10.4.6
 *
 * Header file portable.h
 */

#ifndef PORTABLE_H
#define PORTABLE_H

#include "deprecated_definitions.h"
#include "mpu_wrappers.h"

#ifdef __cplusplus
extern "C" {
#endif

#include "portmacro.h"

#define portNUM_CONFIGURABLE_REGIONS 1

typedef struct HeapRegion
{
	uint8_t *pucStartAddress;
	size_t xSizeInBytes;
} HeapRegion_t;

typedef struct xHeapStats
{
	size_t xAvailableHeapSpaceInBytes;
	size_t xSizeOfLargestFreeBlockInBytes;
	size_t xSizeOfSmallestFreeBlockInBytes;
	size_t uxNumberOfFreeBlocks;
	size_t uxMinimumEverFreeBytesRemaining;
	size_t uxNumberOfSuccessfulAllocations;
	size_t uxNumberOfSuccessfulFrees;
} HeapStats_t;

StackType_t *pxPortInitialiseStack( StackType_t *pxTopOfStack, TaskFunction_t pxCode, void *pvParameters ) PRIVILEGED_FUNCTION;

void *pvPortMalloc( size_t xSize ) PRIVILEGED_FUNCTION;
void vPortFree( void *pvPort ) PRIVILEGED_FUNCTION;
void vPortInitialiseBlocks( void ) PRIVILEGED_FUNCTION;
size_t xPortGetFreeHeapSize( void ) PRIVILEGED_FUNCTION;
size_t xPortGetMinimumEverFreeHeapSize( void ) PRIVILEGED_FUNCTION;
void vPortGetHeapStats( HeapStats_t *xHeapStats );

BaseType_t xPortStartScheduler( void ) PRIVILEGED_FUNCTION;

void vPortEndScheduler( void ) PRIVILEGED_FUNCTION;

#ifdef __cplusplus
}
#endif

#endif /* PORTABLE_H */
