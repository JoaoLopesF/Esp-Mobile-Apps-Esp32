/*
 * ble_server.h
 */

#ifndef UTIL_BLE_SERVER_H_
#define UTIL_BLE_SERVER_H_

///// Includes

#include <stdint.h>
#include <stdbool.h>

// C++

#include <string>
using std::string;

////// Defines

// Ble

#define BLE_MAX_SIZE_SEND GATTS_CHAR_VAL_LEN_MAX
#define BLE_MAX_SIZE_RECV GATTS_CHAR_VAL_LEN_MAX

#define BLE_TIMEOUT_RECV_LINE 3

#define BLE_TASK_RECEV_PRIOR 5

#if !CONFIG_FREERTOS_UNICORE
	#define BLE_TASK_RECEB_CPU 1
#else
	#define BLE_TASK_RECEB_CPU 0
#endif

#define BLE_SIZE_QUEUE_RECV 5

////// Classes

class BleServer;
class BleServerCallbacks;

// Ble Server - C++ class wrapper to ble_uart_server (C)

class BleServer
{
	public:

		void initialize(const char* deviceName, BleServerCallbacks* pBleServerCallbacks);
		void finalize();
		bool connected();
		void sendData(const char*);
		void debug(bool);

	private:

};


// Callbacks - based in Kolban callback example

class BleServerCallbacks {

public:
	virtual ~BleServerCallbacks() {}
	virtual void onConnect() = 0;
	virtual void onDisconnect() = 0;
	virtual void onReceive(char* data, uint8_t size) = 0;

};

#endif /* MAIN_BLE_H_ */

//////// End