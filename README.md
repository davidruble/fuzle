# fuzle
Minimal library that gets the length in seconds of xWMA audio embedded in a Skyrim FUZ file. There are no dependencies outside of the standard C++ libraries.

## Usage
To run the tests in the provided test directory, you can do something akin to the following:
```
cd test
g++ -std=c++11 -static ../fuzle.cpp main.cpp -o test
```