/*
 * FreeRTOS Kernel V10.4.6
 *
 * Header file stream_buffer.h
 */

#ifndef STREAM_BUFFER_H
#define STREAM_BUFFER_H

#ifndef INC_FREERTOS_H
	#error "FreeRTOS.h must be included before stream_buffer.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

struct StreamBufferDef;
typedef struct StreamBufferDef * StreamBufferHandle_t;

StreamBufferHandle_t xStreamBufferGenericCreate( size_t xBufferSizeBytes, size_t xTriggerLevelBytes, BaseType_t xIsMessageBuffer ) PRIVILEGED_FUNCTION;
#define xStreamBufferCreate( xBufferSizeBytes, xTriggerLevelBytes ) xStreamBufferGenericCreate( ( xBufferSizeBytes ), ( xTriggerLevelBytes ), pdFALSE )

size_t xStreamBufferSend( StreamBufferHandle_t xStreamBuffer, const void * pvTxData, size_t xDataLengthBytes, TickType_t xTicksToWait ) PRIVILEGED_FUNCTION;
size_t xStreamBufferReceive( StreamBufferHandle_t xStreamBuffer, void * pvRxData, size_t xBufferLengthBytes, TickType_t xTicksToWait ) PRIVILEGED_FUNCTION;
void vStreamBufferDelete( StreamBufferHandle_t xStreamBuffer ) PRIVILEGED_FUNCTION;

#ifdef __cplusplus
}
#endif

#endif /* STREAM_BUFFER_H */
