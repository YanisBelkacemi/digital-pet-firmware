#include "animations/animations.h"
#include "diplay.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "driver/gpio.h"
#include "driver/spi_common.h"
#include "driver/spi_master.h"

#include "esp_err.h"
#include "esp_lcd_panel_dev.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_io_spi.h"
#include "esp_lcd_st7735.h"

#include "animations/background.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#define CS_PIN 6
#define RST_PIN 0
#define DC_PIN 5
#define MOSI_PIN 21
#define SLTK_PIN 20
#define LD_PIN 4

#define TFT_WIDTH 128
#define TFT_HEIGHT 128

#define BG_WIDTH 128
#define BG_SOURCE_HEIGHT 128
#define BG_HEIGHT 128

#define UI_HEIGHT 16
#define BG_CROP_TOP 0

#define SPI_CLOCK_HZ (20 * 1000 * 1000)

#define Transparent 0x0000

#define BUFFER_ROWS 16

typedef struct
{
    const uint16_t *pixels;
    int width;
    int height;
    int x;
    int y;
    bool transparent;
} Layer;

esp_lcd_panel_handle_t panel = NULL;

static esp_lcd_panel_io_handle_t panel_io = NULL;

static uint16_t render_buffer_a[
    TFT_WIDTH * BUFFER_ROWS
];

static uint16_t render_buffer_b[
    TFT_WIDTH * BUFFER_ROWS
];

static Layer background_layer = {
    .pixels = NULL,
    .width = BG_WIDTH,
    .height = BG_HEIGHT,
    .x = 0,
    .y = 0,
    .transparent = false
};

static Layer pet_layer = {
    .pixels = NULL,
    .width = 0,
    .height = 0,
    .x = 0,
    .y = 0,
    .transparent = true
};

animation_t current_animation = ANIM_IDLE;


/* Initialize the display and background layer. */
void display_init(void)
{
    gpio_config_t config = {
        .pin_bit_mask = (1ULL << LD_PIN),
        .mode = GPIO_MODE_DEF_OUTPUT,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };

    ESP_ERROR_CHECK(
        gpio_config(&config)
    );

    gpio_set_level(
        LD_PIN,
        0
    );

    spi_bus_config_t io = {
        .sclk_io_num = SLTK_PIN,
        .mosi_io_num = MOSI_PIN,
        .miso_io_num = -1,
        .quadhd_io_num = -1,
        .quadwp_io_num = -1,
        .max_transfer_sz =
            TFT_WIDTH *
            BUFFER_ROWS *
            sizeof(uint16_t)
    };

    ESP_ERROR_CHECK(
        spi_bus_initialize(
            SPI2_HOST,
            &io,
            SPI_DMA_CH_AUTO
        )
    );

    esp_lcd_panel_io_spi_config_t panel_h_config = {
        .cs_gpio_num = CS_PIN,
        .dc_gpio_num = DC_PIN,
        .pclk_hz = SPI_CLOCK_HZ,
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
        .spi_mode = 0,
        .trans_queue_depth = 10
    };

    ESP_ERROR_CHECK(
        esp_lcd_new_panel_io_spi(
            (esp_lcd_spi_bus_handle_t)SPI2_HOST,
            &panel_h_config,
            &panel_io
        )
    );

    esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = RST_PIN,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_BGR,
        .bits_per_pixel = 16
    };

    ESP_ERROR_CHECK(
        esp_lcd_new_panel_st7735(
            panel_io,
            &panel_config,
            &panel
        )
    );

    ESP_ERROR_CHECK(
        esp_lcd_panel_reset(panel)
    );

    ESP_ERROR_CHECK(
        esp_lcd_panel_init(panel)
    );

    ESP_ERROR_CHECK(
        esp_lcd_panel_set_gap(
            panel,
            2,
            1
        )
    );

    ESP_ERROR_CHECK(
        esp_lcd_panel_invert_color(
            panel,
            false
        )
    );

    ESP_ERROR_CHECK(
        esp_lcd_panel_disp_on_off(
            panel,
            true
        )
    );

    if (
        background.frame_count > 0 &&
        background.frames != NULL
    )
    {
        background_layer.pixels =
            background.frames[0];
    }

    gpio_set_level(
        LD_PIN,
        0
    );
	
}


/* Set the pet layer. */
static void set_pet_layer(
    const uint16_t *image,
    int width,
    int height
)
{
    if (image == NULL)
        return;

    pet_layer.pixels = image;

    pet_layer.width = width;
    pet_layer.height = height;

    pet_layer.x = width / 5;

    pet_layer.y = height/3;
}


