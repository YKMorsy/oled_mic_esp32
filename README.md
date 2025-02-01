** real time frequency display for audio **

* important config:
1. cpu freq = 160 MHz
2. ADC freq divider = 1 -> ADC freq = 160 MHz
3. 4 mb integrated spi flash
4. 520 kb of SRAM

* tasks:
1. audio input
    - buffer size = 128

3. display values
    - when fft is done processing into 16 bins

* performance improvements:
1. use DMA for ADC capturing so CPU isn't involved during RTOS scheduling
2. use notifications instead of semaphores

/*
 You have to set this config value with menuconfig
 CONFIG_INTERFACE
 for i2c
 CONFIG_MODEL
 CONFIG_SDA_GPIO
 CONFIG_SCL_GPIO
 CONFIG_RESET_GPIO
 for SPI
 CONFIG_CS_GPIO
 CONFIG_DC_GPIO
 CONFIG_RESET_GPIO
*/
