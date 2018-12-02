# fuzle
Minimal header library that gets the length in seconds of xWMA audio embedded in a Skyrim FUZ file.

## Usage
To use the library, just copy fuzle.h into your include directory. There are no dependencies outside of the standard C++ libraries.

To run the tests in the provided test directory, you can do something akin to the following:
```
cd test
g++ -std=c++11 -static main.cpp -o test
```