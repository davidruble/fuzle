#include "fuzle.h"

#include <fstream>
#include <cstring>
#include <iostream>
#include <stddef.h>

namespace Fuzle {

	/////////////////////////////////////////////////////////////////
	// Helper functions/structs that the user shouldn't care about //
	/////////////////////////////////////////////////////////////////
	namespace {

		/*
		 * Contains all of the data from the xWMA (RIFF) header, up until the "data"
		 * structure that contains the actual audio data.
		 *
		 * Note that all of the fields are not marked as "Needed for length calc", yet
		 * aren't commented out. These fields aren't needed to calculate the length of
		 * the audio, but they are helpful to have as a convenient way of getting the
		 * byte offsets in the stream.
		 */
		struct XwmaHeader {
			unsigned char RIFF[4];         // "RIFF"
			unsigned long chunkSize;
			unsigned char XWMA[4];         // "XWMA"
			unsigned char subchunk1Id[4];  // "fmt "
			unsigned long subchunk1Size;   // size of fmt chunk
			unsigned short format;
			unsigned short numChannels;    // Needed for length calc.
			unsigned long samplesPerSec;   // Needed for length calc.
			unsigned long bytesPerSec;
			unsigned short blockAlign;
			unsigned short bitsPerSample;  // Needed for length calc.
			unsigned short extSize;
			unsigned char subchunk2Id[4];  // "dpds"
			// HIDDEN COMPILER PACKING OF 2 BYTES WILL HAPPEN HERE
			unsigned long subchunk2Size;   // Needed for length calc. Length of dpds chunk.
			unsigned long* subchunk2Data;  // Needed for length calc. Dpds data
		};

		// The compiler will add 2 bytes of padding after subchunk2Id to align 
		// subchunk2Size
		constexpr int COMP_PAD = 2;

		// For the header size, we will not consider the subchunk2Data (dpds chunk)
		// since it is dynamic based off of the subchunk2Size. We also need to take
		// into account the compiler padding, as mentioned in the COMP_PAD comment.
		constexpr int HEADER_SIZE_BYTES = sizeof(XwmaHeader) - COMP_PAD - sizeof(unsigned long*);

		/*
		 * Read 2 bytes from the buffer.
		 *
		 * NOTE: Assumes Little Endian.
		 *
		 * @param buffer: Buffer to read from
		 * @param startIndex: Index to start in the buffer.
		 * @param bufLen: Length of the buffer as a whole
		 * @return: Unsigned short representation of the 2 bytes, or throws an
		 *		    exception on error
		 */
		unsigned short ReadShort(char* buffer, int startIndex, int bufLen) {
			if (startIndex + 1 >= bufLen) {
				throw std::runtime_error("Not enough space in buffer for short!");
			}

			unsigned char rightBits = buffer[startIndex];
			unsigned char leftBits = buffer[startIndex + 1];
			return rightBits | (leftBits << 8);
		}

		/*
		 * Read 4 bytes from the buffer.
		 *
		 * NOTE: Assumes Little Endian.
		 *
		 * @param buffer: Buffer to read from
		 * @param startIndex: Index to start in the buffer.
		 * @param bufLen: Length of the buffer as a whole
		 * @return: Unsigned long representation of the 4 bytes, or throws an
		 *			exception on error
		 */
		unsigned long ReadLong(char* buffer, int startIndex, int bufLen) {
			if (startIndex + 3 >= bufLen) {
				throw std::runtime_error("Not enough space in buffer for long!");
			}

			unsigned short rightBits = ReadShort(buffer, startIndex, bufLen);
			unsigned short leftBits = ReadShort(buffer, startIndex + 2, bufLen);
			return rightBits | (leftBits << 16);
		}

