A sample project that illustrates a crash bug I have found with simdjson v0.6.0.


To build and test:

0. git submodule init && git submodule update
1. mkdir build
2. cd build
3. cmake .. -GNinja -DCMAKE_BUILD_TYPE=Release
4. ninja
5. run: ./simdjson-bugs

Watch it crash.

