#include "../diplay.h"
#include "assets/Objects.h"

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_lcd_panel_dev.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_random.h"

#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>

#include "assets/background_flappy.h"

#define Transparent 0x0000

#define TFT_HEIGHT 128
#define TFT_WIDTH 128

#define PIPE_SPEED      2
#define PIPE_GAP        40
#define PIPE_MIN_HEIGHT 40
#define PIPE_MAX_HEIGHT 70

#define GRAVITY         1
#define JUMP_FORCE     -8

static volatile int spirit_velocity_y = 0;

static bool start = false;

static uint16_t frame_buffer[TFT_WIDTH * TFT_HEIGHT];

static volatile int pip_x;
static volatile int pip_top_y;
static volatile int pip_bottom_y;
static volatile int spirit_y = 20;

static TaskHandle_t pip_task_handle = NULL;
static TaskHandle_t gravity_handler = NULL;

static volatile bool game_running = false;

static int score = -1;


/*
 * 5x7 digit font
 *
 * Each number is 5 pixels wide and 7 pixels high.
 */
static const uint8_t digits[10][7] = {

    // 0
    {
        0b01110,
        0b10001,
        0b10011,
        0b10101,
        0b11001,
        0b10001,
        0b01110
    },

    // 1
    {
        0b00100,
        0b01100,
        0b00100,
        0b00100,
        0b00100,
        0b00100,
        0b01110
    },

    // 2
    {
        0b01110,
        0b10001,
        0b00001,
        0b00010,
        0b00100,
        0b01000,
        0b11111
    },

    // 3
    {
        0b11110,
        0b00001,
        0b00001,
        0b01110,
        0b00001,
        0b00001,
        0b11110
    },

    // 4
    {
        0b00010,
        0b00110,
        0b01010,
        0b10010,
        0b11111,
        0b00010,
        0b00010
    },

    // 5
    {
        0b11111,
        0b10000,
        0b10000,
        0b11110,
        0b00001,
        0b00001,
        0b11110
    },

    // 6
    {
        0b01110,
        0b10000,
        0b10000,
        0b11110,
        0b10001,
        0b10001,
        0b01110
    },

    // 7
    {
        0b11111,
        0b00001,
        0b00010,
        0b00100,
        0b01000,
        0b01000,
        0b01000
    },

    // 8
    {
        0b01110,
        0b10001,
        0b10001,
        0b01110,
        0b10001,
        0b10001,
        0b01110
    },

    // 9
    {
        0b01110,
        0b10001,
        0b10001,
        0b01111,
        0b00001,
        0b00001,
        0b01110
    }
};


/*
 * Draw one digit directly into the framebuffer.
 */
static void draw_digit(
    int x,
    int y,
    int digit,
    uint16_t color
)
{
    for (int row = 0; row < 7; row++)
    {
        for (int col = 0; col < 5; col++)
        {
            if (digits[digit][row] & (1 << (4 - col)))
            {
                int screen_x = x + col;
                int screen_y = y + row;

                if (screen_x >= 0 &&
                    screen_x < TFT_WIDTH &&
                    screen_y >= 0 &&
                    screen_y < TFT_HEIGHT)
                {
                    frame_buffer[
                        screen_y * TFT_WIDTH + screen_x
                    ] = color;
                }
            }
        }
    }
}


/*
 * Draw the current score centered at the top.
 */
static void draw_score(int current_score)
{
    char text[12];

    sprintf(text, "%d", current_score);

    int digit_width = 6;
    int length = strlen(text);

    int x = (TFT_WIDTH - (length * digit_width)) / 2;
    int y = 5;

    for (int i = 0; i < length; i++)
    {
        int digit = text[i] - '0';

        draw_digit(
            x + i * digit_width,
            y,
            digit,
            0xFFFF
        );
    }
}


void jump()
{
    start = true;

    spirit_velocity_y = JUMP_FORCE;

    ESP_LOGI("START", "%d", start);
}


/*
 * Gravity / bird physics task
 */