		/*
		 * Helper function to compute the audio length.
		 *
		 * @param is: Stream containing the FUZ data
		 * @return: Audio length in seconds, or FAILURE on failure
		 */
		float ComputeLength(std::istream& is) {
			XwmaHeader hdr = {};
			float audioLength = 0.0f;

			// Buffer to hold the header data read from the stream (minus dpds data)
			char* buffer = new char[HEADER_SIZE_BYTES];

			// Buffer to hold the dynamic dpds data
			char* dpdsBuffer = NULL;

			// There should be a 4 byte length of the LIP portion 9 bytes in, so read 12.
			is.read(buffer, 12);

			// Exit early if not a FUZ file
			if (std::strncmp(buffer, "FUZE", 4) != 0) {
				std::cerr << "Not a FUZ file!" << std::endl;
				delete[] buffer;
				return FAILURE;
			}

			// Get the LIP length and seek the file pointer to after the LIP contents
			try {
				unsigned long lipLen = ReadLong(buffer, 8, HEADER_SIZE_BYTES);
				is.seekg(lipLen, is.cur);
			}
			catch (std::exception& e) {
				std::cerr << "Error getting LIP length: " << e.what() << std::endl;
				delete[] buffer;
				return FAILURE;
			}

			// Read in data for the header up to the subchunk2Data (since it's dynamic).
			//
			// Note: Reading all of the data at once is faster and simpler than
			// reading->seeking->reading->... Plus we're only reading 46 bytes.
			is.read(buffer, HEADER_SIZE_BYTES);

			// After seeking, we should be at the start of the RIFF header
			if (std::strncmp(buffer, "RIFF", 4) != 0) {
				std::cerr << "No RIFF section!" << std::endl;
				delete[] buffer;
				return FAILURE;
			}

			// Make sure there is dpds data present to help calculate the length
			if (std::strncmp(buffer + (int)offsetof(XwmaHeader, subchunk2Id), "dpds", 4) != 0) {
				std::cerr << "No dpds data!" << std::endl;
				delete[] buffer;
				return FAILURE;
			}

			// Get the values we need from the buffer
			try {
				hdr.numChannels = ReadShort(
					buffer, (int)offsetof(XwmaHeader, numChannels), HEADER_SIZE_BYTES);
				hdr.samplesPerSec = ReadLong(
					buffer, (int)offsetof(XwmaHeader, samplesPerSec), HEADER_SIZE_BYTES);
				hdr.bitsPerSample = ReadShort(
					buffer, (int)offsetof(XwmaHeader, bitsPerSample), HEADER_SIZE_BYTES);
				hdr.subchunk2Size = ReadLong(
					buffer, (int)offsetof(XwmaHeader, subchunk2Size) - COMP_PAD, HEADER_SIZE_BYTES);

				// Read the dynamic dpds data
				dpdsBuffer = new char[hdr.subchunk2Size];
				is.read(dpdsBuffer, hdr.subchunk2Size);
				hdr.subchunk2Data = new unsigned long[hdr.subchunk2Size / 4];
				for (unsigned int i = 0; i < hdr.subchunk2Size / 4; i++) {
					hdr.subchunk2Data[i] = ReadLong(dpdsBuffer, i * 4, hdr.subchunk2Size);
				}
			}
			catch (std::exception& e) {
				std::cerr << "Error getting buffer data: " << e.what() << std::endl;
				if (hdr.subchunk2Data) delete[] hdr.subchunk2Data;
				if (dpdsBuffer) delete[] dpdsBuffer;
				delete[] buffer;
				return FAILURE;
			}

			// Calculate the duration in seconds
			unsigned long totalBytes = hdr.subchunk2Data[(hdr.subchunk2Size / 4) - 1];
			float numSamples = float(totalBytes) / float(hdr.numChannels * (hdr.bitsPerSample / 8));
			audioLength = numSamples / hdr.samplesPerSec;

			delete[] hdr.subchunk2Data;
			delete[] buffer;
			delete[] dpdsBuffer;
			return audioLength;
		}
	}


	//////////////////////////
	// Fuzle implementation //
	//////////////////////////

	// See header for function comment
	float GetAudioLengthInSeconds(const char* filepath) {
		// Open the file
		std::ifstream fs(filepath, std::ifstream::in | std::ifstream::binary);
		if (!fs) {
			std::cerr << "Unable to open file " << filepath << std::endl;
			return FAILURE;
		}

		float len = ComputeLength(fs);
		fs.close();
		return len;
	}

	// See header for function comment
	float GetAudioLengthInSeconds(std::istream& is) {
		return ComputeLength(is);
	}

	// See header for function comment
	float GetAudioLengthInSeconds(const uint8_t* data, size_t length) {
		if (data == NULL) return FAILURE;

		/*
		 * TODO: This copies the data TWICE, which is bad juju. Even though this
		 * is only working with files KB in size, I should find a way that's more
		 * memory efficient.
		 *
		 * A possible solution to this could be to make a custom istream class that
		 * also uses a custom streambuf, or (more likely) to overload ComputeLength()
		 * to operate on the raw data.
		 */
		std::string s((char*)data, length);
		std::istringstream iss(s);

		return ComputeLength(iss);
	}
}
