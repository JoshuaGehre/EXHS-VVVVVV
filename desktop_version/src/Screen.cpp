#include "Screen.h"

#include "FileSystemUtils.h"
#include "GraphicsUtil.h"
#include "GraphicsResources.h"

#include <stdlib.h>
#include <cstring>
#include <physfs.h>

void Screen::init()
{
	m_window = NULL;
	m_renderer = NULL;
	m_screenTexture = NULL;
	m_screen = NULL;
	isWindowed = true;
	stretchMode = 0;
	isFiltered = false;
	filterSubrect.x = 1;
	filterSubrect.y = 1;
	filterSubrect.w = 318;
	filterSubrect.h = 238;

	SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "nearest");

#if defined(__SWITCH__) || defined(__ANDROID__)
	SDL_CreateWindowAndRenderer(
		1920,
		1080,
		SDL_WINDOW_FULLSCREEN_DESKTOP,
		&m_window,
		&m_renderer
	);
#else
	// Uncomment this next line when you need to debug -flibit
	// SDL_SetHintWithPriority(SDL_HINT_RENDER_DRIVER, "software", SDL_HINT_OVERRIDE);
	// FIXME: m_renderer is also created in Graphics::processVsync()!
	SDL_CreateWindowAndRenderer(
		640,
		480,
		SDL_WINDOW_HIDDEN | SDL_WINDOW_RESIZABLE,
		&m_window,
		&m_renderer
	);
	SDL_SetWindowTitle(m_window, "VVVVVV: Community Edition");
#ifndef __EMSCRIPTEN__
	SDL_Surface *icon = LoadImage("VVVVVV.png");
	SDL_SetWindowIcon(m_window, icon);
	SDL_FreeSurface(icon);
#endif
#endif

	// FIXME: This surface should be the actual backbuffer! -flibit
	m_screen = SDL_CreateRGBSurface(
		0,
		320,
		240,
		32,
		0x00FF0000,
		0x0000FF00,
		0x000000FF,
		0xFF000000
	);
	// ALSO FIXME: This SDL_CreateTexture() is duplicated in Graphics::processVsync()!
	m_screenTexture = SDL_CreateTexture(
		m_renderer,
		SDL_PIXELFORMAT_ARGB8888,
		SDL_TEXTUREACCESS_STREAMING,
		320,
		240
	);

	badSignalEffect = false;

	initialized = true;

	ResizeScreen(-1, -1);
}

void Screen::ResizeScreen(int x, int y)
{
	if (!initialized) return;
#if !defined(__SWITCH__)
	int resX = 320;
	int resY = 240;
	if (x != -1 && y != -1)
	{
		// This is a user resize!
		resX = x;
		resY = y;
	}

	if (stretchMode > 2) {
		resX *= stretchMode - 2;
		resY *= stretchMode - 2;
		SDL_SetWindowResizable(m_window, SDL_FALSE);
		SDL_SetWindowSize(m_window, resX, resY);
	} else {
		SDL_SetWindowResizable(m_window, SDL_TRUE);
	}

	if(!isWindowed)
	{
		int result = SDL_SetWindowFullscreen(m_window, SDL_WINDOW_FULLSCREEN_DESKTOP);
		if (result != 0)
		{
			printf("Error: could not set the game to fullscreen mode: %s\n", SDL_GetError());
			return;
		}
	}
	else
	{
		int result = SDL_SetWindowFullscreen(m_window, 0);
		if (result != 0)
		{
			printf("Error: could not set the game to windowed mode: %s\n", SDL_GetError());
			return;
		}
		if (x != -1 && y != -1)
		{
			SDL_SetWindowSize(m_window, resX, resY);
			SDL_SetWindowPosition(m_window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
		}
	}
#endif
	if (stretchMode == 1)
	{
		int winX, winY;
		SDL_GetWindowSize(m_window, &winX, &winY);
		int result = SDL_RenderSetLogicalSize(m_renderer, winX, winY);
		if (result != 0)
		{
			printf("Error: could not set logical size: %s\n", SDL_GetError());
			return;
		}
		result = SDL_RenderSetIntegerScale(m_renderer, SDL_FALSE);
		if (result != 0)
		{
			printf("Error: could not set scale: %s\n", SDL_GetError());
			return;
		}
	}
	else
	{
		SDL_RenderSetLogicalSize(m_renderer, 320, 240);
		int result = SDL_RenderSetIntegerScale(m_renderer, (SDL_bool) (stretchMode == 2));
		if (result != 0)
		{
			printf("Error: could not set scale: %s\n", SDL_GetError());
			return;
		}
	}
	SDL_ShowWindow(m_window);
}

void Screen::GetWindowSize(int* x, int* y)
{
	SDL_GetWindowSize(m_window, x, y);
}

void Screen::UpdateScreen(SDL_Surface* buffer, SDL_Rect* rect )
{
	if((buffer == NULL) && (m_screen == NULL) )
	{
		return;
	}

	if(badSignalEffect)
	{
		buffer = ApplyFilter(buffer);
	}

	if(game.cutemode)
	{
		buffer = ApplyCuteFilter(buffer);
	}
	else if(game.allymode)
	{
		buffer = ApplyAllyFilter(buffer);
	}
	else if(game.misamode)
	{
		buffer = ApplyMisaFilter(buffer);
	}

	FillRect(m_screen, 0x000);
	BlitSurfaceStandard(buffer,NULL,m_screen,rect);

	if(badSignalEffect)
	{
		SDL_FreeSurface(buffer);
	}

}

const SDL_PixelFormat* Screen::GetFormat()
{
	return m_screen->format;
}

void Screen::FlipScreen()
{
	SDL_UpdateTexture(
		m_screenTexture,
		NULL,
		m_screen->pixels,
		m_screen->pitch
	);
	SDL_RenderCopy(
		m_renderer,
		m_screenTexture,
		isFiltered ? &filterSubrect : NULL,
		NULL
	);
	SDL_RenderPresent(m_renderer);
#ifndef __EMSCRIPTEN__
	SDL_RenderClear(m_renderer);
#endif
	SDL_FillRect(m_screen, NULL, 0x00000000);
}

void Screen::toggleFullScreen()
{
	isWindowed = !isWindowed;
	ResizeScreen(-1, -1);
}

void Screen::toggleStretchMode()
{
	stretchMode = (stretchMode + 1) % 7;
	ResizeScreen(-1, -1);
}

void Screen::toggleLinearFilter()
{
	isFiltered = !isFiltered;
	SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, isFiltered ? "linear" : "nearest");
	SDL_DestroyTexture(m_screenTexture);
	m_screenTexture = SDL_CreateTexture(
		m_renderer,
		SDL_PIXELFORMAT_ARGB8888,
		SDL_TEXTUREACCESS_STREAMING,
		320,
		240
	);
}
