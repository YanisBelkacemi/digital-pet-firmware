/*
 * animation.h
 *
 *  Created on: Aug 14, 2026
 *      Author: ASUS
 */

#ifndef MAIN_FLAPPY_BIRD_ANIMATION_H_
#define MAIN_FLAPPY_BIRD_ANIMATION_H_
#include "assets/Objects.h"
static volatile bool game_running = false;
void jump(void);
void play_animation_game(const Object *spirit , const Object *pip);

#endif /* MAIN_FLAPPY_BIRD_ANIMATION_H_ */
