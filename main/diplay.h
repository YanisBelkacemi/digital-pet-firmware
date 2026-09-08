/*
 * diplay.h
 *
 *  Created on: Aug 12, 2026
 *      Author: ASUS
 */

 
 
 #ifndef DISPLAY_H
 #define DISPLAY_H

 #include "animations/animations.h"
#include <stdint.h>
#include "esp_lcd_io_spi.h"
 #define TFT_WIDTH   128
 #define TFT_HEIGHT  128
extern esp_lcd_panel_handle_t panel;
 typedef enum {
	OFF,
	ON,
     ANIM_IDLE 
 } animation_t;

extern animation_t current_animation;
 void display_init(void);
 void display_clear(void);
 void play_animation(const Animation *animation);
 void display_image(
     const uint16_t *image,
     int width,
     int height

 );
 void display_turnOff(void);
 void display_turnOn(void);
 
void UI_display(void);
/*
 void display_clear(
     uint16_t color
 );


 void display_pixel(
     int x,
     int y,
     uint16_t color
 );


 void display_rect(
     int x,
     int y,
     int width,
     int height,
     uint16_t color
 );





 void display_image_scaled(
     const uint16_t *image,
     int width,
     int height,
     int x,
     int y,
     int scale
 );
 */

 #endif