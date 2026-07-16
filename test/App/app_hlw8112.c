#include "gd32f4xx.h"
#include "systick.h"
#include <stdio.h>
#include "bsp_hlw8112.h"
#include "FreeRTOS.h"
#include "task.h"
#include "app_hlw8112.h"

/* HLW8112 Data Acquisition Task */
static void hlw8112_task(void *pvParameters)
{
    hlw8112_data_t hlw_data;
    uint8_t cal_ok = 0;
    
    printf("Initializing HLW8112...\r\n");
    hlw8112_init();
    
    /* Verify calibration parameters checksum */
    cal_ok = hlw8112_check_calibration();
    if (cal_ok) {
        printf("HLW8112 Calibration Checksum Verification Success!\r\n");
    } else {
        printf("HLW8112 Calibration Checksum Verification FAILED!\r\n");
    }
    
    while (1) {
        /* Read data from HLW8112 */
        hlw8112_read_data(&hlw_data);
        
        /* Print read results */
        printf("\r\n--- HLW8112 Measurement Data ---\r\n");
        printf("Voltage:      %7.2f V\r\n", hlw_data.voltage);
        printf("Current A:    %7.3f A\r\n", hlw_data.current_a);
        printf("Current B:    %7.3f A\r\n", hlw_data.current_b);
        printf("Power A:      %7.2f W\r\n", hlw_data.active_power_a);
        printf("Power B:      %7.2f W\r\n", hlw_data.active_power_b);
        printf("Energy A:     %7.3f kWh\r\n", hlw_data.active_energy_a);
        printf("Energy B:     %7.3f kWh\r\n", hlw_data.active_energy_b);
        printf("PF:           %7.3f\r\n", hlw_data.power_factor);
        printf("Frequency:    %7.2f Hz\r\n", hlw_data.frequency);
        printf("Phase Angle:  %7.2f Deg\r\n", hlw_data.phase_angle);
        printf("--------------------------------\r\n");
        
        /* Yield CPU to allow other tasks to run. Polling every 1 second (1000ms) is appropriate. */
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

/* Create the HLW8112 data acquisition task in FreeRTOS */
void app_hlw8112_task_create(void)
{
    xTaskCreate((TaskFunction_t)hlw8112_task,
                (const char*    )"hlw8112_task",
                (uint16_t       )512,
                (void*          )NULL,
                (UBaseType_t    )2,
                (TaskHandle_t*  )NULL);
}
