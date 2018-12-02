#pragma once

#include <fstream>
#include <cstring>
#include <iostream>

/* 
 * Contains all of the data from the xWMA (RIFF) header, up until the "data"
 * structure that contains the actual audio data.
 *
 * The commented out fields aren't needed in the duration calculations, but
 * they are left in to show how many bytes are between each necessary field
 * and to show what is in those bytes.
 */
struct XwmaHeader {
	//unsigned long chunkSize;
	//unsigned char XWMA[4];         // "XWMA"
	//unsigned char subchunk1Id[4];  // "fmt "
	//unsigned long subchunk1Size;   // size of fmt chunk
	//unsigned short format;
	unsigned short numChannels;
	unsigned long samplesPerSec;
	//unsigned long bytesPerSec;
	//unsigned short blockAlign;
	unsigned short bitsPerSample;
	//unsigned short extSize;        // Should be 0
	//unsigned char subchunk2Id[4];  // "dpds"
	unsigned long subchunk2Size;     // length of dpds chunk
	unsigned long* subchunk2Data;    // dpds data
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
		bufInd += 4; // chunkSize

		// Early exit if bad file type
		if (std::strncmp(buffer + bufInd, "XWMA", 4) != 0) {
			std::cerr << "File type not XWMA!" << std::endl;
			delete[] buffer;
			return ERROR;
		}

		// Read in the rest of the header
		try {
			bufInd += 4; // XWMA
			bufInd += 4; // subchunk1Id
			bufInd += 4; // subchunk1Size
			bufInd += 2; // format
			hdr.numChannels = ReadShort(buffer, bufInd, length);
			hdr.samplesPerSec = ReadLong(buffer, bufInd, length);
			bufInd += 4; // bytesPerSec
			bufInd += 2; // blockAlign
			hdr.bitsPerSample = ReadShort(buffer, bufInd, length);
			bufInd += 2; // extSize
		} catch (std::exception& e) {
			std::cerr << e.what() << std::endl;
			delete[] buffer;
			return ERROR;
		}
		
		// Break out early if no dpds data
		if (std::strncmp(buffer + bufInd, "dpds", 4) != 0) {
			std::cerr << "DPDS data not present" << std::endl;
			delete[] buffer;
			return ERROR;
		}

		// Read in the dpds data
		try {
			bufInd += 4; // subchunk2Id
			hdr.subchunk2Size = ReadLong(buffer, bufInd, length) / 4;
			hdr.subchunk2Data = new unsigned long[hdr.subchunk2Size];
			for (unsigned int i = 0; i < hdr.subchunk2Size; i++) {
				hdr.subchunk2Data[i] = ReadLong(buffer, bufInd, length);
			}
		} catch (std::exception& e) {
			std::cerr << e.what() << std::endl;
			if (hdr.subchunk2Data) delete[] hdr.subchunk2Data;
			delete[] buffer;
			return ERROR;
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
	 * @return: unsigned short representation of the 2 bytes, or throws an
	 *		    exception on error
	 */
	static unsigned short ReadShort(char* buffer, int& startIndex, int bufLen) {
		if (startIndex + 1 >= bufLen) {
			throw std::runtime_error("Not enough space in buffer for short!");
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
	 * @return: unsigned long representation of the 4 bytes, or throws an 
	 *			exception on error
	 */
	static unsigned long ReadLong(char* buffer, int& startIndex, int bufLen) {
		if (startIndex + 3 >= bufLen) {
			throw std::runtime_error("Not enough space in buffer for long!");
		}

		// Note: startIndex is modified by each call to ReadShort()
		unsigned short rightBits = ReadShort(buffer, startIndex, bufLen);
		unsigned short leftBits = ReadShort(buffer, startIndex, bufLen);
		return rightBits | (leftBits << 16);
	}
};
