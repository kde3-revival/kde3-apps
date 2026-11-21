add_subdirectory(knumber)

include_directories("${CMAKE_CURRENT_SOURCE_DIR}/knumber")
add_compile_definitions(_ISOC99_SOURCE)

kde3_add_kdeinit_executable(kcalc
  SOURCES
    kcalc.cpp
    kcalc_button.cpp
    kcalc_const_button.cpp
    kcalc_const_menu.cpp
    kcalc_core.cpp
    kcalcdisplay.cpp
    dlabel.cpp
    stats.cpp
    colors.ui
    general.ui
    constants.ui
    kcalc_settings.kcfgc
  MOC
    dlabel.h
    kcalc.h
    kcalc_button.h
    kcalc_const_button.h
    kcalc_const_menu.h
    kcalcdisplay.h
  LINK
    knumber
    KDE3::kdeui
)

kde3_install_icons(kcalc)

install(FILES kcalc.desktop DESTINATION ${KDE3_XDG_APPS_INSTALL_DIR})
install(FILES kcalc.kcfg DESTINATION ${KDE3_KCFG_INSTALL_DIR})
install(FILES kcalcui.rc DESTINATION ${KDE3_DATA_INSTALL_DIR}/kcalc)
install(FILES kcalcrc.upd DESTINATION ${KDE3_DATA_INSTALL_DIR}/kconf_update)
