/* ***********
 * Project   : Esp-Idf-App-Mobile - Esp-Idf - Firmware on the Esp32 board - Ble
 * Programmer: Joao Lopes 
 * Module    : Main - main module
 * Versions  :
 * ------- 	-------- 	------------------------- 
 * 0.1.0 	01/08/18 	First version 
 **/

/*
 * TODO list: 
 */ 

/** 
 * Bluetooth messages 
 * ----------------------
 * Format: nn:payload (where nn is code of message and payload is content, can be delimited too) 
 * --------------------------- 
 * Messages codes:
 * 01 Start 
 * 02 Version 
 * 03 Power status(USB or Battery?) 
 * 70 Echo debug
 * 80 Feedback 
 * 98 Reinitialize
 * 99 Standby (enter in deep sleep)
 *
 * // TODO: see it! - keep it update
 **/

///// Includes

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "soc/soc.h"

// C++

#include <string>
using namespace std;

// Util

#include "util/log.h"
#include "util/esp_util.h"
#include "util/fields.h"

// Do projeto

#include "main.h"
#include "ble.h"
#include "peripherals.h"

////// Prototypes

static void main_Task(void *pvParameters) ;
static void standby(const char*	cause) ;
static void debugInitial();
static void checkEnergyVoltage (bool sendingStatus);

////// Variables 

// Log 

static const char *TAG = "main";

// Utility 

static Esp_Util &Util = Esp_Util::getInstance(); 

// Times and intervals 

uint32_t mTimeSeconds = 0; 			// Current time in seconds (for timeouts calculations)

uint32_t mLastTimeReceivedData = 0; // time of receipt of last line via

uint32_t mLastTimeFeedback = 0; 	// Indicates the time of the last feedback message

// Log active (debugging)? 

bool mLogActive = false;

// Connected? 

bool mAppConnected = false; // Indicates connection when receiving message 01: 

/// Sensors

#ifdef HAVE_BATTERY
bool mChargingVUSB = false;	// Charging by USB ?
int16_t mSensorVBat = 0;	// Sensor de voltage of battery
#endif

////// FreeRTOS

// Task main

static TaskHandle_t xTaskMainHandler = NULL;

////// Main

#ifndef ARDUINO // Not for Arduino
extern "C" {
	void app_main();
}
#endif

/**
 * @brief app_main of ESP-IDF 
 */
void app_main()
{

	mLogActive = true; // To show initial messages

	logI("Initializing");  

	// Initialize the Esp32 

	Util.esp32Initialize();

	// Initialize Peripherals

	peripheralsInitialize();

	// Initialize Ble Server 

	bleInitialize();

	// Task -> Initialize task_main in core 1, if is possible

	xTaskCreatePinnedToCore (&main_Task,
				"main_Task", TASK_STACK_LARGE, NULL, TASK_PRIOR_HIGH, &xTaskMainHandler, TASK_CPU);

	// Logging

#ifdef HAVE_BATTERY
	//mLogActive = mChargingVUSB; // Activate only if plugged in USB - comment it to keep active
#endif

    return; 
} 

/**
 * @brief Main Task - main processing 
 */