/* Compose the background and pet layers. */
static void compose_layers(
    uint16_t *buffer,
    int screen_y,
    int rows
)
{
    for (
        int row = 0;
        row < rows;
        row++
    )
    {
        int y =
            screen_y + row;

        for (
            int x = 0;
            x < TFT_WIDTH;
            x++
        )
        {
            uint16_t pixel =
                0xF800 ;

            int bg_x =
                x - background_layer.x;

            int bg_y =
                y - background_layer.y;

            if (
                background_layer.pixels != NULL &&
                bg_x >= 0 &&
                bg_x < BG_WIDTH &&
                bg_y >= 0 &&
                bg_y < BG_HEIGHT
            )
            {
                int source_y =
                    bg_y + BG_CROP_TOP;

                pixel =
                    background_layer.pixels[
                        source_y *
                        BG_SOURCE_HEIGHT +
                        bg_x
                    ];
            }

            if (
                pet_layer.pixels != NULL
            )
            {
                int pet_x =
                    x - pet_layer.x;

                int pet_y =
                    y - pet_layer.y;

                if (
                    pet_x >= 0 &&
                    pet_x < pet_layer.width &&
                    pet_y >= 0 &&
                    pet_y < pet_layer.height
                )
                {
                    uint16_t pet_pixel =
                        pet_layer.pixels[
                            pet_y *
                            pet_layer.width +
                            pet_x
                        ];

                    if (
                        pet_pixel != Transparent
                    )
                    {
                        pixel =
                            pet_pixel;
                    }
                }
            }

            buffer[
                row *
                TFT_WIDTH +
                x
            ] =
                (pixel >> 8) |
                (pixel << 8);
        }
    }
}


/* Render the background and pet without touching the UI area. */
static void render_layers(void)
{
    for (
        int y = 0;
        y < TFT_HEIGHT;
        y += BUFFER_ROWS
    )
    {
        int rows =
            TFT_HEIGHT - y;

        if (
            rows > BUFFER_ROWS
        )
        {
            rows =
                BUFFER_ROWS;
        }

        uint16_t *buffer =
            ((y / BUFFER_ROWS) % 2 == 0)
                ? render_buffer_a
                : render_buffer_b;

        compose_layers(
            buffer,
            y,
            rows
        );

        ESP_ERROR_CHECK(
            esp_lcd_panel_draw_bitmap(
                panel,
                0,
                y,
                TFT_WIDTH,
                y + rows,
                buffer
            )
        );
    }
}





/* Change the background layer. */
void display_set_background(
    const uint16_t *image
)
{
    if (image == NULL)
        return;

    background_layer.pixels =
        image;

    render_layers();
}


/* Change the pet layer. */
void display_set_pet(
    const uint16_t *image,
    int width,
    int height
)
{
    if (image == NULL)
        return;

    set_pet_layer(
        image,
        width,
        height
    );

    render_layers();
}


/* Clear the display with black. */
void display_clear(void)
{
    static uint16_t buffer[
        TFT_WIDTH * BUFFER_ROWS
    ];

    uint16_t black =
        0x0000;

    for (
        int i = 0;
        i < TFT_WIDTH * BUFFER_ROWS;
        i++
    )
    {
        buffer[i] =
            black;
    }

    for (
        int y = 0;
        y < TFT_HEIGHT;
        y += BUFFER_ROWS
    )
    {
        int rows =
            TFT_HEIGHT - y;

        if (
            rows > BUFFER_ROWS
        )
        {
            rows =
                BUFFER_ROWS;
        }

        ESP_ERROR_CHECK(
            esp_lcd_panel_draw_bitmap(
                panel,
                0,
                y,
                TFT_WIDTH,
                y + rows,
                buffer
            )
        );
    }
}


/* Turn the display backlight off. */
void display_turnOff(void)
{
	
	display_clear();
    gpio_set_level(
        LD_PIN,
        0
    );
}


/* Turn the display backlight on. */ 
void display_turnOn(void)
{
		
	   	gpio_set_level(
        LD_PIN,
        1
    );
}


/* Play a pet animation. */
void play_animation(
    const Animation *animation
)
{
    if (
        animation == NULL ||
        animation->frames == NULL ||
        animation->frame_count <= 0
    )
    {
        return;
    }

    set_pet_layer(
        animation->frames[0],
        animation->frame_width,
        animation->frame_height
    );

    for (
        int i = 0;
        i < animation->frame_count;
        i++
    )
    {
        pet_layer.pixels =
            animation->frames[i];

        render_layers();

        vTaskDelay(
            pdMS_TO_TICKS(
                animation->frame_delay_ms
            )
        );
    }
}