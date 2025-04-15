# Use CPM.CMake dependency manager
include(${CMAKE_MODULE_PATH}/CPM.cmake)

# Dependencies
CPMAddPackage("gh:libsdl-org/SDL#release-3.2.10")
CPMAddPackage("gh:g-truc/glm#release-1-0-2")
CPMAddPackage("gh:libsdl-org/SDL_ttf#release-3.2.2")