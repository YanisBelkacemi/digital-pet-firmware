/*

 * buttons.c

 *

 *  Created on: Aug 13, 2026

 *      Author: ASUS

 */

#include "animations/animations.h"

#include "animations/idle.h"

#include "animations/eating.h"

#include "button_gpio.h"

#include "button_types.h"

#include "esp_err.h"

#include "esp_log_level.h"

#include "esp_sleep.h"

#include "iot_button.h"

#include "button.h"

#include "driver/gpio.h"

#include "esp_log.h"

#include <stdint.h>

#include <stdlib.h>

#include <string.h>

#include "freertos/FreeRTOS.h"

#include "freertos/task.h"

#include "diplay.h"

#include "pet.h"

#include "flappy_bird/display_bird.h"

#include "flappy_bird/animation.h"

#include "portmacro.h"

#include "animations/playing.h"

#include "pet.h"

#define TURN_ON 7

#define FEED 1

#define Animate 3

//handling the animation loop problem

static TaskHandle_t animation_handle = NULL;

static TaskHandle_t game_handle = NULL;

static TaskHandle_t eating_handle = NULL;

bool ison = false;

bool busy = false;

bool eating_bool = false;

static void configure_wakeup(void)

{


    esp_err_t err = esp_sleep_enable_gpio_wakeup();

    if (err != ESP_OK)

    {

        ESP_LOGE("SLEEP", "Failed to enable GPIO wakeup: %s",

                 esp_err_to_name(err));

        return;

    }

    gpio_wakeup_enable(

        TURN_ON,

        GPIO_INTR_LOW_LEVEL

    );

    ESP_LOGI("SLEEP", "Wakeup configured on GPIO %d", TURN_ON);

}


static void animation_task(void *arg)

{

    while (1)

    {

	if (hunger <= 100 && hunger > 60){

        play_animation(&playing);

		vTaskDelay(pdMS_TO_TICKS(1));

		}else if(hunger <= 60 && hunger > 20){

			play_animation(&idle);

			vTaskDelay(pdMS_TO_TICKS(1));

		} else if (hunger <= 20){

			play_animation(&idle);

			vTaskDelay(pdMS_TO_TICKS(1));

		}

    } 

};

static void play_game(void *args){

	while(1){

		initialization();

		vTaskDelay(pdMS_TO_TICKS(1));

	}

	vTaskDelete( game_handle);

	game_handle = NULL;

	display_clear();

}

static void animation_task_no_loop(void *arg)

{



	while(hunger<= 100 && eating_bool){

		if (hunger < 100){

			hunger += 1;

			ESP_LOGI("Hunger", "%d", hunger);

			play_animation(&eating);

			vTaskDelay(pdMS_TO_TICKS(1));

		}else{

			break;

		}



}

	if (animation_handle == NULL)

		{

			xTaskCreate(

	        animation_task,

	        "animation",

	        4096,

	        NULL,

	        5,

	        &animation_handle

				 );

	}

	busy = false;

	eating_handle = NULL;

	vTaskDelete(eating_handle);	


}



void Display_turnOff(void *args, void *userdata)

{


	busy = false;

	if (ison && game_handle != NULL){

		vTaskDelete(game_handle);

		game_handle = NULL;

	}

	if (ison && animation_handle != NULL){

			vTaskDelete(animation_handle);

			animation_handle = NULL;

		}
	if (ison && eating_handle != NULL){


			vTaskDelete(eating_handle);

			eating_handle = NULL;

		}

	if(ison){



    ESP_LOGI("BTN", "OFF");

	display_turnOff();

	ison = false;

    if (animation_handle != NULL )

    {

        vTaskDelete(animation_handle);

        animation_handle = NULL;

    }



	}else if (!ison){

		

		ESP_LOGI("BTN", "ON");

		display_turnOn();

	if (animation_handle == NULL)

		{

    		xTaskCreate(

	        animation_task,

	        "animation",

	        4096,

	        NULL,

	        5,

	        &animation_handle

   				 );

	}

	ison = true;

}

}

void feed_animation(void *args, void *userdata){

	if(eating_bool){

		eating_bool = false;

		}
		ESP_LOGI("game", "clicked");
	if  (ison && game_handle != NULL ){
			ESP_LOGI("game", "jump");
			jump();
			}
	else if(ison && game_handle == NULL){
		
		if (!eating_bool){

					eating_bool = true;

				}

		if (feed()){
			ESP_LOGI("game", "not jump");
			if (!busy){

				busy = true;

			}

			if (animation_handle != NULL){

				vTaskDelete(animation_handle);

				animation_handle = NULL;

			}

			ESP_LOGI("feeding : ", "entered" );

			eating_handle = NULL;

			xTaskCreate(animation_task_no_loop, 

			"feed animation", 

			4096,

			NULL,

			 5

			,&eating_handle);




		};	

	}

}

void start_game(void *args, void *userdata){

	if ( ison && game_handle == NULL && !busy){
	
		busy = true;

		ESP_LOGI("game : ","starting");

		if (animation_handle != NULL)

		{

		    vTaskDelete(animation_handle);

		    animation_handle = NULL;

		}

		display_clear();

		xTaskCreate(

		    play_game,

		    "animation",

		    4096,

		    NULL,

		    5,

		    &game_handle

		);

	}

}



void buttonclick(){

	button_config_t config = {0};

	//Start gadget

	button_gpio_config_t turn_off = {

		.gpio_num = TURN_ON,

		.active_level = 0,

		.disable_pull = false

	};

	button_handle_t turn_off_h = NULL;

	ESP_ERROR_CHECK(

		iot_button_new_gpio_device(&config, &turn_off, &turn_off_h)

	);

	ESP_ERROR_CHECK(iot_button_register_cb(turn_off_h , BUTTON_SINGLE_CLICK , NULL , Display_turnOff, NULL));

	//feed pet

	button_gpio_config_t feed = {

			.gpio_num = FEED,

			.active_level = 0,

			.disable_pull = false

		};

		button_handle_t feed_h = NULL;

		ESP_ERROR_CHECK(

			iot_button_new_gpio_device(&config, &feed, &feed_h)

		);

		ESP_ERROR_CHECK(iot_button_register_cb(feed_h , BUTTON_PRESS_DOWN , NULL , feed_animation, NULL));

		button_gpio_config_t start_game_btn = {

				.gpio_num = Animate,

				.active_level = 0,

				.disable_pull = false

			};

			button_handle_t start_game_h = NULL;

			ESP_ERROR_CHECK(

				iot_button_new_gpio_device(&config, &start_game_btn, &start_game_h)

			);

			ESP_ERROR_CHECK(iot_button_register_cb(start_game_h , BUTTON_SINGLE_CLICK , NULL , start_game, NULL));

}