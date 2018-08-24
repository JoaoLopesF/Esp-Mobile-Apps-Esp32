/* ***********
 * Project   : Esp-Idf-App-Mobile - Esp-Idf - Firmware on the Esp32 board - Ble
 * Programmer: Joao Lopes 
 * Module    : Main - main module
 * Versions  :
 * ------- 	-------- 	------------------------- 
 * 0.1.0 	01/08/18 	First version 
 * 0.1.1	20/08/18	BLE task for event can be disabled now
 * 0.2.0	20/08/18	Option to disable logging br BLE (used during repeated sends)
 * 0.3.0	23/08/18	Adjustments to allow sizes of BLE > 255
 * 						BLE has a queue now to receive data
 * 						Need when have more 1 message, to avoid empty string on event
 * 						Changed name of github repos to Esp-App-Mobile-Apps-*
 **/

/**
 * BLE text messages of this app
 * -----------------------------
 * Format: nn:payload
 * (where nn is code of message and payload is content, can be delimited too)
 * -----------------------------
 * Messages codes:
 * 01 Initial
 * 10 Energy status(External or Battery?)
 * 11 Informations about ESP32 device
 * 70 Echo debug
 * 71 Logging (to activate or not)
 * 80 Feedback
 * 98 Restart (reset the ESP32)
 * 99 Standby (enter in deep sleep)
 *
 * // TODO: see it! please remove that you not use and keep it updated
 **/

/*
 * TODO list: 
 * ----------
 * -
 * -
 * -
 */ 

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

// C

extern "C" {
	int rom_phy_get_vdd33();
}

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
static void standby(const char*	cause, bool sendBLEMsg) ;
static void debugInitial();
static void sendInfo(Fields& fields);

#ifdef HAVE_BATTERY
static void checkEnergyVoltage (bool sendingStatus);
#endif

////// Variables 

// Log 

static const char *TAG = "main";

// Utility 

static Esp_Util& mUtil = Esp_Util::getInstance(); 

// Times and intervals 

uint32_t mTimeSeconds = 0; 			// Current time in seconds (for timeouts calculations)

uint32_t mLastTimeFeedback = 0; 	// Indicates the time of the last feedback message

uint32_t mLastTimeReceivedData = 0; // time of receipt of last line via

// Log active (debugging)? 

bool mLogActive = false;
bool mLogActiveSaved = false;

// Connected? 

bool mAppConnected = false; // Indicates connection when receiving message 01: 

////// FreeRTOS

// Task main

static TaskHandle_t xTaskMainHandler = NULL;

////// Main

extern "C" {
	void app_main();
}

/**
 * @brief app_main of ESP-IDF 
 */
void app_main()
{

	mLogActive = true; // To show initial messages

	logI("Initializing");  

	// Initialize the Esp32 

	mUtil.esp32Initialize();

	// Initialize Peripherals

	peripheralsInitialize();

	// Initialize Ble Server 

	bleInitialize();

	// Task -> Initialize task_main in core 1, if is possible

	xTaskCreatePinnedToCore (&main_Task,
				"main_Task", TASK_STACK_LARGE, NULL, TASK_PRIOR_HIGH, &xTaskMainHandler, TASK_CPU);

	// Logging

#ifdef HAVE_BATTERY
	//mLogActive = mGpioVEXT; // Activate only if plugged in Powered by external voltage (USB or power supply) - comment it to keep active
#endif

    return; 
} 

/**
 * @brief Main Task - main processing 
 * Is a second timer to process, adquiry data, send responses to mobile App, control timeouts, etc.
 */
