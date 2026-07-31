/*
 * FreeRTOS Kernel V10.4.6
 *
 * Header file task.h
 */

#ifndef TASK_H
#define TASK_H

#ifndef INC_FREERTOS_H
	#error "FreeRTOS.h must be included before task.h"
#endif

#include "list.h"

#ifdef __cplusplus
extern "C" {
#endif

struct tskTaskControlBlock;
typedef struct tskTaskControlBlock* TaskHandle_t;

typedef enum
{
	eRunning = 0,
	eReady,
	eBlocked,
	eSuspended,
	eDeleted,
	eInvalid
} eTaskState;

typedef enum
{
	eNoAction = 0,
	eSetBits,
	eIncrement,
	eSetValueWithOverwrite,
	eSetValueWithoutOverwrite
} eNotifyAction;

typedef struct xTIME_OUT
{
	BaseType_t xOverflowCount;
	TickType_t xTimeOnEntering;
} TimeOut_t;

typedef struct xMEMORY_REGION
{
	void * pvBaseAddress;
	uint32_t ulLengthInBytes;
	uint32_t ulParameters;
} MemoryRegion_t;

typedef struct xTASK_PARAMETERS
{
	TaskFunction_t pvTaskCode;
	const char * pcName;
	uint16_t usStackDepth;
	void * pvParameters;
	UBaseType_t uxPriority;
	StackType_t * puxStackBuffer;
	MemoryRegion_t xRegions[ portNUM_CONFIGURABLE_REGIONS ];
} TaskParameters_t;

typedef struct xTASK_STATUS
{
	TaskHandle_t xHandle;
	const char * pcTaskName;
	UBaseType_t xTaskNumber;
	eTaskState eCurrentState;
	UBaseType_t uxCurrentPriority;
	UBaseType_t uxBasePriority;
	uint32_t ulRunTimeCounter;
	StackType_t * pxStackBase;
	uint16_t usStackHighWaterMark;
} TaskStatus_t;

typedef enum
{
	eAbortSleep = 0,
	eStandardSleep,
	eNoTasksWaitingTimeout
} eSleepModeStatus;

#define tskIDLE_PRIORITY			( ( UBaseType_t ) 0U )

#define taskSCHEDULER_SUSPENDED		( ( BaseType_t ) 0 )
#define taskSCHEDULER_NOT_STARTED	( ( BaseType_t ) 1 )
#define taskSCHEDULER_RUNNING		( ( BaseType_t ) 2 )

/* Task API Function Prototypes */
BaseType_t xTaskCreate(	TaskFunction_t pxTaskCode,
						const char * const pcName,
						const uint16_t usStackDepth,
						void * const pvParameters,
						UBaseType_t uxPriority,
						TaskHandle_t * const pxCreatedTask ) PRIVILEGED_FUNCTION;

void vTaskDelete( TaskHandle_t xTaskToDelete ) PRIVILEGED_FUNCTION;

void vTaskDelay( const TickType_t xTicksToDelay ) PRIVILEGED_FUNCTION;

void vTaskDelayUntil( TickType_t * const pxPreviousWakeTime, const TickType_t xTimeIncrement ) PRIVILEGED_FUNCTION;

UBaseType_t uxTaskPriorityGet( const TaskHandle_t xTask ) PRIVILEGED_FUNCTION;

void vTaskPrioritySet( TaskHandle_t xTask, UBaseType_t uxNewPriority ) PRIVILEGED_FUNCTION;

void vTaskSuspend( TaskHandle_t xTaskToSuspend ) PRIVILEGED_FUNCTION;

void vTaskResume( TaskHandle_t xTaskToResume ) PRIVILEGED_FUNCTION;

void vTaskStartScheduler( void ) PRIVILEGED_FUNCTION;

void vTaskEndScheduler( void ) PRIVILEGED_FUNCTION;

void vTaskSuspendAll( void ) PRIVILEGED_FUNCTION;

BaseType_t xTaskResumeAll( void ) PRIVILEGED_FUNCTION;

TickType_t xTaskGetTickCount( void ) PRIVILEGED_FUNCTION;

TickType_t xTaskGetTickCountFromISR( void ) PRIVILEGED_FUNCTION;

UBaseType_t uxTaskGetNumberOfTasks( void ) PRIVILEGED_FUNCTION;

char * pcTaskGetName( TaskHandle_t xTaskToQuery ) PRIVILEGED_FUNCTION;

TaskHandle_t xTaskGetHandle( const char * pcNameToQuery ) PRIVILEGED_FUNCTION;

UBaseType_t uxTaskGetStackHighWaterMark( TaskHandle_t xTask ) PRIVILEGED_FUNCTION;

BaseType_t xTaskCallApplicationTaskHook( TaskHandle_t xTask, void * pvParameter ) PRIVILEGED_FUNCTION;

TaskHandle_t xTaskGetCurrentTaskHandle( void ) PRIVILEGED_FUNCTION;

BaseType_t xTaskGetSchedulerState( void ) PRIVILEGED_FUNCTION;

BaseType_t xTaskNotifyGive( TaskHandle_t xTaskToNotify ) PRIVILEGED_FUNCTION;
void vTaskNotifyGiveFromISR( TaskHandle_t xTaskToNotify, BaseType_t * pxHigherPriorityTaskWoken ) PRIVILEGED_FUNCTION;
uint32_t ulTaskNotifyTake( BaseType_t xClearCountOnExit, TickType_t xTicksToWait ) PRIVILEGED_FUNCTION;

BaseType_t xTaskNotify( TaskHandle_t xTaskToNotify, uint32_t ulValue, eNotifyAction eAction ) PRIVILEGED_FUNCTION;
BaseType_t xTaskNotifyFromISR( TaskHandle_t xTaskToNotify, uint32_t ulValue, eNotifyAction eAction, BaseType_t * pxHigherPriorityTaskWoken ) PRIVILEGED_FUNCTION;
BaseType_t xTaskNotifyWait( uint32_t ulBitsToClearOnEntry, uint32_t ulBitsToClearOnExit, uint32_t * pulNotificationValue, TickType_t xTicksToWait ) PRIVILEGED_FUNCTION;

/* Internal functions for port use */
void vTaskIncrementTick( void ) PRIVILEGED_FUNCTION;
void vTaskSwitchContext( void ) PRIVILEGED_FUNCTION;

#ifdef __cplusplus
}
#endif

#endif /* TASK_H */
