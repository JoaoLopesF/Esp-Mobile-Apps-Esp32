#include "Arduino.h"

/////////// Wrapper Arduino example code to Esp-Idf-Mobile-Apps-Esp32 ESP-IDF code
/////////// Using Log from main

#include "main/main.h"
#include "main/util/log.h"

const char* TAG = "arduino";

//The setup function is called once at startup of the sketch
void setup()
{
	// Start app_main to initialize the ESP-IDF things (BLE, GPIOs, etc)

	logV("Starting ESP_IDF app main");
	app_main();
	logV("Started ESP_IDF app main");

}

// The loop function is called in an endless loop
void loop()
{
	//Add your repeated code here (if you need please replace the delay to your Arduino code)
	delay(portMAX_DELAY);
}
