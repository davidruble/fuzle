#pragma once

// TODO: Either break out of ReadShort/Long chains on error or only show "not enough space" error once
// TODO: Only store data that we actually need (but comment out rest so know what's there and how many bytes)

#include <string>
#include <fstream>
#include <algorithm>
#include <iterator>
#include <cstring>
#include <iomanip>
#include <iostream>


/* 
 * Contains all of the data from the xWMA (RIFF) header, up until the "data"
 * structure that contains the actual audio data.
 */
struct XwmaHeader {
	unsigned long chunkSize;
	unsigned char XWMA[4];			// "XWMA"
	unsigned char subchunk1Id[4];	// "fmt "
	unsigned long subchunk1Size;	// size of fmt chunk
	unsigned short format;
	unsigned short numChannels;
	unsigned long samplesPerSec;
	unsigned long bytesPerSec;
	unsigned short blockAlign;
	unsigned short bitsPerSample;
	unsigned short extSize;			// Should be 0
	unsigned char subchunk2Id[4];	// "dpds"
	unsigned long subchunk2Size;	// size of dpds chunk
	unsigned long* subchunk2Data;
};


/* 
 * Static class used to get the length in seconds of a xWMA audio file embedded
 * in a Skyrim FUZ file.
 * 
 * Technically, this works on both FUZ files and xWMA files, but it's meant for
 * FUZ files.
 */
class Fuzle {

public:
	// Return -1 on failure
	static constexpr float ERROR = -1.0f;

	/* 
	 * Gets the length of the xWMA audio in seconds. String path version.
	 *
	 * @param filepath: Path to FUZ file
	 * @return: Duration in seconds, or -1.0 on error
	 */
	static float GetAudioLengthInSeconds(const char* filepath) {
		// Open the file
		std::ifstream fs(filepath, std::ifstream::in | std::ifstream::binary);
		if (!fs) {
			std::cerr << "Unable to open file " << filepath << std::endl;
			return ERROR;
		}

		float len = ComputeLength(fs);
		fs.close();
		return len;
	}

