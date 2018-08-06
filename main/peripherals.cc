/* ***********
 * Project   : Esp-Idf-App-Mobile - Esp-Idf - Firmware on the Esp32 board - Ble
 * Programmer: Joao Lopes
 * Module    : peripherals - Routines to treat peripherals of Esp32, as gpio, adc, etc.
 * Versions  :
 * ------- 	-------- 	------------------------- 
 * 0.1.0 	01/08/18 	First version 
 */

/////// Includes

#include "esp_system.h"
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/adc.h"

// From the project

#include "main.h"

#include "peripherals.h"

// Utilities

#include "util/log.h"

#ifdef MEDIAN_FILTER_READINGS
	#include "util/median_filter.h"
#endif

/////// Variables

// Log

static const char* TAG = "peripherals";

#ifdef PIN_LED_ESP32
// Led Esp32 on?

static bool mLedEsp32On = false; 
#endif

// ADC reading average

#ifdef MEDIAN_FILTER_READINGS
static MedianFilter <uint16_t, MEDIAN_FILTER_READINGS> Filter;
#endif

/////// Prototype - Private

static void gpioInitialize();
static void gpioFinalize();

#ifdef INSTALL_ISR_SERVICE
static void IRAM_ATTR gpio_isr_handler (void * arg);
#endif

static void adcInitialize();
static uint16_t adcReadMedian (adc1_channel_t channelADC1);

////// Methods

/////// Routines for all peripherals

void peripheralsInitialize() {

	// Initialize all peripherals

	logD("Initializing peripherals ...");

	// Gpio

	gpioInitialize();

	// ADC

	adcInitialize();

	// Debug

	logD("Peripherals initialized");
	
}

void peripheralsFinalize() {

	// Finalize all peripherals 

	logD("Finalizing peripherals ...");

	// Gpio 

	gpioFinalize();

	// ADC

	//adcFinalize(); // ADC not needs finalize

	// Debug
	
	logD("Peripherals finalized");

}

/////// Routines for GPIO

static void gpioInitialize() {

	/////// Initializes GPIOs

	logD ("Initializing GPIO ...");

	//// Gpio config

	gpio_config_t config;

	//// Input pins

#ifdef PIN_BUTTON_STANDBY

	// Standby button for deep sleep

	config.pin_bit_mask = (1<<PIN_BUTTON_STANDBY);
	config.mode         = GPIO_MODE_INPUT;
	config.pull_up_en   = GPIO_PULLUP_DISABLE;
	config.pull_down_en = GPIO_PULLDOWN_ENABLE;
	config.intr_type    = GPIO_INTR_POSEDGE; // Pos edge: low -> high;

	gpio_config(&config);
#endif

#ifdef PIN_GROUND_VBAT

	// Pin to ground resistor divider to measure voltage of battery
	// To turn ground only when reading ADC and disable it in deep sleep
	// Soo consume nothing more during deep sleep

	config.pin_bit_mask = (1<<PIN_GROUND_VBAT);
	config.mode         = GPIO_MODE_OUTPUT;
	config.pull_up_en   = GPIO_PULLUP_DISABLE;
	config.pull_down_en = GPIO_PULLDOWN_ENABLE;
	config.intr_type    = GPIO_INTR_DISABLE;

	gpio_config(&config);

	gpioSetLevel (PIN_GROUND_VBAT, 1); // Not ground this - is only do in ADC readings
#endif

#ifdef PIN_SENSOR_VUSB

    // Digital sensor - VUSB - detects 5V

	config.pin_bit_mask = (1<<PIN_SENSOR_VUSB);
	config.mode         = GPIO_MODE_INPUT;
	config.pull_up_en   = GPIO_PULLUP_DISABLE;
	config.pull_down_en = GPIO_PULLDOWN_ENABLE;
	config.intr_type    = GPIO_INTR_ANYEDGE;// Any edge: low -> high or high -> low

	gpio_config(&config);

	// Charging via USB? (first reading, after this, is done via ISR)

	mChargingVUSB = gpioIsHigh (PIN_SENSOR_VUSB);

#endif

#ifdef INSTALL_ISR_SERVICE

	// ISR

	gpio_install_isr_service(0);

	#ifdef PIN_BUTTON_STANDBY
		gpio_isr_handler_add (PIN_BUTTON_STANDBY, gpio_isr_handler, (void *) PIN_BUTTON_STANDBY);
	#endif
	#ifdef PIN_SENSOR_VUSB
		gpio_isr_handler_add(PIN_SENSOR_VUSB, gpio_isr_handler, (void*) PIN_SENSOR_VUSB);
	#endif

#endif

    //// Output pins

#ifdef PIN_LED_ESP32

    // Led do ESP32

	config.pin_bit_mask = (1<<PIN_LED_ESP32);
	config.mode         = GPIO_MODE_OUTPUT;
	config.pull_up_en   = GPIO_PULLUP_DISABLE;
	config.pull_down_en = GPIO_PULLDOWN_DISABLE;
	config.intr_type = GPIO_INTR_DISABLE;

	gpio_config(&config);

	gpio_set_level(PIN_LED_ESP32, 1);
	mLedEsp32On = true;

#endif

	// Debug

	logD ("GPIO initalized");

}

