message("common.pri")
!contains(DEFINES, COMMON_PRI) { 
    DEFINES += COMMON_PRI
    message(" ^ included")
    
    INCLUDEPATH += $$PWD
    
    HEADERS += 	$$PWD/tpcontactstub.h 
    
	SOURCES += 	$$PWD/tpcontactstub.cpp
}
