project(libmisc_test)

if(UNIX)
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -O0")
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g -O0")
endif() 
	
#############################################
# Keyvalues test
#############################################
set(SRCS
	test/libmisc_test.cpp)
add_executable(libmisc_test ${SRCS})
#############################################


#############################################
# Expr
#############################################
set(SRCS 
	test/expr-test.cpp)

add_executable(expr_test ${SRCS})
#############################################

#############################################
# Cairo
#############################################
set(SRCS 
	test/cairo-test.cpp)
add_executable(cairo_test ${SRCS})
target_link_libraries(cairo_test cairo freetype SDL2 GL GLEW)
target_include_directories(cairo_test PRIVATE /usr/include/cairo /usr/include/freetype2)
#############################################