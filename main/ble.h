/*
 * ble.h
 *
 */

#ifndef MAIN_BLE_H_
#define MAIN_BLE_H_

///// Includes

#include "esp_system.h"
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// C++

#include <string>
using namespace std;

/////// Definitions

#define BLE_DEVICE_NAME "Esp32_Example_"    // Device name //TODO: see it!
                                            // Tip: is it ends with _, 
                                            // last two of the mac address is appended to name
////// Prototypes

extern void bleInitialize();
extern void bleFinalize();
extern void bleSendData(string data);
extern bool bleConnected();
extern void bleVerifyTimeouts();

#endif /* MAIN_BLE_H_ */

//////// End