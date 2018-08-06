/*****************************************
 * Project   : util - Utilities to esp-idf
 * Programmer: Joao Lopes
 * Module    : median_filter - an running median filter to ADC readings
 * Comments  : based on ArduinoMedianFilter
 * Versions:
 * ------ 	-------- 	-------------------------
 * 1.0.0  	01/08/18	First version
 *****************************************/

#ifndef MAIN_UTIL_MEDIAN_FILTER_H_
#define MAIN_UTIL_MEDIAN_FILTER_H_

/*
 Based on ArduinoMedianFilter by Rustam Iskenderov
 High performance and low memory usage sorting algorithm for running median filter.
 */

template<typename T, unsigned int _size>
class MedianFilter {
public:

	// Constructor

	MedianFilter() {
	}


	void set(unsigned int idx, T value) {
		// Set value of sample

		_buffer[idx] = value;
	}

	bool getMedian(T& value) {

		// Get the median value

		sort();

		value = _buffer[_size / 2];
		return true;
	}

	bool getAverage(unsigned int samples, T &value) {

		// Get the average

		if (samples > 0) {
			if (_size < samples)
				samples = _size;

			unsigned int start = (_size - samples) / 2;
			unsigned int end = start + samples;

			sort();

			T sum = 0;
			for (unsigned int i = start; i < end; ++i) {
				sum += _buffer[i];
			}
			value = sum / samples;

			return true;
		}

		return false;
	}

private:

	T _buffer[_size]; // The buffer

	void sort() {

		// Sort the buffer

		unsigned int gap = _size;
		unsigned int swapped = true;

		while (gap > 1 || swapped == true) {
			gap = (gap * 10) / 13;

			if (gap < 1)
				gap = 1;

			swapped = false;

			for (unsigned int i = 0; i < _size - gap; ++i) {
				if (_buffer[i] > _buffer[i + gap]) {
					T tmp = _buffer[i];
					_buffer[i] = _buffer[i + gap];
					_buffer[i + gap] = tmp;
					swapped = true;
				}
			}
		}
	}
};

#endif /* MAIN_UTIL_MEDIAN_FILTER_H_ */

//////// End