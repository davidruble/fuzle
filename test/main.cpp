#include <cstdio>
#include <string>
#include <assert.h>
#include <iostream>
#include <Windows.h>

#include "../fuzle.h"

using namespace std;

const string DATA_DIR = "data/";

// Allow a bit of wiggle room around the expected values (since working with floats)
const float acceptance = 0.05f;

// Since chrono::high_resolution_clock sucks on windows, I had to resort to
// Windows APIs for timings.
//
// Source: https://stackoverflow.com/a/1739265
double PCFreq = 0.0;
__int64 CounterStart = 0;
void StartCounter()
{
	LARGE_INTEGER li;
	if (!QueryPerformanceFrequency(&li))
		cout << "QueryPerformanceFrequency failed!\n";

	PCFreq = double(li.QuadPart) / 1000.0;

	QueryPerformanceCounter(&li);
	CounterStart = li.QuadPart;
}
double GetCounter()
{
	LARGE_INTEGER li;
	QueryPerformanceCounter(&li);
	return double(li.QuadPart - CounterStart) / PCFreq; // ms
}

/* 
 * Runs fuzle on the specified file.
 *
 * @param filename: Name to FUZ file (NOT path)
 * @param expectedVal: The expected duration in seconds
 */
void testFile(string filename, float expectedVal) {
	StartCounter();
	string path = DATA_DIR + filename;
	float ret = Fuzle::GetAudioLengthInSeconds(path.c_str());
	bool success = (ret >= expectedVal - acceptance && ret <= expectedVal + acceptance);
	cout << "Test " << filename << ": " << "Result: " << ret
		<< "   Success? " << success
		<< "   Timing: " << GetCounter() << " ms"
		<< endl;
}


int main() {
	// Expected values determined by extracting WAV files from FUZ and playing in VLC
	testFile("c01_c01hellos_000241ff_1.fuz", 1.06f);
	testFile("dialogueco_dcetidle_00096540_1.fuz", 14.07f);
	testFile("mq00__000e0ca8_1.fuz", 3.11f);
	testFile("mq201__00039f0e_1.fuz", 7.84f);
		
	cout << "Hit Enter to exit... ";
	getchar();
	return 0;
}