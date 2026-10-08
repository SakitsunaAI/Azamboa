# Azamboa

**Azamboa** is a highly optimized fork of the AzaharPlus 3DS emulator with extra features and extreme performance tuning. 
*You can use this on a potato instead of a computer :P*


## Features

- **Extreme Performance**: Stripped out the JIT (Dynarmic) and forced heavily optimized static recompilation (LTO, -O3, unrolled loops, fast-math).
- **Universal Compatibility**: If a game works on any Citra fork, it works here.
- **Legacy CPU Support**: Runs on older processors.
- **Quality of Life**: Built-in cheats, Amiibo generation, ZipPass, and enhanced multiplayer compatibility.
- **Easy Setup**: Download system files directly from official servers—no physical 3DS required.

*(The Azahar logo is the property of PabloMK7 and angyartanddraw)*

---

## 🛠️ Build Guide

### Prerequisites
- **Git**
- **CMake** (3.22+)
- A modern C++ compiler (Clang/GCC/MSVC)

### 🐧 Linux
```bash
# 1. Install dependencies (Debian/Ubuntu example)
sudo apt-get install cmake git build-essential libsdl2-dev \
  libavcodec-dev libavformat-dev libavutil-dev libswscale-dev \
  libfmt-dev libssl-dev qtbase5-dev qtmultimedia5-dev libqt5opengl5-dev

# 2. Clone the repository
git clone --recursive https://github.com/SakitsunaAI/Azamboa.git
cd Azamboa

# 3. Build with Extreme Optimization
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release -DENABLE_LTO=ON
make -j$(nproc)
```

### 🍎 macOS
```bash
# 1. Install dependencies via Homebrew
brew install cmake git sdl2 ffmpeg fmt openssl qt@5

# 2. Clone the repository
git clone --recursive https://github.com/SakitsunaAI/Azamboa.git
cd Azamboa

# 3. Build with Extreme Optimization
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release -DENABLE_LTO=ON
make -j$(sysctl -n hw.logicalcpu)
```

### 🪟 Windows (MSVC)
```powershell
# 1. Install Visual Studio 2022 (with C++ CMake tools for Windows)
# 2. Clone the repository
git clone --recursive https://github.com/SakitsunaAI/Azamboa.git
cd Azamboa

# 3. Build with Extreme Optimization
mkdir build
cd build
cmake .. -A x64 -DCMAKE_BUILD_TYPE=Release -DENABLE_LTO=ON
cmake --build . --config Release --parallel
```
