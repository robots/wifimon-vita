cmake_minimum_required(VERSION 2.8)

if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
  if(DEFINED ENV{VITASDK})
    set(CMAKE_TOOLCHAIN_FILE "$ENV{VITASDK}/share/vita.toolchain.cmake" CACHE PATH "toolchain file")
  else()
    message(FATAL_ERROR "Please define VITASDK to point to your SDK path!")
  endif()
endif()

project(WIFIMON)
include("$ENV{VITASDK}/share/vita.cmake" REQUIRED)

set(CMAKE_BUILD_TYPE Debug)

set(VITA_APP_NAME "Wifimon")
set(VITA_TITLEID  "WIFIMON01")
set(VITA_VERSION  "01.00")

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -g -std=gnu11")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -std=c++11")
set(VITA_MKSFOEX_FLAGS "${VITA_MKSFOEX_FLAGS} -d PARENTAL_LEVEL=1")

include_directories(
  ../common
)

link_directories(
  ${CMAKE_CURRENT_BINARY_DIR}
)

add_executable(${PROJECT_NAME}
	patch.c
	dump.c
	util.c
	main.c
	ui.c
)

target_link_libraries(${PROJECT_NAME}
	uwifimon_stub_weak
	taihen_stub
	vita2d
	freetype
	png
	SceDisplay_stub
	SceGxm_stub
	SceIme_stub
	SceCtrl_stub
	ScePgf_stub
	SceAppUtil_stub
	SceAppMgr_stub
	SceCommonDialog_stub
	SceSysmodule_stub
	SceRegistryMgr_stub
	SceVshBridge_stub
	z
	m
	c
	SceNetPs_stub_weak
)

set(VITA_MAKE_FSELF_FLAGS "${VITA_MAKE_FSELF_FLAGS} -a 0x2808000000000000")

vita_create_self(eboot.bin ${PROJECT_NAME} UNSAFE)

vita_create_vpk(${PROJECT_NAME}.vpk ${VITA_TITLEID} eboot.bin
   VERSION ${VITA_VERSION}
   NAME ${VITA_APP_NAME}
   FILE ./DejaVuSansMono.ttf DejaVuSansMono.ttf
   FILE ../uplugin/build/uwifimon.suprx uwifimon.suprx
   FILE ../kplugin/build/kwifimon.skprx kwifimon.skprx
)
