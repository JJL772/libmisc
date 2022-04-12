project(libmisc_test)

set(SRCS
	test/libmisc_test.cpp)

if(UNIX)
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3")
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O3")
endif() 
	
add_executable(libmisc_test ${SRCS})
