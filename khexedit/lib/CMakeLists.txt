add_subdirectory(controller)
add_subdirectory(codecs)

# used by the khepart part (khexedit2part); no -version-info
# (libtool's 0:0:0): libkhexeditcommon.so.0
kde3_add_library(khexeditcommon SHARED
  VERSION_INFO 0:0:0
  SOURCES
    kcolumn.cpp
    kbordercolumn.cpp
    koffsetcolumn.cpp
    kbuffercolumn.cpp
    kvaluecolumn.cpp
    kcharcolumn.cpp
    kcolumnsview.cpp
    khexedit.cpp
    kbytesedit.cpp
    koffsetformat.cpp
    kdatabuffer.cpp
    kwrappingrobuffer.cpp
    kplainbuffer.cpp
    kfixedsizebuffer.cpp
    kbigbuffer.cpp
    kbuffercursor.cpp
    kbufferlayout.cpp
    kbufferranges.cpp
    kcursor.cpp
    kbufferdrag.cpp
    kwordbufferservice.cpp
    ksectionlist.cpp
    kcoordrangelist.cpp
    kbordercoltextexport.cpp
    koffsetcoltextexport.cpp
    kbuffercoltextexport.cpp
    kvaluecoltextexport.cpp
    kcharcoltextexport.cpp
  MOC
    kbufferdrag.h
    kbytesedit.h
    kcolumnsview.h
    khexedit.h
  LINK_PRIVATE
    khecontroller
    khecodecs
  LINK
    KDE3::kdecore
)