static void gpioFinalize () {

	// Debug

	logD ("GPIO finalizing ...");

	// Finish the GPIO

#ifdef PIN_LED_ESP32

	// Turn off the Esp32 led

	gpioSetLevel(PIN_LED_ESP32, 0);

	mLedEsp32On = false;
#endif

#ifdef INSTALL_ISR_SERVICE

	// Disable ISRs

	#ifdef PIN_BUTTON_STANDBY

	gpioDisableISR (PIN_BUTTON_STANDBY);

	#endif

	#ifdef PIN_SENSOR_VUSB

	gpioDisableISR (PIN_SENSOR_VUSB);

	#endif

#endif

	// Debug

	logD ("GPIO finished");

}

#ifdef PIN_LED_ESP32

void gpioBlinkLedEsp32() {

	// Blink the board led

	mLedEsp32On = !mLedEsp32On;

	gpioSetLevel(PIN_LED_ESP32, mLedEsp32On);

}

#endif

//// Privates

#ifdef INSTALL_ISR_SERVICE

static void IRAM_ATTR gpio_isr_handler (void * arg) {

	// Interrupt handler of GPIOs

	uint32_t gpio_num = (uint32_t) arg;

	// Treat gpio

	switch (gpio_num) {

#ifdef PIN_BUTTON_STANDBY
		case PIN_BUTTON_STANDBY: // Standby?

			// Notify main_Task to enter standby - to not do it in ISR
			
			notifyMainTask(MAIN_TASK_ACTION_STANDBY, true);
			break;
#endif
#ifdef PIN_SENSOR_VUSB
		case PIN_SENSOR_VUSB: // Charging by VUSB

			// Charging by VUSB? (variable from main.cc)

			mChargingVUSB = gpioIsHigh(PIN_SENSOR_VUSB);

			break;
#endif
		default:
			break;
	}
}
#endif

/////// Routines for ADC

static void adcInitialize() {

	// Initializes ADC

	logD ("Initializing ADC ...");

	// ADC input pins (sensors)

#ifdef ADC_SENSOR_VBAT
	
	// Sensor VBAT 
	
	adc1_config_width(ADC_WIDTH_12Bit);

	adc1_config_channel_atten (ADC_SENSOR_VBAT, ADC_ATTEN_11db); // VBAT sensor - to identify the current battery voltage (VBAT)
#endif

	// Debug

	logD ("ADC Initialized");

}

// Reading the sensors by ADC

void adcRead() {

	// Read sensors

#ifdef ADC_SENSOR_VBAT 

	// VBAT voltage

	if (mChargingVUSB) {

		mSensorVBat = 0; // Not trust in readings on charging

	} else { 
		
#ifdef PIN_GROUND_VBAT

		// Pin to ground resistor divider to measure voltage of battery
		// To turn ground only when reading ADC 

		gpioSetLevel (PIN_GROUND_VBAT, 0); // Ground this 

#endif

		// Read ADC

		mSensorVBat = adcReadMedian (ADC_SENSOR_VBAT);

#ifdef PIN_GROUND_VBAT
		gpioSetLevel (PIN_GROUND_VBAT, 1); // Not ground this - no consupmition of battery 
#endif
	}
#endif

}

////// Private

static uint16_t adcReadMedian (adc1_channel_t channelADC1) {

	// Average reading - ADC

	for (uint8_t i = 0; i <MEDIAN_FILTER_READINGS; i ++) {

		Filter.set(i, ::adc1_get_raw(channelADC1));
	}

	uint16_t median;
	Filter.getMedian(median);
	return median;
}

////////// End
