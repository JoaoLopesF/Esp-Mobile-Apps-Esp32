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
#include "util/esp_util.h"

#ifdef MEDIAN_FILTER_READINGS
	#include "util/median_filter.h"
#endif

/////// Variables

// Log

static const char* TAG = "peripherals";

#ifdef PIN_LED_STATUS
// Led status on?

static bool mLedStatusOn = false; 
#endif

// ADC reading average

#ifdef MEDIAN_FILTER_READINGS
static MedianFilter <uint16_t, MEDIAN_FILTER_READINGS> Filter;
#endif

/// Sensors

#ifdef HAVE_BATTERY
bool mGpioVEXT = false;			// Powered by external voltage (USB or power supply)
bool mGpioChgBattery = false;	// Charging battery ?
int16_t mAdcBattery = 0;		// voltage of battery readed by ADC
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

/**
 * @brief Initialize all peripherals
 */
void peripheralsInitialize() {

	logD("Initializing peripherals ...");

	// Gpio

	gpioInitialize();

	// ADC

	adcInitialize();

	// Debug

	logD("Peripherals initialized");
	
}

/**
 * @brief Finalize all peripherals 
 */
void peripheralsFinalize() {

	logD("Finalizing peripherals ...");

	// Gpio 

	gpioFinalize();

	// ADC

	//adcFinalize(); // ADC not needs finalize

	// Debug
	
	logD("Peripherals finalized");

}

/////// Routines for GPIO

/**
 * @brief Initializes GPIOs
 */
static void gpioInitialize() {

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

#ifdef PIN_SENSOR_VEXT

    // Digital sensor - VEXT - detects an external voltage (USB or power supply)

	config.pin_bit_mask = (1<<PIN_SENSOR_VEXT);
	config.mode         = GPIO_MODE_INPUT;
	config.pull_up_en   = GPIO_PULLUP_DISABLE;
	config.pull_down_en = GPIO_PULLDOWN_ENABLE;
	config.intr_type    = GPIO_INTR_ANYEDGE;// Any edge: low -> high or high -> low

	gpio_config(&config);

	// Energy by USB (or external source)? (first reading, after this, is done via ISR)

	mGpioVEXT = gpioIsHigh (PIN_SENSOR_VEXT);

#endif

#ifdef PIN_SENSOR_CHARGING

    // Digital sensor - detect if is charging the battery

	config.pin_bit_mask = (1<<PIN_SENSOR_CHARGING);
	config.mode         = GPIO_MODE_INPUT;
	config.pull_up_en   = GPIO_PULLUP_DISABLE;
	config.pull_down_en = GPIO_PULLDOWN_ENABLE;
	config.intr_type    = GPIO_INTR_ANYEDGE;// Any edge: low -> high or high -> low

	gpio_config(&config);

	// Charging now? (first reading, after this, is done via ISR)

	mGpioChgBattery = gpioIsLow (PIN_SENSOR_CHARGING);

#endif

#ifdef INSTALL_ISR_SERVICE

	// ISR

	gpio_install_isr_service(0);

	#ifdef PIN_BUTTON_STANDBY
		gpio_isr_handler_add (PIN_BUTTON_STANDBY, gpio_isr_handler, (void *) PIN_BUTTON_STANDBY);
	#endif
	#ifdef PIN_SENSOR_VEXT
		gpio_isr_handler_add(PIN_SENSOR_VEXT, gpio_isr_handler, (void*) PIN_SENSOR_VEXT);
	#endif
	#ifdef PIN_SENSOR_CHARGING
		gpio_isr_handler_add(PIN_SENSOR_CHARGING, gpio_isr_handler, (void*) PIN_SENSOR_CHARGING);
	#endif

#endif

    //// Output pins

#ifdef PIN_LED_STATUS

    // Led of status (can be a board led of ESP32 or external)

	config.pin_bit_mask = (1<<PIN_LED_STATUS);
	config.mode         = GPIO_MODE_OUTPUT;
	config.pull_up_en   = GPIO_PULLUP_DISABLE;
	config.pull_down_en = GPIO_PULLDOWN_DISABLE;
	config.intr_type = GPIO_INTR_DISABLE;

	gpio_config(&config);

	gpio_set_level(PIN_LED_STATUS, 1);
	mLedStatusOn = true;

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

	gpioSetLevel (PIN_GROUND_VBAT, GPIO_LEVEL_READ_VBAT_OFF); // Not ground this, is only do in ADC readings

#endif

	// Debug

	logD ("GPIO initalized");

}

/**
 * @brief Finish the GPIO
 */
