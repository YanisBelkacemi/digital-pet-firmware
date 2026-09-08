#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "display_bird.h"
#include "esp_now_imp.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_heap_caps.h"

#include "esp_log.h"

#include "diplay.h"

#include "button.h"
#include "pet.h"

#include "ble.h"

#include "client.h"

void app_main(void)
{
	
	display_init();
	display_clear();

	buttonclick();
	//Ble_initilize();
	//client_initialization();
	xTaskCreate(
		getting_hungry,
		"Hungring", 
		2048,
		 NULL, 
		 5, 
		NULL);
	
	
    while (true) {


		
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}
