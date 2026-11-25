#pragma once
typedef enum { GPIO_NUM_0 = 0 } gpio_num_t; typedef enum { GPIO_MODE_OUTPUT = 2 } gpio_mode_t;
int gpio_set_level(gpio_num_t, uint32_t); int gpio_set_direction(gpio_num_t, gpio_mode_t);
