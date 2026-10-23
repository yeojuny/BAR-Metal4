# check whether given python module exists

macro (CheckPythonModule module)
	execute_process(
		COMMAND
			${PYTHON_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/CheckPythonModule.py" "${module}"
		WORKING_DIRECTORY
			${CMAKE_CURRENT_SOURCE_DIR}
		RESULT_VARIABLE
			python_${module}_RESULT_VAR
		)
	if ( python_${module}_RESULT_VAR EQUAL 0 )
		set( python_${module}_FOUND 1 )
	endif ()
endmacro ()
