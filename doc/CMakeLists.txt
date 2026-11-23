# KDE_DOCS = AUTO: this directory has no documentation files itself

# The handbook of an application follows its KDE3_APPS_<APP> option.
# from kdeutils
foreach(_app ark kcalc kcharselect kdf khexedit ktimer kwallet
        # from kdegraphics
        kruler ksnapshot kview
        # from kdetoys
        kweather
        # new
        kbluetooth)
  string(TOUPPER "${_app}" _APP)
  if(KDE3_APPS_${_APP})
    add_subdirectory(${_app})
  endif()
endforeach()
# the KInfoCenter module of KDiskFree (kdf/kcmdf.cpp)
if(KDE3_APPS_KDF)
  add_subdirectory(kinfocenter)
endif()
