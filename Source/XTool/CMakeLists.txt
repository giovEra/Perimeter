
OPTION(OPTION_USE_BACKTRACE "Use libbacktrace if present" OFF)

#GameMath retrieval
FetchContent_Declare(gamemath
    GIT_REPOSITORY    https://github.com/TheAssemblyArmada/GameMath
    GIT_TAG           origin/master
    GIT_SHALLOW       ON
)
FetchContent_MakeAvailable(gamemath)

#Workaround for some compilers that not recognize __always_inline
target_compile_definitions(gamemath_object_lib PUBLIC -D__always_inline=)
target_compile_definitions(gamemath_static_lib PUBLIC -D__always_inline=)

INCLUDE_DIRECTORIES(
    BEFORE .
    "${gamemath_SOURCE_DIR}/include"
)

SET(XTool_SRCS
    xmath/xmath.cpp
    xmath/gamemath.cpp
    xmath/std.cpp
    XBUFFER/XBCNVIN.cpp
    XBUFFER/XBCNVOUT.cpp
    XBUFFER/XBCORE.cpp
    XBUFFER/XBSEARCH.cpp
    XSTREAM/XSCNVOUT.cpp
    XSTREAM/XSSERV.cpp
    XSTREAM/XSRDWR.cpp
    XSTREAM/XSCNVIN.cpp
    XSTREAM/XSENLV.cpp
    XSTREAM/XSCORE.cpp
    xerrhand.cpp
    XUTIL/XUTIL.cpp
    XUTIL/XClock.cpp
    files/files.cpp
    codepages/codepages.cpp
)

SET(XTool_LINK_LIBS gamemath_static_lib ZLIB::ZLIB)

IF(NOT OPTION_DISABLE_STACKTRACE)
    #https://www.boost.org/doc/libs/1_78_0/doc/html/stacktrace/configuration_and_build.html
    IF(MSVC_CL_BUILD)
        #Use WinDbg for MSVC
        MESSAGE("Using WinDbg")
        ADD_DEFINITIONS(-DBOOST_STACKTRACE_USE_WINDBG)
        SET(XTool_LINK_LIBS ${XTool_LINK_LIBS} ole32 dbgeng)
    ELSEIF(PERIMETER_WINDOWS)
        #Use Backtrace if available in Msys/Mingw, else WinDbg
        FIND_PACKAGE(Backtrace)
        IF(Backtrace_FOUND)
            MESSAGE("Using Backtrace")
            ADD_DEFINITIONS(-DBOOST_STACKTRACE_USE_BACKTRACE)
            SET(XTool_LINK_LIBS ${XTool_LINK_LIBS} ${Backtrace_LIBRARIES})
        ELSE()
            MESSAGE("Using WinDbg")
            ADD_DEFINITIONS(-DBOOST_STACKTRACE_USE_WINDBG)
            SET(XTool_LINK_LIBS ${XTool_LINK_LIBS} ole32 dbgeng)
        ENDIF()
    ELSE()
        #Use ADDR2LINE in POSIX unless Backtrace is wanted
        FIND_PACKAGE(Backtrace)
        IF(Backtrace_FOUND AND OPTION_USE_BACKTRACE)
            MESSAGE("Using Backtrace")
            INCLUDE_DIRECTORIES(${Backtrace_INCLUDE_DIR})
            ADD_DEFINITIONS(-DBOOST_STACKTRACE_USE_BACKTRACE)
            SET(XTool_LINK_LIBS ${XTool_LINK_LIBS} ${Backtrace_LIBRARIES})
        ELSE()
            MESSAGE("Using Addr2Line")
            ADD_DEFINITIONS(-DBOOST_STACKTRACE_USE_ADDR2LINE)
            # dl is required for dladdr in stacktrace
            SET(XTool_LINK_LIBS ${XTool_LINK_LIBS} dl)
        ENDIF()
    ENDIF()
ENDIF()

ADD_LIBRARY(XTool STATIC ${XTool_SRCS})
target_compile_options(XTool PUBLIC ${PERIMETER_COMPILE_OPTIONS})
TARGET_LINK_LIBRARIES(XTool ${XTool_LINK_LIBS})