void gravity(void *arg)
{
    while (game_running)
    {
        if (start)
        {
            spirit_velocity_y += GRAVITY;

            spirit_y += spirit_velocity_y;

            if (spirit_y < 0)
            {
                spirit_y = 0;
                spirit_velocity_y = 0;
            }

            if (spirit_y >= TFT_HEIGHT - 20)
            {
                spirit_y = TFT_HEIGHT - 20;
                spirit_velocity_y = 0;
            }
        }

        vTaskDelay(pdMS_TO_TICKS(20));
    }

    gravity_handler = NULL;

    vTaskDelete(NULL);
}


/*
 * Generate a new pipe position.
 */
static void randomize_pipes(void)
{
    int top_height =
        PIPE_MIN_HEIGHT +
        (
            esp_random() %
            (PIPE_MAX_HEIGHT - PIPE_MIN_HEIGHT + 1)
        );

    pip_top_y =
        -(
            PIPE_MIN_HEIGHT +
            (
                esp_random() %
                (PIPE_MAX_HEIGHT - PIPE_MIN_HEIGHT + 1)
            )
        );

    pip_bottom_y = top_height + PIPE_GAP;
}


/*
 * Pipe movement task
 */
static void pip_animation_task(void *arg)
{
    const Object *pip = (const Object *)arg;

    pip_x = TFT_WIDTH;

    randomize_pipes();

    while (game_running)
    {
        if (start)
        {
            pip_x -= PIPE_SPEED;

            if (pip_x + pip->frame_width < 0)
            {
                pip_x = TFT_WIDTH;

                randomize_pipes();

                /*
                 * Player successfully passed the pipe.
                 */
                score++;
            }
        }

        vTaskDelay(pdMS_TO_TICKS(30));
    }

    pip_task_handle = NULL;

    vTaskDelete(NULL);
}


/*
 * Compose the entire game frame.
 */
void game_compose_frame(
    const uint16_t *background,
    const uint16_t *pet,
    const uint16_t *pip,
    int pip_width,
    int pip_height,
    int pet_width,
    int pet_height,
    int current_pip_x,
    int current_pip_top_y,
    int current_pip_bottom_y,
    int current_spirit_y
)
{
    /*
     * Start with background.
     */
    memcpy(
        frame_buffer,
        background,
        TFT_WIDTH * TFT_HEIGHT * sizeof(uint16_t)
    );


    /*
     * Draw pet.
     */
    int x_pet = (TFT_WIDTH - pet_width) / 2;
    int y_pet = current_spirit_y;

    for (int row = 0; row < pet_height; row++)
    {
        for (int col = 0; col < pet_width; col++)
        {
            uint16_t pixel =
                pet[row * pet_width + col];

            if (pixel != Transparent)
            {
                int screen_x = x_pet + col;
                int screen_y = y_pet + row;

                if (screen_x >= 0 &&
                    screen_x < TFT_WIDTH &&
                    screen_y >= 0 &&
                    screen_y < TFT_HEIGHT)
                {
                    frame_buffer[
                        screen_y * TFT_WIDTH + screen_x
                    ] = pixel;
                }
            }
        }
    }


    /*
     * Draw top pipe.
     */
    for (int row = 0; row < pip_height; row++)
    {
        for (int col = 0; col < pip_width; col++)
        {
            uint16_t pixel =
                pip[row * pip_width + col];

            if (pixel != Transparent)
            {
                int screen_x =
                    current_pip_x + col;

                int screen_y =
                    current_pip_top_y + row;

                if (screen_x >= 0 &&
                    screen_x < TFT_WIDTH &&
                    screen_y >= 0 &&
                    screen_y < TFT_HEIGHT)
                {
                    frame_buffer[
                        screen_y * TFT_WIDTH + screen_x
                    ] = pixel;
                }
            }
        }
    }


    /*
     * Draw bottom pipe.
     */
    for (int row = 0; row < pip_height; row++)
    {
        for (int col = 0; col < pip_width; col++)
        {
            uint16_t pixel =
                pip[row * pip_width + col];

            if (pixel != Transparent)
            {
                int screen_x =
                    current_pip_x + col;

                int screen_y =
                    current_pip_bottom_y + row;

                if (screen_x >= 0 &&
                    screen_x < TFT_WIDTH &&
                    screen_y >= 0 &&
                    screen_y < TFT_HEIGHT)
                {
                    frame_buffer[
                        screen_y * TFT_WIDTH + screen_x
                    ] = pixel;
                }
            }
        }
    }


    /*
     * Draw score LAST so it appears above everything.
     */
    draw_score(score);
}


