#define CAIRO_TEXT_IMPLEMENTATION
#include "../code/CairoText.hpp"

#include <SDL2/SDL.h>
#include <SDL2/SDL_video.h>
#include <GL/glew.h>
#include <SDL2/SDL_opengl.h>

int main(int argc,char** argv) {
	SDL_Init(SDL_INIT_VIDEO);
	SDL_Window* window = SDL_CreateWindow("CairoTest", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 
		500, 500, SDL_WINDOW_OPENGL);
	auto context = SDL_GL_CreateContext(window);
	SDL_GL_MakeCurrent(window, context);
	
	glewExperimental = 1;
	if(!glewInit())
		return -1;
	
	while(1) {
		glClearColor(0,0,0,255);
		glClear(GL_COLOR_BUFFER_BIT);
		
		SDL_GL_SwapWindow(window);
	}
	
	SDL_DestroyWindow(window);
}