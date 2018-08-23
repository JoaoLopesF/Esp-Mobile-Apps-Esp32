/*
 * ble_uart_server.h
 */

#ifndef UTIL_BLE_UART_SERVER_H_
#define UTIL_BLE_UART_SERVER_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "stdbool.h"
#include "esp_err.h"
#include "esp_bt.h"

#include "log.h"

#define GATTS_SERVICE_UUID   0x0001
//#define GATTS_CHAR_UUID      0xFF01
//#define GATTS_DESCR_UUID     0x3333
#define GATTS_CHAR_NUM		2
#define GATTS_NUM_HANDLE     1+(3*GATTS_CHAR_NUM)

#define BLE_MANUFACTURER_DATA_LEN  	4

//#define GATTS_CHAR_VAL_LEN_MAX	22
#define GATTS_CHAR_VAL_LEN_MAX		185 - 5 // Changed maximum to avoid split of messages ( - 5 is for safe)

#define BLE_PROFILE_APP_ID 0

//#define BLE_TX_POWER ESP_PWR_LVL_N14 // Power TX lowerest -14db -- comment it to default 
									   // Only for esp-idf v3.1 or higher

// Log macros - this is to reduce number of logging

//#define BLE_DEBUG true		// Show debug logging ? (comment to disable)
								// Only if necessary debug ESP-IDF BT/BLE stuff
#ifndef BLE_DEBUG
	// Without logging output 
	#define ble_logD(fmt, ...)
#else
	// With logging output
	#define ble_logD(fmt, ...) logD(fmt, ##__VA_ARGS__)
#endif

// Log of errors
#define ble_logE(fmt, ...) logE(fmt, ##__VA_ARGS__)

// Prototypes added to esp_uart_server code - public

esp_err_t ble_uart_server_Initialize(const char* device_name);
esp_err_t ble_uart_server_Finalize();
void ble_uart_server_SetCallbackConnection(void (*callbackConnection)(),
		void (*callbackMTU)());
bool ble_uart_server_ClientConnected();
void ble_uart_server_SetCallbackReceiveData(
		void (*callbackReceived)(char* data, uint16_t size));
esp_err_t ble_uart_server_SendData(const char* data, uint16_t size);
uint16_t ble_uart_server_MTU();
const uint8_t* ble_uart_server_MacAddress();

#ifdef __cplusplus
}
#endif

#endif /* UTIL_BLE_UART_SERVER_H_ */

//////// End
