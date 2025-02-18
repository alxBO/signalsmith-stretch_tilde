Signalsmith-Stretch For Max
===========================================

This is a Max external based on Signalsmith Stretch: C++ pitch/time library, the amazing project developped by [Geraint Luff / Signalsmith Audio Ltd] [signalsmith-stretch](https://github.com/Signalsmith-Audio/signalsmith-stretch).

This external is mainly focused on hyper-time stretching.

The object was developed for research or experimental applications. Therefore we do not guarantee any sustained development and support. 


__Compatibility:__ Max 8+ [Mac Intel/ARM, Windows 64bits]

__Features:__
- Read a buffer~ (1-4 channels)
- Realtime time stretching / pitch shifting

__TODO:__
- seek is not implemented


## Author

This code has been developped by <a href="https://www.alexbouvier.art">Alex Bouvier</a>.

## Compiling

### MacOS / Windows x64

__Requirements:__
- Signalsmith stretch (as a submodule, main branch)
- stdc++17
- Max SDK

__Steps:__
- cd [MaxSDKFolder]
- git clone --recursive https://github.com/alxBO/signalsmith-stretch_tilde.git ./source/yoursubfolder/signalsmith-stretch~
- cmake -B build .
- cmake --build build --config Release

__When cross compiling for Win64 using Ming-W64 (brew install mingw-w64)__:
- cd [MaxSDKFolder]
- git clone --recursive https://github.com/alxBO/signalsmith-stretch_tilde.git ./source/yoursubfolder/signalsmith-stretch~
- cmake -B build_win -DCMAKE_TOOLCHAIN_FILE="./source/yoursubfolder/signalsmith-stretch~/cmake/mingw-w64-toolchain.cmake" .
- (optional) check the SIMD in mingw-w64-toolchain.cmake (default: -mavx2)
- cmake --build build_win --config Release


## Credits & License

This project is based on [Signalsmith-Stretch](https://github.com/Signalsmith-Audio/signalsmith-stretch), developed by Signalsmith-Audio and licensed under the MIT License.
The modifications and integration with Max/MSP were developed by [Alex Bouvier] in 2025.
The original MIT license applies to the Signalsmith-Stretch source code. Any additional code written for the Max/MSP external is also released under MIT license.




