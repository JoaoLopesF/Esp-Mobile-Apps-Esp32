/*
 * esp_util.h
 *
 */

#ifndef UTIL_ESP_UTIL_H_
#define UTIL_ESP_UTIL_H_

///// Includes

#include <stdint.h>
#include <stdbool.h>
#include <math.h>

#define _GLIBCXX_USE_C99 // Needed for std::string -> to_string inclusion.

#include <cstdlib>
#include <string>
#include <vector>

using namespace std;

///// Definitions

// Roundf simple - do math.h in Esp32 is too slow

#ifndef ROUNDF
#define ROUNDF(x) (uint16_t)((x * 10.0f + 0.5f) / 10.0f)
#endif

///// Classe Singleton

class Esp_Util
{

	private:

		// Singleton pattern

		Esp_Util() {}

	public:

    	// Singleton pattern

    	static Esp_Util& getInstance() {
    		static Esp_Util instance;
    		return instance;
    	}

    	// Methods

		void esp32Initialize();

		void strReplace(string& str, char c1, char c2);

		bool strIsNum(const string& str);

		int strToInt(const string& str);

		float roundFloat(float value, uint8_t decimals = 2);
		float strToFloat(const string& str);

		string floatToStr(float value, uint8_t decimals = 2, bool comma = false);

		string intToStr(uint32_t value);
		string formatNumber(uint32_t number, uint8_t size, char insert='0');
		string formatFloat(float value, uint8_t intPlaces=0, uint8_t decPlaces=2, bool comma=false);
		string formatMinutes(uint16_t minutes);

		string strExpand(const string& str);

		void strSplit(vector<string>& tokens, const string& str, const string& delimiter);

};

/////// Macros

#ifndef ARDUINO // To ESP-IDF code

#define delay(n) vTaskDelay(n / portTICK_PERIOD_MS)
#define millis() (uint32_t)(esp_timer_get_time() / 1000)

#endif

#endif /* UTIL_ESP_UTIL_H_ */

//////// End