static void main_Task (void * pvParameters) { 

	logI ("Starting main Task"); 

	/////// Variables 

	// Init the time 
	
	mTimeSeconds = 0; 

	/////// Initializations 

	// Initializes app

	appInitialize (false);

	////// FreeRTOS 

	// Timeout - task - 1 second

	const TickType_t xTicks = (1000u / portTICK_RATE_MS);

	////// Loop 

	uint32_t notify; // Notify variable 

	for (;;) {

		// Init notification 
		
		notify = 0; 

		// Wait for the time or or something notified (seen in the FreeRTOS example) 

		if (xTaskNotifyWait (0, 0xffffffff, &notify, xTicks) == pdPASS) { 

			// Action by task notification

			logD ("Notification received -> %d", notify); 

			bool reset_timer = false;

			switch (notify) {

				case MAIN_TASK_ACTION_RESET_TIMER: 	// Reset timer
					reset_timer = true;
					break;

				case MAIN_TASK_ACTION_STANDBY: 		// Enter in standby - to deep sleep not run in ISR
					standby("Pressed button standby");
					break;

				// TODO: see it! If need put here your custom notifications

				default:
					break;
			}

			// Resets the time variables and returns to the beginning of the loop ?
			// Usefull to initialize the time (for example, after App connection)

			if (reset_timer) {

				// Clear variables

				mTimeSeconds = 0;
				mLastTimeFeedback = 0;
				mLastTimeReceivedData = 0;

				// TODO: see it! put here custom reset timer code 

				// Debug

				logD("Reseted time");

				continue; // Returns to loop begin

			}
		}

		////// Processes every second 

		// Time counter 

		mTimeSeconds++; 

#ifdef PIN_LED_ESP32
		// Blink the board led of Esp32

		gpioBlinkLedEsp32();
#endif

		// Sensors readings by ADC

#ifdef HAVE_BATTERY
		bool lastChargingVUSB = mChargingVUSB;
#endif
		adcRead();

		// TODO: see it! Put here your custom code to run every second

		// Debug

		if (mLogActive) {

			if (mTimeSeconds % 5 == 0) { // Debug each 5 secs

#ifdef HAVE_BATTERY
				logD("* Secs=%d | sensors: vusb=%c vbat=%d | mem=%d",
							mTimeSeconds,
							((mChargingVUSB)?'Y':'N'), mSensorVBat,
							esp_get_free_heap_size());
#else
				logD("* Secs=%d", mTimeSeconds);
#endif
			} else { // Verbose

				logV("* Secs=%d", mTimeSeconds);

			}

		}

#ifdef MAX_TIME_INACTIVE
		////// Auto power off (standby) 
		// If it has been inactive for the maximum time allowed, it goes into standby (soft off) 

#ifdef HAVE_BATTERY
		bool verifyInactive = !mChargingVUSB; // Only if it is not charging - to not abort debuggings;
#else
		bool verifyInactive = true;
#endif
		if (verifyInactive) { // Verify it ?

			bool inactive = false;

			if (bleConnected ()) { 
				inactive = ((mTimeSeconds - mLastTimeReceivedData) >= MAX_TIME_INACTIVE);
			} else { 
				inactive = (mTimeSeconds >= MAX_TIME_INACTIVE);
			} 

			if (inactive) { 

				// Set to standby (soft off) 

				standby ("Attained maximum time of inactivity"); 
				return; 

			} 
		} 
#endif
		/////// BLE connected ?

		if (!bleConnected()) {
			continue;
		}

		/////// Routines with only if BLE is connected

		// Check timeouts in BLE 

		bleVerifyTimeouts();

#if HAVE_BATTERY
		// Check the voltage of battery/charging

		if ((mTimeSeconds % 60 == 0) || 				// Each minute
				(mChargingVUSB != lastChargingVUSB)) { 	// or changed VUSB

			checkEnergyVoltage(false);

		}
#endif

#ifdef MAX_TIME_WITHOUT_FB
		// If not received feedback message more than the allowed time

		if (!mLogActive) { // Only if it is not debugging

			if ((mTimeSeconds - mLastTimeFeedback) >= MAX_TIME_WITHOUT_FB) {

				// Enter in standby (soft off)

				standby ("No feedback received in time");
				return;

			} 
		}
#endif

		// TODO: see it! put here custom routines for when BLE is connected		

	} 

	////// End 

	logI ("End of main task");

	// Delete this task 

	vTaskDelete (NULL); 
	xTaskMainHandler = NULL;

}

/**
 * @brief Initializes the app
 */
void appInitialize(bool resetTimerSeconds) {

	logD ("Initializing ..."); 

	///// Initialize the variables

	// Initialize times 

	mLastTimeReceivedData = 0;
	mLastTimeFeedback = 0;

	// TODO: see it! Please put here custom global variables or code for init

	// Debugging

	logD ("Initialized");

} 

/**
 * @brief Process the message received from BLE
 */
