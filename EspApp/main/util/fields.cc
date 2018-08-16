/*****************************************
 * Project   : util - Utilities to esp-idf
 * Programmer: Joao Lopes
 * Module    : fields - C++ class to process delimited fields in string
 * Comments  :
 * Versions  :
 * ------- 	-------- 	-------------------------
 * 0.1.0 	01/08/18 	First version
 */


///// Includes

#include <stdint.h>
#include <stdbool.h>

#include <string>
#include <vector>

using namespace std;

// Utilities

#include"esp_util.h"

// This

#include "fields.h"

////// Variables

// Log

static const char* TAG = "fields";

////// Vector

static vector <string> fields;

// Utils

static Esp_Util& Util = Esp_Util::getInstance();

////// Class

/**
* @brief Fields C++ class constructor
*/
Fields::Fields(const string& str, const string& delimiter) {

	// Process the string and populate a vector with this fields

	fields.clear();

	// Split

	Util.strSplit(fields, str, delimiter);

//	for (int i = 0; i< fields.size(); i++)
//		logV("Test field %u = %s\n", i, fields[i].c_str());

}

/**
* @brief Fields C++ class destructor
*/
Fields::~Fields()
{
	// Clear the vector 

	fields.clear();

}

/////// Methods

/**
* @brief Return the size of fields load in vector
*/
uint8_t Fields::size() {

	return fields.size();
}

/**
* @brief Get field type string
*/
string Fields::getString(uint8_t fieldNum) {

	if (fieldNum > 0 && fieldNum <= fields.size()) {
		return fields[fieldNum - 1];
	} else {
		return "";
	}
}

/**
* @brief Get field type char
*/
char Fields::getChar(uint8_t fieldNum) {

	if (fieldNum > 0 && fieldNum <= fields.size()) {
		return fields[fieldNum - 1][0];
	} else {
		return '\0';
	}

}

/**
* @brief Returns if the contents is numeric or not
*/
bool Fields::isNum(uint8_t fieldNum) {

	if (fieldNum > 0 && fieldNum <= fields.size()) {
		return Util.strIsNum(fields[fieldNum - 1]);
	} else {
		return false;
	}
}

/**
* @brief Get field type int 
*/
int32_t Fields::getInt(uint8_t fieldNum) {

	if (fieldNum > 0 && fieldNum <= fields.size()) {
		return Util.strToInt(fields[fieldNum - 1]);
	} else {
		return 0;
	}

}

/**
* @brief Get field type float
*/
float Fields::getFloat(uint8_t fieldNum) {

	if (fieldNum > 0 && fieldNum <= fields.size()) {
		return Util.strToFloat(fields[fieldNum - 1]);
	} else {
		return 0.0f;
	}

}

//////// End