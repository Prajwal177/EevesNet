cmake_minimum_required(VERSION 3.21)

if(CMAKE_HOST_WIN32 AND NOT CMAKE_TOOLCHAIN_FILE)
	set(_vcpkg_root "$ENV{VCPKG_ROOT}")
	if(NOT _vcpkg_root)
		find_program(_vcpkg_executable NAMES vcpkg)
		if(_vcpkg_executable)
			get_filename_component(_vcpkg_root "${_vcpkg_executable}" DIRECTORY)
		endif()
	endif()
	if(NOT EXISTS "${_vcpkg_root}/scripts/buildsystems/vcpkg.cmake")
		message(FATAL_ERROR "Set VCPKG_ROOT to a full vcpkg checkout to use the libmaxminddb manifest dependency.")
	endif()
	set(CMAKE_TOOLCHAIN_FILE "${_vcpkg_root}/scripts/buildsystems/vcpkg.cmake" CACHE FILEPATH "vcpkg toolchain file")
endif()

project(EevesNet LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

if(WIN32 AND NOT Qt6_DIR AND NOT CMAKE_PREFIX_PATH AND NOT "$ENV{CMAKE_PREFIX_PATH}")
	file(GLOB _qt_msvc_kits "C:/Qt/*/msvc2022_64")
	if(_qt_msvc_kits)
		list(SORT _qt_msvc_kits COMPARE NATURAL ORDER DESCENDING)
		list(GET _qt_msvc_kits 0 _qt_msvc_kit)
		list(PREPEND CMAKE_PREFIX_PATH "${_qt_msvc_kit}")
		message(STATUS "Using Qt from ${_qt_msvc_kit}")
	endif()
endif()

find_package(Qt6 6.5 REQUIRED COMPONENTS Network Widgets)
find_package(Qt6 6.5 QUIET COMPONENTS WebEngineWidgets)
find_package(maxminddb CONFIG REQUIRED
	PATHS "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/share/maxminddb"
)

option(EEVESNET_ENABLE_NPCAP "Enable Npcap capture when its SDK is available" ON)
set(NPCAP_SDK_ROOT "" CACHE PATH "Path to the Npcap SDK root directory")
set(GEOLITE2_CITY_DB "${CMAKE_CURRENT_SOURCE_DIR}/GeoLite2-City.mmdb" CACHE FILEPATH "Path to the GeoLite2 City database")

add_executable(EevesNet WIN32
	cpp/main.cpp
	cpp/main_window.cpp
	cpp/main_window.h
	cpp/capture_worker.cpp
	cpp/capture_worker.h
)

target_link_libraries(EevesNet PRIVATE Qt6::Network Qt6::Widgets)
target_include_directories(EevesNet PRIVATE cpp)
target_link_libraries(EevesNet PRIVATE maxminddb::maxminddb)
if(TARGET Qt6::WebEngineWidgets)
	target_link_libraries(EevesNet PRIVATE Qt6::WebEngineWidgets)
	target_compile_definitions(EevesNet PRIVATE EEVESNET_HAS_QT_WEBENGINE=1)
else()
	message(WARNING "Qt WebEngineWidgets was not found. Install the Qt WebEngine component to enable the embedded Leaflet Dashboard map.")
	target_compile_definitions(EevesNet PRIVATE EEVESNET_HAS_QT_WEBENGINE=0)
endif()
file(TO_CMAKE_PATH "${GEOLITE2_CITY_DB}" _geolite2_city_db_path)
target_compile_definitions(EevesNet PRIVATE "EEVESNET_GEOLITE2_DB_PATH=\"${_geolite2_city_db_path}\"")

if(WIN32)
	get_filename_component(_qt_bin_dir "${Qt6_DIR}/../../../bin" ABSOLUTE)
	find_program(WINDEPLOYQT_EXECUTABLE
		NAMES windeployqt
		HINTS "${_qt_bin_dir}"
		REQUIRED
	)
	add_custom_command(TARGET EevesNet POST_BUILD
		COMMAND "${WINDEPLOYQT_EXECUTABLE}" --no-translations
			"$<TARGET_FILE:EevesNet>"
		COMMENT "Deploying Qt runtime dependencies"
		VERBATIM
	)
endif()

if(EEVESNET_ENABLE_NPCAP)
	find_path(NPCAP_INCLUDE_DIR
		NAMES pcap.h
		HINTS "${NPCAP_SDK_ROOT}" "$ENV{NPCAP_SDK_ROOT}"
		PATH_SUFFIXES Include
	)

	if(CMAKE_SIZEOF_VOID_P EQUAL 8)
		set(_npcap_library_suffixes Lib/x64 Lib)
	else()
		set(_npcap_library_suffixes Lib/x86 Lib)
	endif()

	find_library(NPCAP_LIBRARY
		NAMES wpcap
		HINTS "${NPCAP_SDK_ROOT}" "$ENV{NPCAP_SDK_ROOT}"
		PATH_SUFFIXES ${_npcap_library_suffixes}
	)

	if(NPCAP_INCLUDE_DIR AND NPCAP_LIBRARY)
		target_include_directories(EevesNet PRIVATE "${NPCAP_INCLUDE_DIR}")
		target_link_libraries(EevesNet PRIVATE "${NPCAP_LIBRARY}")
		target_compile_definitions(EevesNet PRIVATE EEVESNET_HAS_NPCAP=1)
		message(STATUS "Npcap SDK found: ${NPCAP_INCLUDE_DIR}")
	else()
		message(WARNING "Npcap SDK not found. EevesNet will build without capture support. Set NPCAP_SDK_ROOT to the SDK directory to enable it.")
	endif()
endif()

if(NOT NPCAP_INCLUDE_DIR OR NOT NPCAP_LIBRARY OR NOT EEVESNET_ENABLE_NPCAP)
	target_compile_definitions(EevesNet PRIVATE EEVESNET_HAS_NPCAP=0)
endif()

if(MSVC)
	target_compile_options(EevesNet PRIVATE /W4 /permissive-)
else()
	target_compile_options(EevesNet PRIVATE -Wall -Wextra -Wpedantic)
endif()
