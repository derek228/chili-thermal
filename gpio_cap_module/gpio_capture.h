
#ifndef __GPIO_CAPTURE_H
#define __GPIO_CAPTURE_H

struct gpio_capture_platform_device_data {
    int gpio_nums;
    int irq_type;
    unsigned int debounce_us; 
    int *pgpio;
};

#endif
