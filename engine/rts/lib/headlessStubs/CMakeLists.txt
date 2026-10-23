SET(headlessStubsSources
		"gladstub.cpp"
		"sdlstub.c"
	)

if(RECOIL_USE_SDL3)
	# SDL2's dummy function signatures cannot be mixed with SDL3 opaque handles
	# or events. Auxiliary CPU targets use real SDL3 without creating a window.
	ADD_LIBRARY(headlessStubs STATIC EXCLUDE_FROM_ALL gladstub.cpp)
	target_link_libraries(headlessStubs PUBLIC SDL3::SDL3)
else()
	find_package(SDL2 MODULE REQUIRED)
	ADD_LIBRARY(headlessStubs STATIC EXCLUDE_FROM_ALL ${headlessStubsSources})
	target_link_libraries(headlessStubs PUBLIC $<COMPILE_ONLY:SDL2::SDL2>)
endif()
SET_TARGET_PROPERTIES(headlessStubs PROPERTIES COMPILE_FLAGS "${PIC_FLAG}")

# Explicit transitional boundary for the native scene experiment. SDL must be
# real: linking sdlstub.c would disable windows, events, keyboard and picking.
# Unconverted legacy GL calls remain non-rendering and confer no capabilities.
ADD_LIBRARY(headlessGLStubs STATIC EXCLUDE_FROM_ALL gladstub.cpp)
SET_TARGET_PROPERTIES(headlessGLStubs PROPERTIES COMPILE_FLAGS "${PIC_FLAG}")
