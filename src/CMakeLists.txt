CMAKE_MINIMUM_REQUIRED (VERSION 2.8.11)
include(ExternalProject)

set(PROJ_NAME lootcli)

CMAKE_POLICY(SET CMP0020 NEW)
#CMAKE_POLICY(SET CMP0043 NEW)

SET(PROJ_AUTHOR Tannin)
SET(PROJ_ARCH x86)
if(CMAKE_SIZEOF_VOID_P EQUAL 8)
  SET(PROJ_ARCH x64)
endif()


SET(${PROJ_NAME}_SRCS
  game_settings.cpp
  lootthread.cpp
  main.cpp)

SET(${PROJ_NAME}_HDRS
  game_settings.h
  lootthread.h)

# Boost
SET(Boost_USE_STATIC_LIBS        OFF)
SET(Boost_USE_MULTITHREADED      ON)
SET(Boost_USE_STATIC_RUNTIME    OFF)
#SET(CMAKE_CXX_FLAGS_RELWITHDEBINFO  "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} /MT")
FIND_PACKAGE(Boost REQUIRED)

INCLUDE_DIRECTORIES(${Boost_INCLUDE_DIRS})

SET(default_project_path "${DEPENDENCIES_DIR}")
GET_FILENAME_COMPONENT(${default_project_path} ${default_project_path} REALPATH)

SET(project_path "${default_project_path}" CACHE PATH "path to the other mo projects")

ExternalProject_Add(cpptoml
                    PREFIX "external"
                    URL "https://github.com/skystrife/cpptoml/archive/2051836a96a25e5a2d5283be7f633a157848f15e.tar.gz"
                    CONFIGURE_COMMAND ""
                    BUILD_COMMAND ""
                    INSTALL_COMMAND "")
ExternalProject_Get_Property(cpptoml SOURCE_DIR)
set(CPPTOML_INCLUDE_DIRS "${SOURCE_DIR}/include")

math(EXPR BITS "8*${CMAKE_SIZEOF_VOID_P}")

if(NOT DEFINED ENV{LIBLOOT_PATH})
set(LIBLOOT_PATH "${DEPENDENCIES_DIR}/libloot" CACHE PATH "path to libloot files")
endif()

include_directories(${CMAKE_SOURCE_DIR}/src
                ${LIBLOOT_PATH}/include
                ${CPPTOML_INCLUDE_DIRS})

LINK_DIRECTORIES(${Boost_LIBRARY_DIRS}
         ${LIBLOOT_PATH})

ADD_EXECUTABLE      (${PROJ_NAME} WIN32 ${${PROJ_NAME}_HDRS} ${${PROJ_NAME}_SRCS})
add_dependencies    (${PROJ_NAME} cpptoml)
TARGET_LINK_LIBRARIES (${PROJ_NAME}
            ${Boost_LIBRARIES}
            loot)

IF (MSVC)
  SET_TARGET_PROPERTIES(${PROJ_NAME} PROPERTIES COMPILE_FLAGS "/std:c++latest")
ENDIF()
IF (MSVC AND CMAKE_SIZEOF_VOID_P EQUAL 4)
  # 32 bits
  SET_TARGET_PROPERTIES(${PROJ_NAME} PROPERTIES LINK_FLAGS "/LARGEADDRESSAWARE")
ENDIF()

IF (NOT "${OPTIMIZE_COMPILE_FLAGS}" STREQUAL "")
  SET_TARGET_PROPERTIES(${PROJ_NAME} PROPERTIES COMPILE_FLAGS_RELWITHDEBINFO ${OPTIMIZE_COMPILE_FLAGS})
ENDIF()
IF (NOT "${OPTIMIZE_LINK_FLAGS}" STREQUAL "")
  SET_TARGET_PROPERTIES(${PROJ_NAME} PROPERTIES LINK_FLAGS_RELWITHDEBINFO ${OPTIMIZE_LINK_FLAGS})
ENDIF()

###############
## Installation

INSTALL(TARGETS ${PROJ_NAME}
        RUNTIME DESTINATION bin/loot)
INSTALL(FILES $<TARGET_PDB_FILE:${PROJ_NAME}>
    DESTINATION pdb)
