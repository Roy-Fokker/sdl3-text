# SDL3 + C++23 Project to render text

## Goal
Learn how to use SDL3 to render text to screen while using SDL3-GPU.
Assume we can use SDL3-TTF to draw the text.

## Uses
- CPM.CMake for package management
- Ninja-Build for build engine
- CMake 4.0+ with cmakepresets for configuration and build
  - There is an issue, on my machine, with CMake 4.0. It fails to configure properly. Use v4.0.1
- C++ modules enabled
- Uses C++ Standard Library modules
  - MSVC (Windows only)
  - Clang (Linux with libc++ only)
- Focuses on SDL3 GPU
- HLSL for all shaders

## to be figured out
- Intellisense, ccls and clangd cannot handle modules so don't work correctly

## CMake commands
VSCode will run these automatically.
- On Windows
```shell
  # Configure Project
  cmake --preset windows-default
  # Build Project, parameter order matters
  cmake --build --preset windows-debug
```
- On Linux
```shell
  # Configure Project
  cmake --preset linux-default
  # Build Project, parameter order matters
  cmake --build --preset linux-debug
```

## Example project dependencies
Uses following libraries retrieved from their project repos by CPM.CMake
- SDL3, obviously
- GLM, for math
- DXC, either from Windows SDK (for D3D12 backend) or Vulkan SDK (for Vulkan backend)

## Basic notes on code structure
- As much as is possible, functions will not take ownership of pointers to objects.
- Where able, all SDL GPU types are wrapped into std::unique_ptr with custom deleters, so they self clean on destruction.

### Function flow
- main
  - Application Init
    - SDL init
    - TTF init
    - Window create
    - GPU create
    - High Resolution Clock
  - Application Run
    - Scene Init
      - Set Clear Colour
      - Create Pipeline
        - Load Vertex Shader
        - Load Fragment/Pixel Shader
        - Set Vertex Attributes
        - Set Vertex Buffer Descriptions
        - Build Pipeline
      - Create TTF GPU Text Engine
      - Create TTF Font
      - Create TTF Text run with Text Engine and Font
      - Get Text mesh and texture from TTF Text
      - Create Perspective projection matrix
      - Create Text mesh transform matrix
    - Loop till quit event
      - Handle SDL events
        - Handle Keyboard and Mouse Inputs
      - Update Application State
      - Draw
        - Acquire GPU Command Buffer
        - Acquire Next swapchain image
        - Create Color Target
        - Begin GPU Render Pass
          - Bind Pipeline
          - Bind Vertex and Index buffers
          - Bind Texture and Sampler
          - Draw Indexed Primitives
        - End GPU Render Pass
        - Submit Command Buffer
      - Clock Tick
    - Clean/Destroy Scene
  - Application Stop
    - Destroy GPU
    - Destroy Window
    - TTF Quit
    - SDL Quit

## references
- Building on linux with Clang and libc++ for module support, https://mattbolitho.github.io/posts/vcpkg-with-libcxx/
- CMake 3.30 magic encantations, https://www.kitware.com/import-std-in-cmake-3-30/
- CMake module compilations, https://www.kitware.com/import-cmake-the-experiment-is-over/
