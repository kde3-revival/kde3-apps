# kweathertodaytest: the Today card of the forecast from canned answers
# of Open-Meteo (no network), for tests (not installed); run-tests.sh
# runs it in a sandbox.
kde3_add_executable(kweathertodaytest NO_INSTALL
  SOURCES
    todaytest.cpp
    ../download.cpp
    ../forecastchart.cpp
    ../json.cpp
    ../localtime.cpp
    ../weatherlib.cpp
    ../weathericons.cpp
    ../weathermodels.cpp
  MOC
    ../download.h
    ../forecastchart.h
    ../weatherlib.h
  LINK
    KDE3::kio
)
target_include_directories(kweathertodaytest PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/..")