void processBleMessage (const string &message) {

	// This is to process ASCII (text) messagens - not binary ones

	string response = "00:OK"; // Return default to indicate everything OK

	// --- Process the received line 

	// Check the command

	if (message.length () <2 ) {

		error("Message length must have 2 or more characters");
		return; 

	} 

	// Process fields of the message 

	Fields fields(message, ":");

	// Code of the message 

	uint8_t code = 0;

	if (!fields.isNum (0)) { // Not numerical
		error ("Non-numeric message code"); 
		return; 
	} 

	code = fields.getInt (0);

	if (code == 0) { // Invalid code
		error ("Invalid message code"); 
		return; 
	} 

	logV("Code -> %u Message -> %s", code, Util.strExpand (message).c_str());

	// Considers the message received as feedback also 

	mLastTimeFeedback = mTimeSeconds;

	// Process the message 

	bool sendEnergy = false; // Send response with the energy situation? 

	switch (code) { // Note: the '{' and '}' in cases, is to allow to create local variables, else give an error cross definition ... 

	// --- Initial messages 

	case 1: // Start 
		{
			// Initial message sent by the mobile application, to indicate start of the connection

			if (mLogActive) { 
				debugInitial();
			}

			// Reinicialize the app - include timer of seconds

			appInitialize(true);

			// Indicates connection initiated by the application 

			mAppConnected = true;

			// Send energy status (also if this project not have battery, to mobile app know it)

			sendEnergy = true;

			// Debug

			bool turnOnDebug = false;

#ifdef HAVE_BATTERY

			// Turn on the debugging (if the USB cable is connected)

			if (mChargingVUSB && !mLogActive) {
				turnOnDebug = true; 
			} 
#else

			// Turn on debugging

			turnOnDebug = !mLogActive;

#endif

			// Turn on the debugging (if the USB cable is connected)

			if (turnOnDebug) {

				mLogActive = true; 
				debugInitial();
			} 

			// Reset the time in main_Task

			notifyMainTask(MAIN_TASK_ACTION_RESET_TIMER);
		}
		break; 
	
	case 2: // Firmware version
		{
			response = "02:";
			response.append(FW_VERSION);
		}
		break; 
	case 3: // Status of energy (battery or VUSB)
		{
			sendEnergy = true;
			response = "";
		}
		break; 
	
	// TODO: see it! Please put here custom menssages

	case 70: // Echo (for test purpose)
		{
			response = message;
		}
		break; 

	case 80: // Feedback 
		{
			// Message sent by the application periodically, for connection verification

			logV("Feedback recebido");
		}
		break; 

	case 98: // Reinicialize the app
		{
			logI ("Reinitialize");

			response = "";

			// End code placed at the end of this routine to send OK before 
		}
		break; 

	case 99: // Enter in standby
		{
			logI ("Entering in standby");

			response = "";

			// End code placed at the end of this routine to send OK before 
		}
		break; 

	default: 

		error ("Type of message invalid"); 
		return;

	}

	// return 

	if (response.size() > 0) {

		// Return -> Send message response

		if (bleConnected ()) { 
			bleSendData(response);
		} 
	} 

	// Return energy situation too? 

	if (sendEnergy) { 

		checkEnergyVoltage (true);

	} 

	// Mark the mTimeSeconds of the receipt 

	mLastTimeReceivedData = mTimeSeconds;

	// Here is processed messages that have actions to do after response sended

	switch (code) {

	case 98: // Reinicialize the app

		appInitialize (true);
		break; 

	case 99: // Standby - enter in deep sleep

#ifdef PIN_BUTTON_STANDBY

		// Turn on the status LED 

		// Wait 500 ms, to give mobile app time to quit 

		delay (500); 

		// Off - enters standby 

		standby ("99 code msg - standby");

#else

		// No have standby - reinitialize

		appInitialize(true);

#endif

		break; 

	} 

} 

/**
 * @brief Check the voltage of the power supply (battery or charging via usb) 
 * Used also if this project not have battery, to mobile app know that)
 */
