option(UNITSYNC_PYTHON_WRAPPER "Create a python wrapper for unitsync, if python is found on the system" TRUE)
if (UNITSYNC_PYTHON_WRAPPER)

#	ADD_LIBRARY(static-unitsync STATIC ${unitsync_files})
# 	Add_Dependencies(static-unitsync generateVersionFiles)
# 	TARGET_LINK_LIBRARIES(static-unitsync ${unitsync_libs})

	set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "../../..")
	FIND_PACKAGE(PythonLibs)
	FIND_PACKAGE(PythonInterp)
	INCLUDE(CheckPythonModule.cmake)
	CheckPythonModule(pybindgen)
	CheckPythonModule(pygccxml)

	if    (PYTHONLIBS_FOUND AND PYTHONINTERP_FOUND AND python_pybindgen_FOUND AND python_pygccxml_FOUND)
		MESSAGE(STATUS "Python unitsync wrapper will be created")

		SET(pythonUnitsyncTarget    pyunitsync)
		SET(pythonWrapperGenerator "${CMAKE_CURRENT_SOURCE_DIR}/binding_generator.py")
		SET(pythonWrapperTest      "${CMAKE_CURRENT_SOURCE_DIR}/binding_test.py")
		SET(pythonWrapperCpp       "${CMAKE_CURRENT_BINARY_DIR}/unitsync_python_wrapper.cc")
		SET_SOURCE_FILES_PROPERTIES( "${pythonWrapperCpp}" PROPERTIES GENERATED TRUE)

		ADD_CUSTOM_COMMAND(
			OUTPUT
				"${pythonWrapperCpp}"
			COMMAND
				"${PYTHON_EXECUTABLE}" "${pythonWrapperGenerator}"
				"${CMAKE_SOURCE_DIR}/tools/unitsync" "${CMAKE_CURRENT_BINARY_DIR}" "${CMAKE_SOURCE_DIR}/rts"
			DEPENDS
				"${pythonWrapperGenerator}"
			WORKING_DIRECTORY
				"${CMAKE_CURRENT_BINARY_DIR}"
			COMMENT
				"  ${pythonUnitsyncTarget}: Generating the wrapper script" VERBATIM
			)
		ADD_CUSTOM_TARGET( "test-${pythonUnitsyncTarget}"
			COMMAND
				"${PYTHON_EXECUTABLE}" "${pythonWrapperTest}"
			DEPENDS
				${pythonUnitsyncTarget}
			WORKING_DIRECTORY
				"${CMAKE_BINARY_DIR}"
			)
		ADD_CUSTOM_TARGET( "test-${pythonUnitsyncTarget}-all"
			COMMAND
				"${PYTHON_EXECUTABLE}" "${pythonWrapperTest}" "1"
			DEPENDS
				${pythonUnitsyncTarget}
			WORKING_DIRECTORY
				"${CMAKE_BINARY_DIR}"
			)

		INCLUDE_DIRECTORIES(${PYTHON_INCLUDE_DIRS} "${CMAKE_CURRENT_SOURCE_DIR}" "${CMAKE_SOURCE_DIR}")
		ADD_LIBRARY(${pythonUnitsyncTarget} MODULE "${pythonWrapperCpp}" ${transformed_unitsync_files})
		SET_TARGET_PROPERTIES(${pythonUnitsyncTarget} PROPERTIES PREFIX "")
		TARGET_LINK_LIBRARIES(${pythonUnitsyncTarget} unitsync )
		if( WIN32 )
			#somewhat hackish, but I have no idea why the libs are either not found or not linked on win otherwise
			TARGET_LINK_LIBRARIES(${pythonUnitsyncTarget} ${MINGWLIBS}/dll/python25.dll )
		endif ()

		INSTALL(TARGETS ${pythonUnitsyncTarget} DESTINATION ${LIBDIR})
	else ()
		MESSAGE(STATUS "Python unitsync wrapper will NOT be created")
		if( NOT PYTHONLIBS_FOUND )
			message( STATUS "\t because python libs were not found" )
		endif ()
		if( NOT PYTHONINTERP_FOUND )
			message( STATUS "\t because python interpreter was not found" )
		endif ()
		if( NOT python_pybindgen_FOUND )
			message( STATUS "\t because pybindgen was not found" )
		endif ()
		if( NOT python_pygccxml_FOUND )
			message( STATUS "\t because pygccxml was not found" )
		endif ()
	endif ()
endif ()
