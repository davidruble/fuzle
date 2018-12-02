#include <cstdio>
#include <string>
#include <assert.h>
#include <iostream>

#include "../fuzle.h"

const std::string DATA_DIR = "data/";

// Allow a bit of wiggle room around the expected values (since working with floats)
const float acceptance = 0.05f;

void testFile(std::string filename, float expectedVal) {
	std::string path = DATA_DIR + filename;
	float ret = fuzle::GetAudioLengthInSeconds(path.c_str());
	bool success = (ret >= expectedVal - acceptance && ret <= expectedVal + acceptance);
	std::cout << "Test " << filename << ": " << "Result: " << ret << "   Success? " << success << std::endl;
}

int main() {
	// Expected values determined by extracting WAV files from FUZ and playing in VLC
	testFile("c01_c01hellos_000241ff_1.fuz", 1.06f);
	testFile("dialogueco_dcetidle_00096540_1.fuz", 14.07f);
	testFile("mq00__000e0ca8_1.fuz", 3.11f);
	testFile("mq201__00039f0e_1.fuz", 7.84f);
		
	getchar();
	return 0;
}