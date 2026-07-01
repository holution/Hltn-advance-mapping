cmake_minimum_required(VERSION 3.16)
project(hltn-advanced VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(OBS_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../obs-studio")
set(OBS_BIN_DIR "C:/Program Files/obs-studio/bin/64bit")

set(LIBOBS_INCLUDE_DIR "${OBS_ROOT}/libobs")
set(OBS_GRAPHICS_INCLUDE_DIR "${OBS_ROOT}/libobs/graphics")
set(OBS_FRONTEND_INCLUDE_DIR "${OBS_ROOT}/frontend/api")

set(OBS_DLL "${OBS_BIN_DIR}/obs.dll")
set(OBS_IMPLIB "${CMAKE_BINARY_DIR}/libobs.dll.a")

set(FE_DLL "${OBS_BIN_DIR}/obs-frontend-api.dll")
set(FE_IMPLIB "${CMAKE_BINARY_DIR}/libfrontend.dll.a")

add_custom_command(OUTPUT ${OBS_IMPLIB}
  COMMAND ${CMAKE_COMMAND} -E echo "Generating import lib for obs.dll..."
  COMMAND C:/msys64/mingw64/bin/gendef - "${OBS_DLL}" > "${CMAKE_BINARY_DIR}/obs.def" 2>NUL
  COMMAND C:/msys64/mingw64/bin/dlltool -d "${CMAKE_BINARY_DIR}/obs.def" -l "${OBS_IMPLIB}" -D "${OBS_DLL}"
  DEPENDS ${OBS_DLL}
  COMMENT "Creating import library for obs.dll"
  BYPRODUCTS "${CMAKE_BINARY_DIR}/obs.def")

add_custom_command(OUTPUT ${FE_IMPLIB}
  COMMAND ${CMAKE_COMMAND} -E echo "Generating import lib for obs-frontend-api.dll..."
  COMMAND C:/msys64/mingw64/bin/gendef - "${FE_DLL}" > "${CMAKE_BINARY_DIR}/frontend.def" 2>NUL
  COMMAND C:/msys64/mingw64/bin/dlltool -d "${CMAKE_BINARY_DIR}/frontend.def" -l "${FE_IMPLIB}" -D "${FE_DLL}"
  DEPENDS ${FE_DLL}
  COMMENT "Creating import library for obs-frontend-api.dll"
  BYPRODUCTS "${CMAKE_BINARY_DIR}/frontend.def")

add_custom_target(obs_implib DEPENDS ${OBS_IMPLIB} ${FE_IMPLIB})

add_library(obs SHARED IMPORTED)
set_target_properties(obs PROPERTIES
  IMPORTED_LOCATION "${OBS_DLL}"
  IMPORTED_IMPLIB "${OBS_IMPLIB}"
  INTERFACE_INCLUDE_DIRECTORIES
    "${LIBOBS_INCLUDE_DIR};${OBS_GRAPHICS_INCLUDE_DIR};${OBS_FRONTEND_INCLUDE_DIR}")

add_library(obs_frontend SHARED IMPORTED)
set_target_properties(obs_frontend PROPERTIES
  IMPORTED_LOCATION "${FE_DLL}"
  IMPORTED_IMPLIB "${FE_IMPLIB}")

set(SOURCES
  src/plugin-main.cpp
  src/editor-window.cpp
  src/warp-mesh.cpp)

set(HEADERS
  src/editor-window.h
  src/warp-mesh.h)

add_library(hltn-advanced MODULE ${SOURCES} ${HEADERS})
target_include_directories(hltn-advanced PRIVATE src)
target_link_libraries(hltn-advanced PRIVATE obs obs_frontend)

add_dependencies(hltn-advanced obs_implib)

if(WIN32)
  set_target_properties(hltn-advanced PROPERTIES PREFIX "")
endif()
