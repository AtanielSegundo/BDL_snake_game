#define BDLED_IMPLEMENTATION
#include "bdl_led_matrix.h"
#include "music.h"
#include "pico/stdlib.h"
#include "hardware/adc.h"
#include "hardware/pwm.h"
#include "pico/multicore.h"
#include <stdio.h>
#include <stdlib.h>

#define JOYSTICK_X 27
#define JOYSTICK_Y 26
#define JOYSTICK_SW 22
#define MATRIX_SIZE 5
#define BRIGHT 0.06

#define BUZZER_A 21  // Bass-like sounds
#define BUZZER_B 10  // Piano-like sounds

typedef struct {
    int x, y;
} Point;

LedMatrix matrix;
Point snake[MATRIX_SIZE * MATRIX_SIZE];
int snake_length = 1;
Point direction = {0, 0};
Point food;

void init_hardware() {
    stdio_init_all();
    bdl_matrixInit(&matrix, LED_PIN, MATRIX_SIZE, MATRIX_SIZE);
    bdl_startMatrixUpdater(&matrix, 120);
    adc_init();
    adc_gpio_init(JOYSTICK_X);
    adc_gpio_init(JOYSTICK_Y);
    gpio_init(JOYSTICK_SW);
    gpio_set_dir(JOYSTICK_SW, GPIO_IN);
    gpio_pull_up(JOYSTICK_SW);
}

// Initialize a buzzer for PWM output
void buzzer_init(uint gpio) {
    gpio_set_function(gpio, GPIO_FUNC_PWM);
    uint slice_num = pwm_gpio_to_slice_num(gpio);
    pwm_set_enabled(slice_num, false);
}

// Play tones concurrently on two buzzers.
// A frequency value of 0 is treated as a rest for that channel.
void play_dual_tone(uint gpio_a, uint gpio_b, float freq_a, float freq_b, int duration_ms) {
    uint wrap = 12500;

    // Setup Buzzer A (Bass)
    if (freq_a > 0) {
        uint slice_a = pwm_gpio_to_slice_num(gpio_a);
        uint channel_a = pwm_gpio_to_channel(gpio_a);
        pwm_set_wrap(slice_a, wrap);
        float divider_a = 125000000.0f / (freq_a * (wrap + 1));
        pwm_set_clkdiv(slice_a, divider_a);
        pwm_set_chan_level(slice_a, channel_a, wrap / 512);
        pwm_set_enabled(slice_a, true);
    }

    if (freq_b > 0) {
        uint slice_b = pwm_gpio_to_slice_num(gpio_b);
        uint channel_b = pwm_gpio_to_channel(gpio_b);
        pwm_set_wrap(slice_b, wrap);
        float divider_b = 125000000.0f / (freq_b * (wrap + 1));
        pwm_set_clkdiv(slice_b, divider_b);
        pwm_set_chan_level(slice_b, channel_b, wrap / 512);
        pwm_set_enabled(slice_b, true);
    }

    sleep_ms(duration_ms);
    if (freq_a > 0) {
        uint slice_a = pwm_gpio_to_slice_num(gpio_a);
        pwm_set_enabled(slice_a, false);
    }
    if (freq_b > 0) {
        uint slice_b = pwm_gpio_to_slice_num(gpio_b);
        pwm_set_enabled(slice_b, false);
    }
}

void music_thread() {
    buzzer_init(BUZZER_A);
    buzzer_init(BUZZER_B);

    typedef struct {
        float bass_freq;   // Frequency for bass (Buzzer A)
        float piano_freq;  // Frequency for melody (Buzzer B)
        int duration;      // Duration in milliseconds
    } NoteEvent;
    
    int melody_length = sizeof(music) / sizeof(music[0]);
    while (true) {
        for (int i = 0; i < melody_length; i++) {
            play_dual_tone(BUZZER_A, BUZZER_B,
                           music[i].bass_freq,
                           music[i].piano_freq,
                           music[i].duration);
        }
        sleep_ms(50);
    }
}

void read_joystick() {
    adc_select_input(0);
    int x_val = adc_read();
    adc_select_input(1);
    int y_val = adc_read();
    direction = (Point){0, 0};
    if (x_val < 800) direction.x = -1;
    else if (x_val > 3000) direction.x = 1;
    if (y_val < 800) direction.y = -1;
    else if (y_val > 3000) direction.y = 1;
}

bool colide_with_snake(int x, int y, int start) {
    bool colide = false;
    for (int p = start; p < snake_length; p++) {
        colide |= (snake[p].x == x) && (snake[p].y == y);
    }
    return colide;
}

void spawn_food() {
    do {
        food.x = rand() % MATRIX_SIZE;
        food.y = rand() % MATRIX_SIZE;
    } while (colide_with_snake(food.x, food.y, 0));
}

void update_snake() {
    bool reset_state = false;
    bool is_moving = (direction.x != 0 || direction.y != 0);
    Point new_head = {snake[0].x + direction.x, snake[0].y + direction.y};
    if (new_head.x < 0 || new_head.x >= MATRIX_SIZE || new_head.y < 0 || new_head.y >= MATRIX_SIZE) {
        is_moving = false; 
    }
    if (is_moving) {
        for (int i = snake_length; i > 0; i--) {
            snake[i] = snake[i - 1];
        }
        snake[0] = new_head;
        if (colide_with_snake(snake[0].x, snake[0].y, 1)) {
            snake_length = 1;
            snake[0].x = 2;
            snake[0].y = 2;
            direction.x = 0;
            direction.y = 0;
        }
    }
    if (snake[0].x == food.x && snake[0].y == food.y) {
        snake_length++;
        spawn_food();
    }
}

void draw() {
    bdl_matrixClear(&matrix);
    for (int i = 0; i < snake_length; i++) {
        if (i == 0)
            bdl_matrixSetPixel(&matrix, snake[i].x, snake[i].y, 127, 127, 0, BRIGHT*1.5);
        else{
            if (i == snake_length - 1){
                bdl_matrixSetPixel(&matrix, snake[i].x, snake[i].y, 0x99, 0x33, 0x99, BRIGHT);
            }
            else if (i % 2 == 0 ){
                bdl_matrixSetPixel(&matrix, snake[i].x, snake[i].y, 0, 255, 0, BRIGHT);
            }
            else{
                bdl_matrixSetPixel(&matrix, snake[i].x, snake[i].y, 0xFF, 0X14, 0X93, BRIGHT);
            }

        }
    }
    bdl_matrixSetPixel(&matrix, food.x, food.y, 255, 0, 0, BRIGHT);
    bdl_waitMatrixUpdate();
}

int main() {
    init_hardware();
    snake[0] = (Point){2, 3};
    spawn_food();

    // Launch the music thread on core1 so that the melody plays concurrently with the snake game.
    multicore_launch_core1(music_thread);

    while (true) {
        read_joystick();
        update_snake();
        draw();
        sleep_ms(150);
    }
}
