cmake_minimum_required(VERSION 2.8)

if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
  if(DEFINED ENV{VITASDK})
    set(CMAKE_TOOLCHAIN_FILE "$ENV{VITASDK}/share/vita.toolchain.cmake" CACHE PATH "toolchain file")
  else()
    message(FATAL_ERROR "Please define VITASDK to point to your SDK path!")
  endif()
endif()

project(uwifimon)
include("$ENV{VITASDK}/share/vita.cmake" REQUIRED)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wl,-q -Wall -O3 -std=gnu99")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -nostdlib")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -fno-rtti -fno-exceptions")

include_directories(
  ../common
)

add_executable(uwifimon
  uwifimon.c
)

target_link_libraries(uwifimon
  taihen_stub
  SceLibKernel_stub
  kwifimon_stub
  c
  m
  SceAppMgr_stub
  SceAppUtil_stub
  SceAudio_stub
  SceCommonDialog_stub
  SceCompat_stub
  SceCtrl_stub
  SceDisplay_stub
  SceKernelDmacMgr_stub
  SceGxm_stub
  SceIofilemgr_stub
  SceKernelModulemgr_stub
  SceKernelThreadMgr_stub
  SceLibKernel_stub
  SceMtpIfDriver_stub
  ScePgf_stub_weak
  ScePower_stub
  SceProcessmgr_stub
  SceRtc_stub
  SceScreenShot_stub_weak
  SceShellSvc_stub
  SceSysmem_stub
  SceSysmodule_stub
  SceNetPs_stub_weak
)

vita_create_self(${PROJECT_NAME}.suprx ${PROJECT_NAME}
  UNSAFE
  CONFIG ${CMAKE_SOURCE_DIR}/exports.yml
)

vita_create_stubs(${PROJECT_NAME}-stubs ${PROJECT_NAME} ${CMAKE_SOURCE_DIR}/exports.yml
)

install(DIRECTORY ${CMAKE_BINARY_DIR}/${PROJECT_NAME}-stubs/
  DESTINATION lib
  FILES_MATCHING PATTERN "*.a"
)

install(FILES uwifimon.h
  DESTINATION include
)
