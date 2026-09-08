#ifndef ANIMATION_H
#define ANIMATION_H

#include <stdint.h>

typedef struct {
    const uint16_t **frames;

    uint16_t frame_width;
    uint16_t frame_height;

    uint16_t frame_count;

    uint16_t frame_delay_ms;
} Animation;

#endif