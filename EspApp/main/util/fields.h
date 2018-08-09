/*
 * fields.h
 */

#ifndef UTIL_FIELDS_H_
#define UTIL_FIELDS_H_

///// Includes

#include <stdint.h>
#include <stdbool.h>

#include <string>

using namespace std;

////// Definitions

///// Class

class Fields
{
	public:

		// Constructors

		Fields(const string& str, const string& delimiter = ":");
		~Fields(void);

		// Methods

		uint8_t size();
	    string getString(uint8_t fieldNum);
	    char getChar(uint8_t fieldNum);
	    bool isNum(uint8_t fieldNum);
	    int32_t getInt(uint8_t fieldNum);
	    float getFloat(uint8_t fieldNum);

};

#endif /* UTIL_FIELDS_H_ */

//////// End