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

////// Constructors

Fields::Fields(const string& str, const string& delimiter) {

	// Constructor

	// Process the string and populate a vector with this fields

	fields.clear();

	// Split

	Util.strSplit(fields, str, delimiter);

//	for (int i = 0; i< fields.size(); i++)
//		logV("Test field %u = %s\n", i, fields[i].c_str());

}

Fields::~Fields()
{
	// Destructor

	// Clear the vector 

	fields.clear();

}

/////// Methods

uint8_t Fields::size() {

	// Return the size of fields load in vector

	return fields.size();
}

string Fields::getString(uint8_t fieldNum) {

	// Get field type string

	if (fieldNum < fields.size()) {
		return fields[fieldNum];
	} else {
		return "";
	}
}

char Fields::getChar(uint8_t fieldNum) {

	// Get field type char

	if (fieldNum < fields.size()) {
		return fields[fieldNum][0];
	} else {
		return '\0';
	}

}

bool Fields::isNum(uint8_t fieldNum) {

	// Returns if the contents is numeric or not

	if (fieldNum < fields.size()) {
		return Util.strIsNum(fields[fieldNum]);
	} else {
		return false;
	}
}

int32_t Fields::getInt(uint8_t fieldNum) {

	// Get field type int

	if (fieldNum < fields.size()) {
		return Util.strToInt(fields[fieldNum]);
	} else {
		return 0;
	}

}
float Fields::getFloat(uint8_t fieldNum) {

	// Get field type float

	if (fieldNum < fields.size()) {
		return Util.strToFloat(fields[fieldNum]);
	} else {
		return 0.0f;
	}

}

//////// End