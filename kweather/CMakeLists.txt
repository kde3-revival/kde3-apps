# KWeather: the weather applet of the panel, its settings module and the
# DCOP service KWeatherService, which gets the weather from Open-Meteo.

# the applet
kde3_add_module(weather_panelapplet
  SOURCES
    kweather.cpp
    dockwidget.cpp
    weatherbutton.cpp
    forecastpopup.cpp
    forecastchart.cpp
    weathericons.cpp
    weatherIface.skel
    weatherservice.stub
  MOC
    kweather.h
    dockwidget.h
    weatherbutton.h
    forecastpopup.h
    forecastchart.h
  LINK
    KDE3::kutils
)

# its settings
kde3_add_module(kcm_weather
  SOURCES
    kcmweather.cpp
    download.cpp
    json.cpp
    weathermodels.cpp
    weatherservice.stub
  MOC
    kcmweather.h
    download.h
  LINK
    KDE3::kio
)

# the service
kde3_add_executable(kweatherservice
  SOURCES
    main.cpp
    download.cpp
    json.cpp
    localtime.cpp
    weatherlib.cpp
    weathericons.cpp
    weathermodels.cpp
    weatherservice.cpp
    weatherservice.skel
  MOC
    download.h
    weatherlib.h
    weatherservice.h
  LINK
    KDE3::kio
)

# The DCOP files are made by one target that the others wait for.  With
# the Makefile generators a custom command whose outputs are sources of
# several targets runs once in each of them (CMake documents this), so a
# parallel build ran dcopidl on weatherservice.h three times at once and
# the runs raced on weatherservice.kidl.  The macros of newer kdelibs do
# this themselves (_kde3_generated_by_one_target); a second target for the same
# commands would race with theirs, so only for older ones.
if(NOT COMMAND _kde3_generated_by_one_target)
  set(_bin "${CMAKE_CURRENT_BINARY_DIR}")
  add_custom_target(kweather_dcop
    DEPENDS
      "${_bin}/weatherservice.kidl"
      "${_bin}/weatherservice_stub.h"
      "${_bin}/weatherservice_stub.cpp"
      "${_bin}/weatherservice_skel.cpp"
      "${_bin}/weatherIface.kidl"
      "${_bin}/weatherIface_skel.cpp"
  )
  foreach(_target weather_panelapplet kcm_weather kweatherservice)
    add_dependencies(${_target} kweather_dcop)
  endforeach()
endif()

kde3_install_icons(kweather)

install(FILES kweather.desktop DESTINATION ${KDE3_DATA_INSTALL_DIR}/kicker/applets)
install(FILES kweatherservice.desktop DESTINATION ${KDE3_SERVICES_INSTALL_DIR})
install(FILES kcmweather.desktop DESTINATION ${KDE3_SERVICES_INSTALL_DIR})
install(FILES eventsrc DESTINATION ${KDE3_DATA_INSTALL_DIR}/kweather)

# the pictures of KWeather 3.5, for the weather icons the icon theme lacks
file(GLOB _pictures "${CMAKE_CURRENT_SOURCE_DIR}/graphics/*.png")
install(FILES ${_pictures} DESTINATION ${KDE3_DATA_INSTALL_DIR}/kweather)