/*
 * Send framebuffer to TFT.
 */
void game_display_frame(const uint16_t *frame)
{
    static uint16_t spi_buffer[TFT_WIDTH * TFT_HEIGHT];

    for (int i = 0; i < TFT_WIDTH * TFT_HEIGHT; i++)
    {
        spi_buffer[i] =
            (frame[i] >> 8) |
            (frame[i] << 8);
    }

    ESP_ERROR_CHECK(
        esp_lcd_panel_draw_bitmap(
            panel,
            0,
            0,
            TFT_WIDTH,
            TFT_HEIGHT,
            spi_buffer
        )
    );
}


/*
 * Rectangle collision detection.
 */
bool collision(
    int pet_x,
    int pet_y,
    int pet_width,
    int pet_height,
    int pipe_x,
    int pipe_y,
    int pipe_width,
    int pipe_height
)
{
    return (
        pet_x < pipe_x + pipe_width &&
        pet_x + pet_width - 5 > pipe_x &&
        pet_y < pipe_y + pipe_height &&
        pet_y + pet_height - 5 > pipe_y
    );
}


/*
 * Main game loop.
 */
void play_animation_game(
    const Object *spirit,
    const Object *pip
)
{
    srand((unsigned int)esp_random());

    /*
     * Reset game state.
     */
    game_running = true;

    start = false;

    score = 0;

    spirit_y = 20;

    spirit_velocity_y = 0;


    /*
     * Start pipe task.
     */
    if (pip_task_handle == NULL)
    {
        xTaskCreate(
            pip_animation_task,
            "pip_animation",
            2048,
            (void *)pip,
            5,
            &pip_task_handle
        );
    }


    /*
     * Start gravity task.
     */
    if (gravity_handler == NULL)
    {
        xTaskCreate(
            gravity,
            "gravity",
            2048,
            NULL,
            5,
            &gravity_handler
        );
    }


    /*
     * Main rendering loop.
     */
    while (game_running)
    {
        for (
            int i = 0;
            i < spirit->frame_count && game_running;
            i++
        )
        {
            int pet_x =
                (TFT_WIDTH - spirit->frame_width) / 2;


            /*
             * Check collision with TOP pipe.
             */
            bool hit_top = collision(
                pet_x,
                spirit_y,
                spirit->frame_width,
                spirit->frame_height,

                pip_x,
                pip_top_y,
                pip->frame_width,
                pip->frame_height
            );


            /*
             * Check collision with BOTTOM pipe.
             */
            bool hit_bottom = collision(
                pet_x,
                spirit_y,
                spirit->frame_width,
                spirit->frame_height,

                pip_x,
                pip_bottom_y,
                pip->frame_width,
                pip->frame_height
            );


            if (hit_top || hit_bottom)
            {
				
                ESP_LOGI(
                    "GAME",
                    "Collision detected! Score: %d",
                    score
                );

                game_running = false;

                start = false;


				spirit_y = 54;
				pip_x = -128;
				score = -1;
                break;
            }
			


            /*
             * Compose frame.
             */
            game_compose_frame(
                background.frames[0],

                spirit->frames[i],

                pip->frames[i],

                pip->frame_width,
                pip->frame_height,

                spirit->frame_width,
                spirit->frame_height,

                pip_x,
                pip_top_y,
                pip_bottom_y,

                spirit_y
            );


            /*
             * Display frame.
             */
            game_display_frame(frame_buffer);


            /*
             * Approximately 60 FPS.
             */
            vTaskDelay(
                pdMS_TO_TICKS(16)
            );
        }
    }
}