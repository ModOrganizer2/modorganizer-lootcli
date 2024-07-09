cmake_minimum_required(VERSION 3.16)

find_package(tomlplusplus CONFIG REQUIRED)
find_package(libloot CONFIG REQUIRED)
find_package(Boost REQUIRED CONFIG COMPONENTS locale)
find_package(nlohmann_json CONFIG REQUIRED)

# avoid CMake error/warning
set_target_properties(libloot::loot PROPERTIES
  MAP_IMPORTED_CONFIG_RELEASE RelWithDebInfo
  MAP_IMPORTED_CONFIG_MINSIZEREL RelWithDebInfo
)

add_executable(lootcli WIN32)
set_target_properties(lootcli PROPERTIES CXX_STANDARD 20)
target_sources(lootcli
	PRIVATE
		game_settings.cpp
		game_settings.h
		lootthread.cpp
		lootthread.h
		main.cpp
		pch.h
		version.h
		version.rc
		${CMAKE_CURRENT_SOURCE_DIR}/../include/lootcli/lootcli.h
)
target_compile_definitions(lootcli PRIVATE _UNICODE UNICODE)
target_include_directories(lootcli PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../include)
target_precompile_headers(lootcli PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/pch.h)
target_link_libraries(lootcli
    PRIVATE libloot::loot Boost::headers Boost::locale
	tomlplusplus::tomlplusplus nlohmann_json::nlohmann_json)

if (MSVC)
	target_compile_options(lootcli
		PRIVATE
		"/MP"
		"/Wall"
		"/external:anglebrackets"
		"/external:W0"
	)
	target_link_options(lootcli
		PRIVATE
		$<$<CONFIG:RelWithDebInfo>:/LTCG /INCREMENTAL:NO /OPT:REF /OPT:ICF>
	)

	set_target_properties(lootcli PROPERTIES VS_STARTUP_PROJECT lootcli)
endif()

# add_library(lootcli-utils )

# add_library(mo2::lootcli ALIAS lootcli)

install(FILES
	$<TARGET_FILE:lootcli>
	$<TARGET_FILE:libloot::loot>
DESTINATION bin/loot)
