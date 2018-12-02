#pragma once

// TODO: Either break out of readShort/Long chains on error or only show "not enough space" error once
// TODO: Read from input stream as well as file path
// TODO: Better comments
// TODO: Change to static class function (make helpers private)
// TODO: Only store data that we actually need (but comment out rest so know what's there and how many bytes)

#include <string>
#include <fstream>
#include <algorithm>
#include <iterator>
#include <cstring>
#include <iomanip>
#include <iostream>

namespace fuzle {

	// Return -1 on failure
	const float ERROR = -1.0f;

	struct WxmaHeader {
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

	// NOTE: Assumes Little Endian
	unsigned short readShort(char* buffer, int& startIndex, int bufLen) {
		if (startIndex + 1 >= bufLen) {
			std::cerr << "Not enough space in buffer for short!" << std::endl;
			return -1;
		}

		unsigned char rightBits = buffer[startIndex++];
		unsigned char leftBits = buffer[startIndex++];
		return rightBits | (leftBits << 8);
	}

	// NOTE: Assumes Little Endian
	unsigned long readLong(char* buffer, int& startIndex, int bufLen) {
		if (startIndex + 3 >= bufLen) {
			std::cerr << "Not enough space in buffer for long!" << std::endl;
			return -1;
		}

		// Note: startIndex is modified by each call to readShort()
		unsigned short rightBits = readShort(buffer, startIndex, bufLen);
		unsigned short leftBits = readShort(buffer, startIndex, bufLen);
		return rightBits | (leftBits << 16);
	}

	// Main entry point
	float GetAudioLengthInSeconds(const char* filepath) {
		float audioLength = 0.0f;

		// Open the file
		std::ifstream fs(filepath, std::ifstream::in | std::ifstream::binary);
		if (!fs) {
			std::cerr << "Unable to open file " << filepath << std::endl;
			return ERROR;
		}

		// Get the length of the file
		//
		// NOTE: Since the FUZ files are relatively small, we'll read the entire file into memory
		fs.seekg(0, fs.end);
		int length = fs.tellg();
		fs.seekg(0, fs.beg);

		// Read the file into a buffer
		char* buffer = new char[length];
		fs.read(buffer, length);

		if (!fs) {
			std::cerr << "Could not read entire file. Only " << fs.gcount()
				<< " out of " << length << " could be read" << std::endl;
			delete[] buffer;
			return ERROR;
		}
		fs.close();

		// Find the start index of "RIFF" in the file. This is the start of the
		// actual xWMA file data.
		int startIndex = -1;
		for (int i = 0; i < length - 3; i++) {
			if (std::strncmp(buffer + i, "RIFF", 4) == 0) {
				startIndex = i + 4;
				break;
			}
		}

		// RIFF not found in the file: most likely not a fuz file
		if (startIndex == -1) {
			std::cerr << "Unexpected file format" << std::endl;
			delete[] buffer;
			return ERROR;
		}

		// Read in the beginning of the WXMA header (up to WXMA)
		WxmaHeader hdr = {};
		int bufInd = startIndex;
		hdr.chunkSize = readLong(buffer, bufInd, length);

		// Early exit if bad file type
		if (std::strncmp(buffer + bufInd, "XWMA", 4) != 0) {
			std::cerr << "File type not XWMA!" << std::endl;
			delete[] buffer;
			return ERROR;
		}

		// Read in the rest of the header
		memcpy(hdr.XWMA, buffer + bufInd, 4); bufInd += 4;
		memcpy(hdr.subchunk1Id, buffer + bufInd, 4); bufInd += 4;
		hdr.subchunk1Size = readLong(buffer, bufInd, length); 
		hdr.format = readShort(buffer, bufInd, length);
		hdr.numChannels = readShort(buffer, bufInd, length);
		hdr.samplesPerSec = readLong(buffer, bufInd, length);
		hdr.bytesPerSec = readLong(buffer, bufInd, length);
		hdr.blockAlign = readShort(buffer, bufInd, length);
		hdr.bitsPerSample = readShort(buffer, bufInd, length);
		hdr.extSize = readShort(buffer, bufInd, length);
		
		// Break out early if no dpds data
		if (std::strncmp(buffer + bufInd, "dpds", 4) != 0) {
			std::cerr << "DPDS data not present" << std::endl;
			delete[] buffer;
			return ERROR;
		}

		// Read in the dpds data
		memcpy(hdr.subchunk2Id, buffer + bufInd, 4); bufInd += 4;
		hdr.subchunk2Size = readLong(buffer, bufInd, length) / 4;
		hdr.subchunk2Data = new unsigned long[hdr.subchunk2Size];
		for (unsigned int i = 0; i < hdr.subchunk2Size; i++) {
			hdr.subchunk2Data[i] = readLong(buffer, bufInd, length);
		}

		// Calculate the duration in seconds
		unsigned long totalBytes = hdr.subchunk2Data[hdr.subchunk2Size - 1];
		float numSamples = float(totalBytes) / float(hdr.numChannels * (hdr.bitsPerSample / 8));
		audioLength = numSamples / hdr.samplesPerSec;

		delete[] hdr.subchunk2Data;
		delete[] buffer;
		return audioLength;
	}
}
