/*
 * peripherals.h
 */

#ifndef MAIN_PERIPHERALS_H_
#define MAIN_PERIPHERALS_H_

/////// Includes

#include "driver/adc.h"
#include "driver/gpio.h"

// From project

#include "main.h"

/////// Definitions   

// Note: please see schematics in: https://easyeda.com/JoaoLopesF/esp-idf-mobile-app-v1-0
// TODO: see it! 

//// Input pins

// Standby button for deep sleep - comment to disable this - please see schematics exmaples

//#define PIN_BUTTON_STANDBY GPIO_NUM_4

#ifdef HAVE_BATTERY

	// Sensor 5V VUSB - comment to disable this - please see schematics exmaples

	#define PIN_SENSOR_VUSB GPIO_NUM_16

	// VBat sensor ground - comment to disable this - please see schematics exmaples

	#define PIN_GROUND_VBAT GPIO_NUM_12

#endif

// Install ISR service ?

#if defined PIN_BUTTON_STANDBY || defined PIN_SENSOR_VUSB

	#define INSTALL_ISR_SERVICE true

#endif

///// ADC

// Filter median to ADC readings - comment if your project not use it

#define MEDIAN_FILTER_READINGS 7

// Sensor voltage battery - comment if your project not use it

//#define ADC_SENSOR_VBAT ADC1_CHANNEL_7

///// Output pins

// Esp32 board led pin (it in some boards is 5, anothers is 2. comment to disable this)

//#define PIN_LED_ESP32 GPIO_NUM_5
//#define PIN_LED_ESP32 GPIO_NUM_2

/////// Prototypes

void peripheralsInitialize();
void peripheralsFinalize();

void gpioBlinkLedEsp32();

void adcRead();

//////// Macros

#define gpioSetLevel(gpio_num, level) 	gpio_set_level(gpio_num, level)
#define gpioIsHigh(gpio_num) 			(gpio_get_level(gpio_num) == 1u)
#define gpioIsLow(gpio_num) 			(gpio_get_level(gpio_num) == 0u)
#define gpioDisableISR(gpio_num) 		gpio_isr_handler_remove(gpio_num)

#endif /* MAIN_PERIPHERALS_H_ */

//////// End