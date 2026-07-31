/*
 * FreeRTOS Kernel V10.4.6
 *
 * Source file stream_buffer.c
 */

#include <stdlib.h>
#include <string.h>

#include "FreeRTOS.h"
#include "task.h"
#include "stream_buffer.h"

typedef struct StreamBufferDef
{
	size_t xTail;
	size_t xHead;
	size_t xLength;
	size_t xTriggerLevelBytes;
	uint8_t *pucBuffer;
} StreamBuffer_t;

StreamBufferHandle_t xStreamBufferGenericCreate( size_t xBufferSizeBytes, size_t xTriggerLevelBytes, BaseType_t xIsMessageBuffer )
{
	StreamBuffer_t *pxStreamBuffer;
	( void ) xIsMessageBuffer;

	if( xBufferSizeBytes > 0 )
	{
		pxStreamBuffer = ( StreamBuffer_t * ) pvPortMalloc( sizeof( StreamBuffer_t ) + xBufferSizeBytes );
		if( pxStreamBuffer != NULL )
		{
			pxStreamBuffer->pucBuffer = ( ( uint8_t * ) pxStreamBuffer ) + sizeof( StreamBuffer_t );
			pxStreamBuffer->xLength = xBufferSizeBytes;
			pxStreamBuffer->xTriggerLevelBytes = xTriggerLevelBytes;
			pxStreamBuffer->xHead = 0;
			pxStreamBuffer->xTail = 0;
			return ( StreamBufferHandle_t ) pxStreamBuffer;
		}
	}
	return NULL;
}

size_t xStreamBufferSend( StreamBufferHandle_t xStreamBuffer, const void * pvTxData, size_t xDataLengthBytes, TickType_t xTicksToWait )
{
	StreamBuffer_t * const pxStreamBuffer = ( StreamBuffer_t * ) xStreamBuffer;
	( void ) xTicksToWait;

	if( ( pxStreamBuffer != NULL ) && ( pvTxData != NULL ) && ( xDataLengthBytes > 0 ) )
	{
		taskENTER_CRITICAL();
		{
			if( xDataLengthBytes > pxStreamBuffer->xLength )
			{
				xDataLengthBytes = pxStreamBuffer->xLength;
			}
			memcpy( pxStreamBuffer->pucBuffer, pvTxData, xDataLengthBytes );
		}
		taskEXIT_CRITICAL();
		return xDataLengthBytes;
	}
	return 0;
}

size_t xStreamBufferReceive( StreamBufferHandle_t xStreamBuffer, void * pvRxData, size_t xBufferLengthBytes, TickType_t xTicksToWait )
{
	StreamBuffer_t * const pxStreamBuffer = ( StreamBuffer_t * ) xStreamBuffer;
	( void ) xTicksToWait;

	if( ( pxStreamBuffer != NULL ) && ( pvRxData != NULL ) && ( xBufferLengthBytes > 0 ) )
	{
		taskENTER_CRITICAL();
		{
			if( xBufferLengthBytes > pxStreamBuffer->xLength )
			{
				xBufferLengthBytes = pxStreamBuffer->xLength;
			}
			memcpy( pvRxData, pxStreamBuffer->pucBuffer, xBufferLengthBytes );
		}
		taskEXIT_CRITICAL();
		return xBufferLengthBytes;
	}
	return 0;
}

void vStreamBufferDelete( StreamBufferHandle_t xStreamBuffer )
{
	if( xStreamBuffer != NULL )
	{
		vPortFree( xStreamBuffer );
	}
}
