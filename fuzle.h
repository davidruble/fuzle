#pragma once

#include <sstream>

/* 
 * Helper namespace used to get the length in seconds of a xWMA audio file embedded
 * in a Skyrim FUZ file.
 */
namespace Fuzle {
	// Return -1 on failure
	constexpr float FAILURE = -1.0f;

	/* 
	 * Gets the length of the xWMA audio in seconds. String path version.
	 *
	 * @param filepath: Path to FUZ file
	 * @return: Duration in seconds, or -1.0 on error
	 */
	float GetAudioLengthInSeconds(const char* filepath);

	/* 
	 * Gets the length of the xWMA audio in seconds. Input stream version.
	 *
	 * @param is: Input stream with FUZ data
	 * @return: Duration in seconds, or -1.0 on error
	 */
	float GetAudioLengthInSeconds(std::istream& is);

	/* 
	 * Gets the length of the xWMA audio in seconds. Raw data version.
	 *
	 * @param data: Raw FUZ data
	 * @param length: Length of FUZ data
	 * @return: Duration in seconds, or -1.0 on error
	 */
	float GetAudioLengthInSeconds(const uint8_t* data, size_t length);
}
