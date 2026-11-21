include_directories("${CMAKE_CURRENT_SOURCE_DIR}/../../lib")

kde3_add_module(libkhexedit2part
  SOURCES
    khepart.cpp
    khebrowserextension.cpp
    khepartfactory.cpp
  MOC
    khebrowserextension.h
    khepart.h
    khepartfactory.h
  LINK
    khexeditcommon
    KDE3::kparts
    KDE3::kio
)

install(FILES khexedit2part.desktop DESTINATION ${KDE3_SERVICES_INSTALL_DIR})
install(FILES khexedit2partui.rc DESTINATION ${KDE3_DATA_INSTALL_DIR}/khexedit2part)