	/* 
	 * Gets the length of the xWMA audio in seconds. Input stream version.
	 *
	 * @param is: Input stream with FUZ data
	 * @return: Duration in seconds, or -1.0 on error
	 */
	static float GetAudioLengthInSeconds(std::istream& is) {
		return ComputeLength(is);
	}

private:
	/*
	 * Does the heavy lifting for GetAudioLengthInSeconds()
	 *
	 * @param is: Input stream with FUZ data
	 * @return: Duration in seconds, or -1.0 on error
	*/
	static float ComputeLength(std::istream& is) {
		float audioLength = 0.0f;

		// Get the length of the file.
		//
		// NOTE: Since the FUZ files are relatively small, we'll read the entire
		// file into memory
		is.seekg(0, is.end);
		int length = is.tellg();
		is.seekg(0, is.beg);

		// Read the file into a buffer
		char* buffer = new char[length];
		is.read(buffer, length);

		if (!is) {
			std::cerr << "Could not read entire file. Only " << is.gcount()
				<< " out of " << length << " could be read" << std::endl;
			delete[] buffer;
			return ERROR;
		}

		// Find the start index of "RIFF" in the file. This is the start of the
		// actual xWMA file data.
		int startIndex = -1;
		for (int i = 0; i < length - 3; i++) {
			if (std::strncmp(buffer + i, "RIFF", 4) == 0) {
				startIndex = i + 4;
				break;
			}
		}

		// RIFF not found in the file: most likely not a FUZ file
		if (startIndex == -1) {
			std::cerr << "Unexpected file format" << std::endl;
			delete[] buffer;
			return ERROR;
		}

		// Read in the beginning of the xWMA header (up to XWMA)
		XwmaHeader hdr = {};
		int bufInd = startIndex;
		hdr.chunkSize = ReadLong(buffer, bufInd, length);

		// Early exit if bad file type
		if (std::strncmp(buffer + bufInd, "XWMA", 4) != 0) {
			std::cerr << "File type not XWMA!" << std::endl;
			delete[] buffer;
			return ERROR;
		}

		// Read in the rest of the header
		memcpy(hdr.XWMA, buffer + bufInd, 4); bufInd += 4;
		memcpy(hdr.subchunk1Id, buffer + bufInd, 4); bufInd += 4;
		hdr.subchunk1Size = ReadLong(buffer, bufInd, length); 
		hdr.format = ReadShort(buffer, bufInd, length);
		hdr.numChannels = ReadShort(buffer, bufInd, length);
		hdr.samplesPerSec = ReadLong(buffer, bufInd, length);
		hdr.bytesPerSec = ReadLong(buffer, bufInd, length);
		hdr.blockAlign = ReadShort(buffer, bufInd, length);
		hdr.bitsPerSample = ReadShort(buffer, bufInd, length);
		hdr.extSize = ReadShort(buffer, bufInd, length);
		
		// Break out early if no dpds data
		if (std::strncmp(buffer + bufInd, "dpds", 4) != 0) {
			std::cerr << "DPDS data not present" << std::endl;
			delete[] buffer;
			return ERROR;
		}

		// Read in the dpds data
		memcpy(hdr.subchunk2Id, buffer + bufInd, 4); bufInd += 4;
		hdr.subchunk2Size = ReadLong(buffer, bufInd, length) / 4;
		hdr.subchunk2Data = new unsigned long[hdr.subchunk2Size];
		for (unsigned int i = 0; i < hdr.subchunk2Size; i++) {
			hdr.subchunk2Data[i] = ReadLong(buffer, bufInd, length);
		}

		// Calculate the duration in seconds
		unsigned long totalBytes = hdr.subchunk2Data[hdr.subchunk2Size - 1];
		float numSamples = float(totalBytes) / float(hdr.numChannels * (hdr.bitsPerSample / 8));
		audioLength = numSamples / hdr.samplesPerSec;

		delete[] hdr.subchunk2Data;
		delete[] buffer;
		return audioLength;
	}

	/* 
	 * Reads 2 bytes from the buffer.
	 *
	 * NOTE: Assumes Little Endian.
	 *
	 * @param buffer: Buffer to read from
	 * @param startIndex: Index to start in the buffer. THIS WILL BE INCREMENTED
	 *					  BY 2 AS A RESULT OF THIS FUNCTION.
	 * @param bufLen: Length of the buffer as a whole
	 * @return: unsigned short representation of the 2 bytes
	 */
	static unsigned short ReadShort(char* buffer, int& startIndex, int bufLen) {
		if (startIndex + 1 >= bufLen) {
			std::cerr << "Not enough space in buffer for short!" << std::endl;
			return -1;
		}

		unsigned char rightBits = buffer[startIndex++];
		unsigned char leftBits = buffer[startIndex++];
		return rightBits | (leftBits << 8);
	}

	/* 
	 * Reads 4 bytes from the buffer.
	 *
	 * NOTE: Assumes Little Endian.
	 *
	 * @param buffer: Buffer to read from
	 * @param startIndex: Index to start in the buffer. THIS WILL BE INCREMENTED
	 *					  BY 4 AS A RESULT OF THIS FUNCTION.
	 * @param bufLen: Length of the buffer as a whole
	 * @return: unsigned long representation of the 4 bytes
	 */
	static unsigned long ReadLong(char* buffer, int& startIndex, int bufLen) {
		if (startIndex + 3 >= bufLen) {
			std::cerr << "Not enough space in buffer for long!" << std::endl;
			return -1;
		}

		// Note: startIndex is modified by each call to ReadShort()
		unsigned short rightBits = ReadShort(buffer, startIndex, bufLen);
		unsigned short leftBits = ReadShort(buffer, startIndex, bufLen);
		return rightBits | (leftBits << 16);
	}
};