static void main_Task (void * pvParameters) { 

	logI ("Starting main Task"); 

	/////// Variables 

	// Init the time 
	
	mTimeSeconds = 0; 

	/////// Initializations 

	// Initializes app

	appInitialize (false);

	// Sensors values

#if defined HAVE_BATTERY && defined PIN_SENSOR_CHARGING
	// Use this to verify changes on this sensor
	// Due debounce logic
	bool lastChgBattery = mGpioChgBattery; 
#endif

	////// FreeRTOS 

	// Timeout - task - 1 second

	const TickType_t xTicks = (1000u / portTICK_RATE_MS);

	////// Loop 

	uint32_t notification; // Notification variable 

	for (;;) {

		// Wait for the time or something notified (seen in the FreeRTOS example) 

		if (xTaskNotifyWait (0, 0xffffffff, &notification, xTicks) == pdPASS) { 

			// Action by task notification

			logD ("Notification received -> %u", notification); 

			bool reset_timer = false;

			switch (notification) {

				case MAIN_TASK_ACTION_RESET_TIMER: 	// Reset timer
					reset_timer = true;
					break;

				case MAIN_TASK_ACTION_STANDBY_BTN: 	// Enter in standby - to deep sleep not run in ISR
					standby("Pressed button standby", true);
					break;

				case MAIN_TASK_ACTION_STANDBY_MSG: 	// Enter in standby - to deep sleep not run in ISR
					standby ("99 code msg - standby", false);
					break;
#ifdef HAVE_BATTERY
				case MAIN_TASK_ACTION_SEN_VEXT: 	// Sensor of Powered by external voltage (USB or power supply) is changed - to not do it in ISR

					checkEnergyVoltage(true);			
					break;

	#ifdef MAIN_TASK_ACTION_SEN_CHGR
				case MAIN_TASK_ACTION_SEN_CHGR: 	// Sensor of battery charging is changed - to not do it in ISR

					checkEnergyVoltage(true);			
					break;
	#endif
#endif
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

#ifdef PIN_LED_STATUS
		// Blink the led of status (board led of Esp32 or external)

		gpioBlinkLedStatus();
#endif

		// Sensors readings by ADC

		adcRead();

		// TODO: see it! Put here your custom code to run every second

		// Debug

		if (mLogActive) {

			if (mTimeSeconds % 5 == 0) { // Debug each 5 secs

#ifdef HAVE_BATTERY
				logD("* Secs=%d | sensors: vext=%c charging=%c vbat=%d | mem=%d",
							mTimeSeconds,
							((mGpioVEXT)?'Y':'N'), 
							((mGpioChgBattery)?'Y':'N'), 
							mAdcBattery,
							esp_get_free_heap_size());
#else
				logD("* Time seconds=%d", mTimeSeconds);
#endif
			} else { // Verbose //TODO: see it! put here that you want see each second
							    // If have, please add our variables and uncomment it
				// logV("* Time seconds=%d", mTimeSeconds);

			}
		}

#if defined MAX_TIME_INACTIVE && defined HAVE_S

		////// Auto power off (standby) 
		// If it has been inactive for the maximum time allowed, it goes into standby (soft off) 

#ifdef HAVE_BATTERY
		bool verifyInactive = !mGpioVEXT; // Only if it is not powered by external voltage (USB or power supply) - to not abort debuggings;
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

				standby ("Attained maximum time of inactivity", true); 
				return; 

			} 
		} 
#endif
		/////// BLE connected ?

		if (!bleConnected()) {
			continue;
		}

		/////// Routines with only if BLE is connected

#if HAVE_BATTERY

		// Check the voltage of battery/charging

		#ifdef PIN_SENSOR_CHARGING
				// Verify if it is changed (due debounce logic when no battery plugged)
				if (mGpioChgBattery != lastChgBattery) {
					checkEnergyVoltage(true);
				}
		#endif

		if ((mTimeSeconds % 60) == 0) { // Each minute

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

		// Sensors values saving

#if defined HAVE_BATTERY && defined PIN_SENSOR_CHARGING

		lastChgBattery = mGpioChgBattery; 
#endif

		// TODO: see it! put here custom routines for when BLE is connected		

	} 

	////// End 

	// Delete this task 

	vTaskDelete (NULL); 
	xTaskMainHandler = NULL;

}

/**
 * @brief Initializes the app
 */
void appInitialize(bool resetTimerSeconds) {

	// Restore logging ?

	if (mLogActiveSaved && !mLogActive) {
		mLogActive = true; // Restore state
	}

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
 * Note: this routine is in main.cc due major resources is here
 */
void processBleMessage (const string& message) {

	// This is to process ASCII (text) messagens - not binary ones

	string response = ""; // Return response to mobile app

	// --- Process the received line 

	// Check the message

	if (message.length () < 2 ) {

		error("Message length must have 2 or more characters");
		return; 

	} 

	// Process fields of the message 

	Fields fields(message, ":");

	// Code of the message 

	uint8_t code = 0;

	if (!fields.isNum (1)) { // Not numerical
		error ("Non-numeric message code"); 
		return; 
	} 

	code = fields.getInt (1);

	if (code == 0) { // Invalid code
		error ("Invalid message code"); 
		return; 
	} 

	logV("Code -> %u Message -> %s", code, mUtil.strExpand (message).c_str());

	// Considers the message received as feedback also 

	mLastTimeFeedback = mTimeSeconds;

	// Process the message 

#ifdef HAVE_BATTERY
	bool sendEnergy = false; // Send response with the energy situation too? 
#endif

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

			// Inform to mobile app, if this device is battery powered and sensors 
			// Note: this is important to App works with differents versions or models of device

#ifdef HAVE_BATTERY

			// Yes, is a battery powered device

			string haveBattery = "Y";
			
#ifdef PIN_SENSOR_CHARGING

			// Yes, have a sensor of charging
			string sensorCharging = "Y";

#else
			// No have a sensor of charging
			string sensorCharging = "N";
#endif
			// Send energy status (also if this project not have battery, to mobile app know it)

			sendEnergy = true;

#else

			// No, no is a battery powered device

			string haveBattery = "N";
			string sensorCharging = "N";

#endif
			// Debug

			bool turnOnDebug = false;

#ifdef HAVE_BATTERY

			// Turn on the debugging (if the USB is connected)

			if (mGpioVEXT && !mLogActive) {
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

			// Returns status of device, this firware version and if is a battery powered device

			response = "01:";
			response.append(FW_VERSION);
			response.append(1u, ':');
			response.append(haveBattery);
			response.append(1u, ':');
			response.append(sensorCharging);
			
		}
		break; 
	
#ifdef HAVE_BATTERY
	case 10: // Status of energy: battery or external (USB or power supply)
		{
			sendEnergy = true;
		}
		break; 
#endif
	
	case 11: // Request of ESP32 informations
		{
			// Example of passing fields class to routine process

			sendInfo(fields);

		}
		break;

	// TODO: see it! Please put here custom messages

	case 70: // Echo (for test purpose)
		{
			response = message;
		}
		break; 

	case 71: // Logging - activate or desactivate debug logging - save state to use after
		{
			switch (fields.getChar(2)) // Process options
			{
				case 'Y': // Yes

					mLogActiveSaved = mLogActive; // Save state
					mLogActive = true; // Activate it

					logV("Logging activated now");
					break;

				case 'N': // No

					logV("Logging deactivated now");

					mLogActiveSaved = mLogActive; // Save state
					mLogActive = false; // Deactivate it
					break;

				case 'R': // Restore

					mLogActive = mLogActiveSaved; // Restore state
					logV("Logging state restored now");
					break;
			}
		}
		break; 

	case 80: // Feedback 
		{
			// Message sent by the application periodically, for connection verification

			logV("Feedback recebido");

			// Response it (put here any information that needs)

			response = "80:"; 
		}
		break; 

	case 98: // Reinicialize the app
		{
			logI ("Reinitialize");

			// End code placed at the end of this routine to send OK before 
		}
		break; 

	case 99: // Enter in standby
		{
			logI ("Entering in standby");

			// End code placed at the end of this routine to send OK before 
		}
		break; 

	default: 
		{
			string errorMsg = "Code of message invalid: ";
			errorMsg.append(mUtil.intToStr(code)); 
			error (errorMsg.c_str()); 
			return;
		}
	}

	// return 

	if (response.size() > 0) {

		// Return -> Send message response

		if (bleConnected ()) { 
			bleSendData(response);
		} 
	} 

#ifdef HAVE_BATTERY
	// Return energy situation too? 

	if (sendEnergy) { 

		checkEnergyVoltage (true);

	} 
#endif

	// Mark the mTimeSeconds of the receipt 

	mLastTimeReceivedData = mTimeSeconds;

	// Here is processed messages that have actions to do after response sended

	switch (code) {

	case 98: // Restart the Esp32

		// Wait 500 ms, to give mobile app time to quit 

		if (mAppConnected) {
			delay (500); 
		}

		restartESP32();
		break; 

	case 99: // Standby - enter in deep sleep

#ifdef HAVE_STANDBY

		// Wait 500 ms, to give mobile app time to quit 

		if (mAppConnected) {
			delay (500); 
		}

		// Soft Off - enter in standby by main task to not crash or hang on finalize BLE
		// Notify main_Task to enter standby
			
		notifyMainTask(MAIN_TASK_ACTION_STANDBY_MSG);

#else

		// No have standby - restart

		restartESP32();

#endif

		break; 

	} 

} 

#ifdef HAVE_BATTERY
/**
 * @brief Check the voltage of the power supply (battery or charging via usb) 
 */
static void checkEnergyVoltage (bool sendingStatus) {

	string energy = "";

	static int16_t lastReadingVBAT = 0;

	// Volts in the power supply (battery) of the ESP32 via the ADC pin 
	// There is one resistive divider

	uint16_t readVBAT = mAdcBattery; // Read in adc.cc

	// Send the status to the application 

	int16_t diffAnalog = (readVBAT - lastReadingVBAT);
	if (diffAnalog < 0) diffAnalog *= -1;

	// Send the status of the application when requested or when there were alterations 

	if (bleConnected() &&										// Only if connected
			(sendingStatus ||									// And sending status
					(!mGpioVEXT && diffAnalog > 20))) {			// Or significative diff

		logD("vbat=%u diff=%u", readVBAT, diffAnalog);

		// Message to App

		energy="10:";
		energy.append ((mGpioVEXT)? "EXT:": "BAT:");
		energy.append ((mGpioChgBattery)? "Y:": "N:");
		energy.append (mUtil.intToStr(readVBAT));
		energy.append (1u, ':'); 

	}

	// Send it to app ?

	if (energy.size() > 0) {

		bleSendData(energy);
	}

	// Save last readings

	lastReadingVBAT = readVBAT;
} 
#endif

/**
 * @brief Standby - enter in deep sleep
 */
static void standby (const char *cause, bool sendBLEMsg) {

	// Enter in standby (standby off is reseting ESP32)
	// Yet only support a button to control it, touchpad will too in future

	// Debug

	logD ("Entering standby, cause -> %s", cause);

#ifdef PIN_BUTTON_STANDBY

	// Disable interrupt on gpio

	gpioDisableISR(PIN_BUTTON_STANDBY);

	// Send message to app mobile ?

	if (sendBLEMsg &&  mAppConnected && bleConnected()) {

		// Send the cause of the standby 

		string message = "99:";
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

	// Debug

	logD ("Finalizing ...");

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
		bleSendData(error);
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
 * @brief Process informations request
 */
static void sendInfo(Fields& fields) {

	// Note: the field 1 is a code of message

	// Type 

	string type = fields.getString(2);

	logV("type=%s", type.c_str());

	// Note: this is a example of send large message 

	const uint16_t MAX_INFO = 300;
	char info[MAX_INFO];

	// Return response (can bem more than 1, delimited by \n)

	string response = "";

	if (type == "ESP32" || type == "ALL") { // Note: For this example string type, but can be numeric

		// About the ESP32 // based on Kolban GeneralUtils
		// With \r as line separator

		esp_chip_info_t chipInfo;
		esp_chip_info(&chipInfo);

		const uint8_t* macAddr = bleMacAddress();

		char deviceName[30] = BLE_DEVICE_NAME;

		uint8_t size = strlen(deviceName);

		if (size > 0 && deviceName[size-1] == '_') { // Put last 2 of mac address in the name

			char aux[7];
			sprintf(aux, "%02X%02X", macAddr[4], macAddr[5]);

			strcat (deviceName, aux);
		}

#if !CONFIG_FREERTOS_UNICORE
		const char* uniCore = "No"; 
#else
		const char* uniCore = "Yes"; 
#endif

		// Note: the \n is a message separator and : is a field separator
		// Due this send # and ; (this will replaced in app mobile) 

		snprintf(info, MAX_INFO, "11:ESP32:"\
									"*** Chip Info#" \
									"* Model; %d#" \
									"* Revision; %d#" \
									"* Cores; %d#" \
									"* FreeRTOS unicore ?; %s#"
									"* ESP-IDF;#  %s#" \
									"*** BLE info#" \
									"* Device name; %s#" \
									"* Mac-address; %02X;%02X;%02X;%02X;%02X;%02X#" \
									"\n", \
									chipInfo.model, \
									chipInfo.revision, \
									chipInfo.cores, \
									uniCore, 
									esp_get_idf_version(), \
									deviceName,
									macAddr[0], macAddr[1], \
									macAddr[2], macAddr[3], \
									macAddr[4], macAddr[5] \
									); 

		response.append(info);
	}

	if (type == "FMEM" || type == "ALL") {

		// Free memory of ESP32 
		
		snprintf(info, MAX_INFO, "11:FMEM:%u\n", heap_caps_get_free_size(MALLOC_CAP_8BIT));

		response.append(info);

	}

	if (type == "VDD33" || type == "ALL") {

		// Voltage of ESP32 
		
		int read = rom_phy_get_vdd33();
		logV("rom_phy_get_vdd33=%d", read);

		snprintf(info, MAX_INFO, "11:VDD33:%d\n", read);

		response.append(info);

	}

#ifdef HAVE_BATTERY

	// VEXT and VBAT is update from energy message type

	if (type == "VBAT" || type == "VEXT" || type == "ALL") {

		checkEnergyVoltage (true);

	} 
#endif

//	logV("response -> %s", response.c_str());

	// Send

	bleSendData(response);

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

	// Main Task is alive ?

	if (xTaskMainHandler == NULL) {
		return;
	}

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
