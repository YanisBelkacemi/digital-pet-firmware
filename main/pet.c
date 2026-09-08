/*
 * pet.c
 *
 *  Created on: Aug 14, 2026
 *      Author: ASUS
 */

#include "freertos/FreeRTOS.h"
#include "freertos/idf_additions.h"
#include "freertos/projdefs.h"
#include "pet.h"
#include "esp_log.h"


int hunger = 100;
int timer = 60000;

bool feed(){
	
	if (hunger < 100){
		ESP_LOGI("feeding : ", "Being fed");
		timer = 60000;
		return true;
	}
	return false;
}


void getting_hungry(void *args){
	while (1){
		
		vTaskDelay(pdMS_TO_TICKS(timer));
		if(hunger>0){
			hunger -= 1;
			ESP_LOGI("hunger : ", "%d " , hunger );
		}
		timer = 10000;
	}
	
	
}

