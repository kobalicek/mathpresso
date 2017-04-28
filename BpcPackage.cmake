include(CMakePackageConfigHelpers)
include( CMakeParseArguments )

function( BpcInstallPackage )
	
	cmake_parse_arguments( "PACKAGE"
		"ALL_COMPILERS;USE_HEADER_SUBFOLDER" "COMPATIBILITY;NAMESPACE;NAME;VERSION" "TARGETS" ${ARGN}
	)

	if( NOT PACKAGE_COMPATIBILITY )
		set( PACKAGE_COMPATIBILITY AnyNewerVersion )
	endif()
	
	if( NOT PACKAGE_NAME )
		set( PACKAGE_NAME ${CMAKE_PROJECT_NAME} )
	endif()
	
	set( ns )
	if( PACKAGE_NAMESPACE )
		set( ns "NAMESPACE;${PACKAGE_NAMESPACE}" )
	endif()
	
	string( TOLOWER ${PACKAGE_NAME} package )
	string( TOUPPER ${PACKAGE_NAME} PACKAGE )

	if( NOT PACKAGE_VERSION )
		message( FATAL_ERROR "Missing version argument!" )
	endif()

	if( PACKAGE_ALL_COMPILERS )
		# Version for installation to ALL_COMPIERS
		if( CMAKE_CL_64 )
			set( PLATFORM x64 )
		else()
			set( PLATFORM win32 )
		endif()

		set( CONF_ROOT "cmake/ALL_COMPILERS/${PLATFORM}" )
		set( INST_ROOT "ALL_COMPILERS" )
	else()
		# Version for compiler specific packages
		if (CMAKE_CL_64)
			set( BPC_COMPILER MSVC-64-14.0 )
		else()
			set( BPC_COMPILER MSVC-32-14.0 )
		endif()
		set( CONF_ROOT "cmake/${BPC_COMPILER}" )
		set( INST_ROOT ${BPC_COMPILER} )
	endif()

	set( CONF_DESTINATION "${CONF_ROOT}/${package}-${PACKAGE_VERSION}" )
	set( INST_DESTINATION "${INST_ROOT}/${package}/${package}-${PACKAGE_VERSION}" )

	foreach( config ${CMAKE_CONFIGURATION_TYPES} )
		string( TOUPPER ${config} CONFIG )
		set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_${CONFIG} "${CMAKE_BINARY_DIR}/bin/${config}")
		set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_${CONFIG} "${CMAKE_BINARY_DIR}/bin/${config}")
		set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_${CONFIG} "${CMAKE_BINARY_DIR}/lib/${config}")
	endforeach()

	write_basic_package_version_file(
		"${CMAKE_CURRENT_BINARY_DIR}/${package}-config-version.cmake"
		VERSION ${PACKAGE_VERSION}
		COMPATIBILITY ${PACKAGE_COMPATIBILITY}
	)

	install(FILES "${CMAKE_CURRENT_BINARY_DIR}/${package}-config-version.cmake" 
		DESTINATION ${CONF_DESTINATION} )

	if( PACKAGE_USE_HEADER_SUBFOLDER )
		set( INC_DESTINATION ${INST_DESTINATION}/include/${package} )
	else()
		set( INC_DESTINATION ${INST_DESTINATION}/include )
	endif()
	
	foreach( config ${CMAKE_CONFIGURATION_TYPES} )
		string( TOUPPER ${config} CONFIG )
		install( TARGETS ${PACKAGE_TARGETS} EXPORT ${package}.${config} CONFIGURATIONS ${config} 
			RUNTIME DESTINATION ${INST_DESTINATION}/bin/${config}
			LIBRARY DESTINATION ${INST_DESTINATION}/bin/${config}
			ARCHIVE DESTINATION ${INST_DESTINATION}/lib/${config}
			PUBLIC_HEADER DESTINATION ${INC_DESTINATION}
			INCLUDES DESTINATION ${INST_DESTINATION}/include
		)
		install(EXPORT ${package}.${config} ${ns} DESTINATION ${CONF_DESTINATION} CONFIGURATIONS ${config} FILE ${package}-config.cmake)
	endforeach()
endfunction()