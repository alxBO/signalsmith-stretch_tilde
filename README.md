Signalsmith-Stretch For Max (signalsmith-stretch~)
===========================================

Max external based on Signalsmith Stretch: C++ pitch/time library, the amazing project developped by [Geraint Luff / Signalsmith Audio Ltd] [signalsmith-stretch](https://github.com/Signalsmith-Audio/signalsmith-stretch). It allows realtime time-stretching and pitch-shifting.

This external is mainly focused on hyper time-stretching.

The object was developed for research or experimental applications. Therefore we do not guarantee any sustained development and support. 

## See Latest Releases 

-  [macOS Intel (x86_64)](https://github.com/alxBO/signalsmith-stretch_tilde/releases/latest/download/signalsmith-stretch_macOS_Intel_x86_x64.zip)
-  [macOS Apple Silicon](https://github.com/alxBO/signalsmith-stretch_tilde/releases/latest/download/signalsmith-stretch_macOS_Apple_Silicon.zip)
-  [Windows 64-bit (x86_64)](https://github.com/alxBO/signalsmith-stretch_tilde/releases/latest/download/signalsmith-stretch.mxe64)

## Detail

__Features:__
- Read a buffer~ (1-4 channels)
- Realtime time stretching / pitch shifting

__TODO:__
- seek is not implemented

__Compatibility:__ Max 8+

## Compiling

### MacOS / Windows x64

__Requirements:__
- Signalsmith stretch (as a submodule, main branch)
- stdc++17
- Max SDK

__Steps:__
- cd [MaxSDKFolder] [clone](https://github.com/Cycling74/max-sdk)
- git clone --recurse-submodules https://github.com/alxBO/signalsmith-stretch_tilde.git ./source/yoursubfolder/signalsmith-stretch~
- cmake -B build .
- cmake --build build --config Release

__When cross compiling for Win64 using Ming-W64__:
- brew install mingw-w64
- cd [MaxSDKFolder] [clone](https://github.com/Cycling74/max-sdk)
- git clone --recurse-submodules https://github.com/alxBO/signalsmith-stretch_tilde.git ./source/yoursubfolder/signalsmith-stretch~
- cmake -B build_win -DCMAKE_TOOLCHAIN_FILE="./source/yoursubfolder/signalsmith-stretch~/cmake/mingw-w64-toolchain.cmake" .
- (optional) check the SIMD in mingw-w64-toolchain.cmake (default: -mavx2)
- cmake --build build_win --config Release


## Credits & License

This project is based on [Signalsmith-Stretch](https://github.com/Signalsmith-Audio/signalsmith-stretch), developed by Signalsmith-Audio and licensed under the MIT License.
The modifications and integration with Max/MSP were developed by [Alex Bouvier] in 2025.
The original MIT license applies to the Signalsmith-Stretch source code. Any additional code written for the Max/MSP external is also released under MIT license.

## About the author

This code has been developped by <a href="https://www.alexbouvier.art">Alex Bouvier</a>.



