/*
 * display.c
 *
 *  Created on: Aug 14, 2026
 *      Author: ASUS
 */
#include "freertos/FreeRTOS.h"


#include "esp_lcd_io_spi.h"


#include "esp_log.h"

#include "../diplay.h"
#include "animation.h"
#include "assets/bird.h"
#include "assets/Objects.h"
#include "assets/obsticle.h"

#include "display_bird.h"

void initialization(){
	
	
	

	play_animation_game(&spirit , &pip_object);
	//play_animation(const Animation *animation)
	
	
}






