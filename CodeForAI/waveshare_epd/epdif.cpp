#include "epdif.h"
#include "hardware/spi.h"
#include "hardware/gpio.h"
#include "pico/stdlib.h"

int EpdIf::IfInit(void) {
    // GPIO setup for control pins
    gpio_init(CS_PIN);   gpio_set_dir(CS_PIN,   GPIO_OUT); gpio_put(CS_PIN,   1);
    gpio_init(RST_PIN);  gpio_set_dir(RST_PIN,  GPIO_OUT); gpio_put(RST_PIN,  1);
    gpio_init(DC_PIN);   gpio_set_dir(DC_PIN,   GPIO_OUT); gpio_put(DC_PIN,   0);
    gpio_init(BUSY_PIN); gpio_set_dir(BUSY_PIN, GPIO_IN);
    gpio_init(PWR_PIN);  gpio_set_dir(PWR_PIN,  GPIO_OUT); gpio_put(PWR_PIN,  1);

    // SPI is already initialised in main.cpp
    // (spi0, pins 0/2/3 at 2MHz)
    return 0;
}

void EpdIf::DigitalWrite(int pin, int value) {
    gpio_put((uint)pin, value ? 1 : 0);
}

int EpdIf::DigitalRead(int pin) {
    return gpio_get((uint)pin) ? 1 : 0;
}

void EpdIf::DelayMs(unsigned int ms) {
    sleep_ms(ms);
}

void EpdIf::SpiTransfer(unsigned char data) {
    gpio_put(CS_PIN, 0);
    spi_write_blocking(spi0, &data, 1);
    gpio_put(CS_PIN, 1);
}
