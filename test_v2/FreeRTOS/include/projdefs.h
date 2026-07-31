/*
 * FreeRTOS Kernel V10.4.6
 *
 * Header file projdefs.h
 */

#ifndef PROJDEFS_H
#define PROJDEFS_H

typedef void (*TaskFunction_t)( void * );

#define pdFALSE                                 ( ( BaseType_t ) 0 )
#define pdTRUE                                  ( ( BaseType_t ) 1 )

#define pdPASS                                  ( pdTRUE )
#define pdFAIL                                  ( pdFALSE )
#define errQUEUE_EMPTY                          ( ( BaseType_t ) 0 )
#define errQUEUE_FULL                           ( ( BaseType_t ) 0 )

/* FreeRTOS error definitions. */
#define errCOULD_NOT_ALLOCATE_REQUIRED_MEMORY   ( -1 )
#define errQUEUE_BLOCKED                        ( -4 )
#define errQUEUE_YIELD                          ( -5 )

/* FreeRTOS API return values. */
#define pdMS_TO_TICKS( xTimeInMs )              ( ( TickType_t ) ( ( ( TickType_t ) ( xTimeInMs ) * ( TickType_t ) configTICK_RATE_HZ ) / ( TickType_t ) 1000U ) )

#endif /* PROJDEFS_H */
