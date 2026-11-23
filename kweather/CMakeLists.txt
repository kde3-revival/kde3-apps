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

kde3_install_icons(kweather)

install(FILES kweather.desktop DESTINATION ${KDE3_DATA_INSTALL_DIR}/kicker/applets)
install(FILES kweatherservice.desktop DESTINATION ${KDE3_SERVICES_INSTALL_DIR})
install(FILES kcmweather.desktop DESTINATION ${KDE3_SERVICES_INSTALL_DIR})
install(FILES eventsrc DESTINATION ${KDE3_DATA_INSTALL_DIR}/kweather)

# the pictures of KWeather 3.5, for the weather icons the icon theme lacks
file(GLOB _pictures "${CMAKE_CURRENT_SOURCE_DIR}/graphics/*.png")
install(FILES ${_pictures} DESTINATION ${KDE3_DATA_INSTALL_DIR}/kweather)
