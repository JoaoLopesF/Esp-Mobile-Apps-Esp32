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

///// ADC

// Filter median to ADC readings - comment if your project not use it

#define MEDIAN_FILTER_READINGS 7

// Sensor voltage Bettery - comment if your project not use it

#ifdef HAVE_BATTERY 
	#define ADC_SENSOR_VBAT ADC1_CHANNEL_7
#endif

///// Digital

#ifdef HAVE_STANDBY

	// To enter/exit of deep sleep, actually supported only a button 
	// In future will support to touchpad too

	// Standby button for deep sleep - comment to disable this - please see schematics

	#define PIN_BUTTON_STANDBY GPIO_NUM_4
#endif

#ifdef HAVE_BATTERY

	// Sensor external voltage (USB or power supply) - comment to disable this - please see schematics
	// Note: This only for 5v, if your project is powered by more than this, please 
	// recalculate de resistor divider to put this in maximum 3v3
	// More than this, you can burn the ESP32
	// TODO: see it!

	#define PIN_SENSOR_VEXT GPIO_NUM_16

	// VBAT sensor ground - comment to disable this - please see schematics

	#define PIN_GROUND_VBAT GPIO_NUM_12

	#ifdef PIN_GROUND_VBAT
		// To ground only when reading VBAT, to reduce consupmition

		// VBAT sensor ground by MOSFET - comment if without MOSFET - please see schematics

		//#define PIN_GROUND_VBAT_MOSTFET true 

		#ifdef PIN_GROUND_VBAT_MOSTFET
			#define GPIO_LEVEL_READ_VBAT_ON 1
			#define GPIO_LEVEL_READ_VBAT_OFF 0
		#else
			#define GPIO_LEVEL_READ_VBAT_ON 0
			#define GPIO_LEVEL_READ_VBAT_OFF 1
		#endif
	#endif

	// VBAT charging status - from charging chip status - please see schematics
	// Note: it is tested in Lolin32 module with TP4054 charging chip
	// For another chip, please verify it
	
	#define PIN_SENSOR_CHARGING GPIO_NUM_17

#endif

// Install ISR service ?

#if defined PIN_BUTTON_STANDBY || defined PIN_SENSOR_VEXT || defined PIN_SENSOR_CHARGING

	#define INSTALL_ISR_SERVICE true

#endif

///// Output pins

// Led status pin - comment to disable this
// Can be a ESP32 board led (it in some boards is 5, anothers is 2)
// or extern led

#define PIN_LED_STATUS GPIO_NUM_5
//#define PIN_LED_STATUS GPIO_NUM_2

/////// Prototypes

void peripheralsInitialize();
void peripheralsFinalize();

void gpioBlinkLedStatus();

void adcRead();

//////// External variables

#ifdef HAVE_BATTERY
extern bool mGpioVEXT ;			// Powered by external voltage (USB or power supply) ?
extern bool mGpioChgBattery ;	// Charging battery ?
extern int16_t mAdcBattery;		// Voltage of battery by ADC
#endif

//////// Macros

#define gpioSetLevel(gpio_num, level) 	gpio_set_level(gpio_num, level)
#define gpioIsHigh(gpio_num) 			(gpio_get_level(gpio_num) == 1u)
#define gpioIsLow(gpio_num) 			(gpio_get_level(gpio_num) == 0u)
#define gpioDisableISR(gpio_num) 		gpio_isr_handler_remove(gpio_num)

#endif /* MAIN_PERIPHERALS_H_ */

//////// End