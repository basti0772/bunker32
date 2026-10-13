// Passed to all compiler units by Build-And-Flash.ps1, including TFT_eSPI.
#pragma once
#define USER_SETUP_LOADED
#define USER_SETUP_INFO "Bunker32 ESP32 ILI9341"
#define ILI9341_DRIVER
#define TFT_MISO 19
#define TFT_MOSI 23
#define TFT_SCLK 18
#define TFT_CS 15
#define TFT_DC 2
#define TFT_RST 4
#define TOUCH_CS 32
#define LOAD_GLCD
#define SPI_FREQUENCY 27000000
#define SPI_READ_FREQUENCY 20000000
#define SPI_TOUCH_FREQUENCY 2500000
