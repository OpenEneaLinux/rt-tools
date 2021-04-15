cmake_minimum_required (VERSION 2.6)

# Create partrt project, which is a shell script
project (partrt
	DESCRIPTION "CPU core partitioning tool"
	VERSION 1.0.0
	HOMEPAGE_URL https://github.com/OpenEneaLinux/rt-tools
	LANGUAGES C
	)

# Enable testing using ctest
enable_testing()
set(CTEST_OUTPUT_ON_FAILURE ON)

# Bitcalc application
add_subdirectory (bitcalc)

# Partrt script
add_subdirectory (partrt)

# count_ticks script
add_subdirectory (count_ticks)

#
# Packaging
#

if( CMAKE_SYSTEM_NAME STREQUAL Linux )
    set( CPACK_GENERATOR "DEB;RPM;TGZ" )
    set( CPACK_SOURCE_GENERATOR ${CPACK_GENERATOR} )
elseif( CMAKE_SYSTEM_NAME STREQUAL Darwin )
    set( CPACK_GENERATOR "DragNDrop" )
    set( CPACK_SOURCE_GENERATOR ${CPACK_GENERATOR} )
elseif( CMAKE_SYSTEM_NAME MATCHES ".*BSD$" )
    set( CPACK_GENERATOR "PKG" )
    set( CPACK_SOURCE_GENERATOR ${CPACK_GENERATOR} )
elseif( CMAKE_SYSTEM_NAME STREQUAL Windows )
    set( CPACK_GENERATOR "NSIS" )
    set( CPACK_SOURCE_GENERATOR ${CPACK_GENERATOR} )
else()
  message( WARNING "${CMAKE_SYSTEM_NAME}: Don't know what package format to use, disabling packaging" )
  set( CPACK_GENERATOR "" )
  set( CPACK_SOURCE_GENERATOR "" )
endif()

if( CPACK_GENERATOR OR CPACK_SOURCE_GENERATOR )
    # Enable component packaging for package generators that do not have this as
    # default, but supports it.
    set( CPACK_RPM_COMPONENT_INSTALL ON )
    set( CPACK_DEB_COMPONENT_INSTALL ON )
    set( CPACK_TBZ2_COMPONENT_INSTALL ON )
    set( CPACK_TGZ_COMPONENT_INSTALL ON )
    set( CPACK_TXZ_COMPONENT_INSTALL ON )
    set( CPACK_TZ_COMPONENT_INSTALL ON )

    # One package per component
    set( CPACK_COMPONENTS_IGNORE_GROUPS 1 )

    set( CPACK_COMPONENT_lib${PROJECT_NAME}_DESCRIPTION "Libraries and headers for ${PROJECT_NAME}" )

    set( CPACK_PACKAGE_CONTACT "Mats G. Liljegren" )

    include( CPack )
endif()