static void gpioFinalize () {

	// Debug

	logD ("GPIO finalizing ...");

#ifdef PIN_LED_STATUS

	// Turn off the led of status

	gpioSetLevel(PIN_LED_STATUS, 0);

	mLedStatusOn = false;
#endif

#ifdef INSTALL_ISR_SERVICE
	// Disable ISRs

	#ifdef PIN_BUTTON_STANDBY
	gpioDisableISR (PIN_BUTTON_STANDBY);
	#endif

	#ifdef PIN_SENSOR_VEXT
	gpioDisableISR (PIN_SENSOR_VEXT);
	#endif

	#ifdef PIN_SENSOR_CHARGING
	gpioDisableISR (PIN_SENSOR_CHARGING);
	#endif

#endif

	// Debug

	logD ("GPIO finished");

}

#ifdef PIN_LED_STATUS
/**
 * @brief Blink the status led
 */
void gpioBlinkLedStatus() {

	mLedStatusOn = !mLedStatusOn;

	gpioSetLevel(PIN_LED_STATUS, mLedStatusOn);

}

#endif

//// Privates

#ifdef INSTALL_ISR_SERVICE

/**
 * @brief Interrupt handler of GPIOs
 */
static void IRAM_ATTR gpio_isr_handler (void * arg) {

	uint32_t gpioNum = (uint32_t) arg;

	// Debounce events 

	static uint32_t lastTime = 0;		// Last event time
	static uint32_t lastGpioNum = 0; 	// Last GPIO 

	bool ignore = false;				// Ignore event ?

	// Treat gpio

	switch (gpioNum) {

#ifdef PIN_BUTTON_STANDBY
		case PIN_BUTTON_STANDBY: // Standby?

			// Debounce

			if (lastGpioNum == PIN_BUTTON_STANDBY && lastTime > 0) {

				if ((millis() - lastTime) < 50) {
					ignore=true;
				}

			}

			if (!ignore) {

				// Notify main_Task to enter standby - to not do it in ISR
				
				notifyMainTask(MAIN_TASK_ACTION_STANDBY_BTN, true);
			}
			break;
#endif

#ifdef PIN_SENSOR_VEXT
		case PIN_SENSOR_VEXT: // Powered by external voltage (USB or power supply)

			// Not need debounce

			// Powered by VEXT? 

			mGpioVEXT = gpioIsHigh(PIN_SENSOR_VEXT);

			// Notify main_Task that powered by VEXT is changed - to not do it in ISR
			
			notifyMainTask(MAIN_TASK_ACTION_SEN_VEXT, true);

			break;
#endif

#ifdef PIN_SENSOR_CHARGING
		case PIN_SENSOR_CHARGING: // Charging battery

			// Without debounce - due not notification used more - this is done in main_Task
			//                    (due if no battery plugged, the notification not ok)
			// // Debounce - it is important, due if no battery plugged, the value change fast

			// if (lastGpioNum == PIN_SENSOR_CHARGING && lastTime > 0) {

			// 	if ((millis() - lastTime) < 50) {
			// 		ignore=true;
			// 	}

			// }

			// Charging now ? 

			mGpioChgBattery = gpioIsLow(PIN_SENSOR_CHARGING);

			// No notification 
			// if (!ignore) {
			// 
			// 	// Notify main_Task that charging is changed - to not do it in ISR
			//	
			// 	notifyMainTask(MAIN_TASK_ACTION_SEN_CHGR, true);
			// }
			break;
#endif
		default:
			break;
	}

	// Save this

	lastTime = millis();
	lastGpioNum = gpioNum;
}

#endif

/////// Routines for ADC

/**
 * @brief Initializes ADC
 */
static void adcInitialize() {

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

/**
 * @brief Reading the sensors by ADC
 */
void adcRead() {

	// Read sensors

#if defined HAVE_BATTERY && defined ADC_SENSOR_VBAT 

	// VBAT voltage
		
	#ifdef PIN_GROUND_VBAT

		// Pin to ground resistor divider to measure voltage of battery
		// To turn ground only when reading ADC 

		gpioSetLevel (PIN_GROUND_VBAT, GPIO_LEVEL_READ_VBAT_ON); // Ground this 

	#endif

	// Read ADC

	mAdcBattery = adcReadMedian (ADC_SENSOR_VBAT);

	#ifdef PIN_GROUND_VBAT
		gpioSetLevel (PIN_GROUND_VBAT, GPIO_LEVEL_READ_VBAT_OFF); // Not ground this - no consupmition of battery 
	#endif
	
#endif

}

////// Private

/**
 * @brief Average reading - ADC
 */
 
static uint16_t adcReadMedian (adc1_channel_t channelADC1) {

	for (uint8_t i = 0; i <MEDIAN_FILTER_READINGS; i ++) {

		Filter.set(i, ::adc1_get_raw(channelADC1));
	}

	uint16_t median;
	Filter.getMedian(median);
	return median;
}

////////// End
