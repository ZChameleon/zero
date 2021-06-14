add_executable(main main.cpp)

set_target_properties(
	main PROPERTIES
	CXX_STANDARD 20
	CXX_STANDARD_REQUIRED ON
	CXX_EXTENSIONS OFF
)

target_compile_definitions(
	main PRIVATE
	_SILENCE_CLANG_COROUTINE_MESSAGE
)

if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
	if (CMAKE_CXX_SIMULATE_ID STREQUAL "MSVC")
		target_compile_options(main PRIVATE /clang:-frelaxed-template-template-args -fno-delayed-template-parsing)
	else()
		target_compile_options(main PRIVATE -frelaxed-template-template-args)
	endif()
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
		target_compile_options(main PRIVATE -fcoroutines)
endif()

target_include_directories(
	main PRIVATE
	../include
)
