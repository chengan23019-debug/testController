/*
 * FreeRTOS Kernel V10.4.6
 *
 * Header file mpu_wrappers.h
 */

#ifndef MPU_WRAPPERS_H
#define MPU_WRAPPERS_H

#ifndef PRIVILEGED_FUNCTION
	#define PRIVILEGED_FUNCTION
#endif

#ifndef PRIVILEGED_DATA
	#define PRIVILEGED_DATA
#endif

#ifndef FREERTOS_SYSTEM_CALL
	#define FREERTOS_SYSTEM_CALL
#endif

#define portUSING_MPU_WRAPPERS 0

#endif /* MPU_WRAPPERS_H */