static void checkEnergyVoltage (bool sendingStatus) {

	string energy = "";

#ifdef HAVE_BATTERY

	static int16_t lastReadingVBAT = 0;

	// Volts in the power supply (battery) of the ESP32 via the ADC pin 
	// There is one resistive divider

	uint16_t readVBAT = mSensorVBat; // Read in adc.cc

	// Send the status to the application 

	int16_t diffAnalog = (readVBAT - lastReadingVBAT);
	if (diffAnalog < 0) diffAnalog *= -1;

	// Send the status of the application when requested or when there were alterations 

	if (bleConnected() &&										// Only if connected
			(sendingStatus ||									// And sending status
					(!mChargingVUSB && diffAnalog > 20))) {		// Or significative diff

		logD("vbat=%u diff=%u", readVBAT, diffAnalog);

		// Message to App

		energy="03:";
		energy.append ((mChargingVUSB)? "VUSB:": "BAT:");
		energy.append (Util.intToStr(readVBAT));
		energy.append (1u, ':'); 

	}

#else // Without battery

	if (bleConnected()) {

		// Message to App

		energy="03:VUSB:0:";

	}

#endif

	// Send it to app ?

	if (energy.size() > 0) {

		bleSendData(energy);
	}

	// Save last readings

#ifdef HAVE_BATTERY
	lastReadingVBAT = readVBAT;
#endif

} 

/**
 * @brief Standby - enter in deep sleep
 */
static void standby (const char *cause) {

	// Enter in standby (standby off is reseting ESP32)
	// Yet only support a button to control it, touchpad will too in future

	// Debug

	logD ("Entering standby, cause -> %s", cause);

#ifdef PIN_BUTTON_STANDBY

	// Disable interrupt on gpio

	gpioDisableISR(PIN_BUTTON_STANDBY);

	// Send message to app mobile

	if (mAppConnected && bleConnected()) {

		// Send the cause of the standby 

		string message = "44:";
		message.append (cause); 

		if (bleConnected ()) { 

			bleSendData(message);

			delay (500); 

		} 

	} 

#ifdef PIN_GROUND_VBAT
	// Pin to ground resistor divider to measure voltage of battery
	// To consume nothing more during deep sleep

	gpioSetLevel (PIN_GROUND_VBAT, 0); // Not ground this more
#endif

	////// Entering standby 

	// Finalize BLE

	bleFinalize();

	// Finalize the peripherals

	peripheralsFinalize();

	// A delay time

	delay(200);

	// Waiting for button to be released

	logD ("Waiting for button to be released ..."); 

	while (gpioIsHigh (PIN_BUTTON_STANDBY)) {
		delay (10); 
	} 

	// Enter the Deep Sleep of ESP32, and exit only if the button is pressed 

	esp_sleep_enable_ext0_wakeup (PIN_BUTTON_STANDBY, 1); // 1 = High, 0 = Low

	logI ("Entering deep sleep ...");

	esp_deep_sleep_start (); // TODO: hibernate ???

#else 

	logI ("Do not enter deep-sleep - pin not set - rebooting ..."); 

	appInitialize (true); 
#endif 
} 

/**
 * @brief Show error and notifies the application error occurred
 */
void error (const char *message, bool fatal) {

	// Debug

	logE("Error -> %s", message);

	// Send the message 

	if (bleConnected ()) { 
		string error = "-1:"; // -1 is a code of error messages
		error.append (message);
		bleSendData(message);
	} 

	// Fatal ?

	if (fatal) {

		// Wait a time

		delay (200);

		// Restart ESP32

		restartESP32 ();

	}

} 

/**
 * @brief Reset the ESP32
 */
void restartESP32 () { 

	// TODO: see it! if need, put your custom code here 

	// Reinitialize 

	esp_restart (); 

} 

/**
 * @brief Initial Debugging 
 */
static void debugInitial () {

	logV ("Debugging is on now");

	logV ("Firmware device version: %s", FW_VERSION);

} 

/**
 * @brief Cause an action on main_Task by task notification
 */
void IRAM_ATTR notifyMainTask(uint32_t action, bool fromISR) {

	// Debug (for non ISR only) 

	if (!fromISR) {
		logD ("action=%u", action);
	} 

	// Notify the main task

	if (fromISR) {// From ISR
		BaseType_t xHigherPriorityTaskWoken;
		xTaskNotifyFromISR (xTaskMainHandler, action, eSetValueWithOverwrite, &xHigherPriorityTaskWoken);
	} else {
		xTaskNotify (xTaskMainHandler, action, eSetValueWithOverwrite);
	}

}

//////// End
