#include "diablo.h"
#include "../3rdParty/Storm/Source/storm.h"
#include "display.h"
#include <SDL.h>

namespace dvl {

int sgdwLockCount;
BYTE *gpBuffer;
#ifdef _DEBUG
int locktbl[256];
#endif
static CCritSect sgMemCrit;
HMODULE ghDiabMod;

int refreshDelay;
SDL_Window *window;
SDL_Renderer *renderer;
SDL_Texture *texture;

/** Currently active palette */
SDL_Palette *palette;
unsigned int pal_surface_palette_version = 0;

/** 24-bit renderer texture surface */
SDL_Surface *renderer_texture_surface = nullptr;

/** 8-bit surface wrapper around #gpBuffer */
SDL_Surface *pal_surface;

#ifdef PIXEL_LIGHT
SDL_Surface *tmp_surface;
SDL_Surface *ui_surface;
SDL_Surface *predrawnEllipses[20];
SDL_Texture *ellipsesTextures[20];
int lightReady = 0;
#endif

static void dx_create_back_buffer()
{
	pal_surface = SDL_CreateRGBSurfaceWithFormat(0, BUFFER_WIDTH, BUFFER_HEIGHT, 8, SDL_PIXELFORMAT_INDEX8);
	if (pal_surface == NULL) {
		ErrSdl();
	}

	gpBuffer = (BYTE *)pal_surface->pixels;

#ifndef USE_SDL1
	// In SDL2, `pal_surface` points to the global `palette`.
	if (SDL_SetSurfacePalette(pal_surface, palette) < 0)
		ErrSdl();
#else
	// In SDL1, `pal_surface` owns its palette and we must update it every
	// time the global `palette` is changed. No need to do anything here as
	// the global `palette` doesn't have any colors set yet.
#endif

#ifdef PIXEL_LIGHT
	ui_surface = SDL_CreateRGBSurfaceWithFormat(0, BUFFER_WIDTH, BUFFER_HEIGHT, 8, SDL_PIXELFORMAT_INDEX8);
	if (ui_surface == NULL)
		ErrSdl();

	if (SDL_SetSurfacePalette(ui_surface, palette) < 0)
		ErrSdl();

	tmp_surface = SDL_CreateRGBSurfaceWithFormat(0, BUFFER_WIDTH, BUFFER_HEIGHT, 8, SDL_PIXELFORMAT_INDEX8);
	if (tmp_surface == NULL)
		ErrSdl();

	if (SDL_SetSurfacePalette(tmp_surface, palette) < 0)
		ErrSdl();
#endif
	pal_surface_palette_version = 1;
}

static void dx_create_primary_surface()
{
#ifndef USE_SDL1
	if (renderer) {
		int width, height;
		SDL_RenderGetLogicalSize(renderer, &width, &height);
		Uint32 format;
		if (SDL_QueryTexture(texture, &format, nullptr, nullptr, nullptr) < 0)
			ErrSdl();
		renderer_texture_surface = SDL_CreateRGBSurfaceWithFormat(0, width, height, SDL_BITSPERPIXEL(format), format);
	}
#endif
	if (GetOutputSurface() == nullptr) {
		ErrSdl();
	}
}

void dx_init(HWND hWnd)
{
	SDL_RaiseWindow(window);
	SDL_ShowWindow(window);

	dx_create_primary_surface();
	palette_init();
	dx_create_back_buffer();
}
static void lock_buf_priv()
{
	sgMemCrit.Enter();
	if (sgdwLockCount != 0) {
		sgdwLockCount++;
		return;
	}

	gpBufEnd += (uintptr_t)(BYTE *)pal_surface->pixels;
	gpBuffer = (BYTE *)pal_surface->pixels;
	sgdwLockCount++;
}

void lock_buf(BYTE idx)
{
#ifdef _DEBUG
	locktbl[idx]++;
#endif
	lock_buf_priv();
}

static void unlock_buf_priv()
{
	if (sgdwLockCount == 0)
		app_fatal("draw main unlock error");
	if (!gpBuffer)
		app_fatal("draw consistency error");

	sgdwLockCount--;
	if (sgdwLockCount == 0) {
		gpBufEnd -= (uintptr_t)gpBuffer;
		//gpBuffer = NULL; unable to return to menu
	}
	sgMemCrit.Leave();
}

void unlock_buf(BYTE idx)
{
#ifdef _DEBUG
	if (!locktbl[idx])
		app_fatal("Draw lock underflow: 0x%x", idx);
	locktbl[idx]--;
#endif
	unlock_buf_priv();
}

void dx_cleanup()
{
	if (ghMainWnd)
		SDL_HideWindow(window);
	sgMemCrit.Enter();
	sgdwLockCount = 0;
	gpBuffer = NULL;
	sgMemCrit.Leave();

	if (pal_surface == nullptr)
		return;
	SDL_FreeSurface(pal_surface);
	pal_surface = nullptr;
	SDL_FreePalette(palette);
	SDL_FreeSurface(renderer_texture_surface);
	SDL_DestroyTexture(texture);
	SDL_DestroyRenderer(renderer);
	SDL_DestroyWindow(window);
#ifdef PIXEL_LIGHT
	SDL_FreeSurface(tmp_surface);
	SDL_FreeSurface(ui_surface);
	for (int i = 0; i < 20; i++) {
		SDL_FreeSurface(predrawnEllipses[i]);
		SDL_DestroyTexture(ellipsesTextures[i]);
	}
	lightReady = 0;
#endif
}

void dx_reinit()
{
#ifdef USE_SDL1
	int flags = window->flags;
	window = SDL_SetVideoMode(0, 0, 0, window->flags ^ SDL_FULLSCREEN);
	if (window == NULL) {
		ErrSdl();
	}
#else
	Uint32 flags = 0;
	if (!fullscreen) {
		flags = renderer ? SDL_WINDOW_FULLSCREEN_DESKTOP : SDL_WINDOW_FULLSCREEN;
	}
	if (SDL_SetWindowFullscreen(window, flags)) {
		ErrSdl();
	}
#endif
	fullscreen = !fullscreen;
	force_redraw = 255;
}

void CreatePalette()
{
	palette = SDL_AllocPalette(256);
	if (palette == NULL) {
		ErrSdl();
	}
}

void BltFast(SDL_Rect *src_rect, SDL_Rect *dst_rect)
{
	if (OutputRequiresScaling()) {
		ScaleOutputRect(dst_rect);
		// Convert from 8-bit to 32-bit
		SDL_Surface *tmp = SDL_ConvertSurface(pal_surface, GetOutputSurface()->format, 0);
		if (SDL_BlitScaled(tmp, src_rect, GetOutputSurface(), dst_rect) < 0) {
			SDL_FreeSurface(tmp);
			ErrSdl();
		}
		SDL_FreeSurface(tmp);
	} else {
		// Convert from 8-bit to 32-bit
		if (SDL_BlitSurface(pal_surface, src_rect, GetOutputSurface(), dst_rect) < 0) 
			ErrSdl();
	}
}

/**
 * @brief Limit FPS to avoid high CPU load, use when v-sync isn't available
 */
void LimitFrameRate()
{
	static uint32_t frameDeadline;
	uint32_t tc = SDL_GetTicks() * 1000;
	uint32_t v = 0;
	if (frameDeadline > tc) {
		v = tc % refreshDelay;
		SDL_Delay(v / 1000 + 1); // ceil
	}
	frameDeadline = tc + v + refreshDelay;
}

#ifdef PIXEL_LIGHT
int width, height;
Uint32 format;

void PutPixel32_nolock(SDL_Surface *surface, int x, int y, Uint32 color)
{
	Uint8 *pixel = (Uint8 *)surface->pixels;
	pixel += (y * surface->pitch) + (x * sizeof(Uint32));
	*((Uint32 *)pixel) = color;
}

struct POINT {
	int x;
	int y;
};

POINT gameToScreen(int targetRow, int targetCol)
{
	int playerRow = plr[myplr].WorldX;
	int playerCol = plr[myplr].WorldY;
	int sx = TILE_SIZE * (targetRow - playerRow) + TILE_SIZE * (playerCol - targetCol) + SCREEN_WIDTH / 2;
	if (ScrollInfo._sdir == SDIR_E)
		sx -= TILE_SIZE;
	else if (ScrollInfo._sdir == SDIR_W)
		sx += TILE_SIZE;

	int sy = TILE_SIZE * (targetCol - playerCol) + sx / 2;
	if (ScrollInfo._sdir == SDIR_W)
		sy -= TILE_SIZE;

	POINT ret;
	ret.x = sx;
	ret.y = sy;
	return ret;
}

int mergeChannel(int a, int b, float amount)
{
	float result = (a * amount) + (b * (1 - amount));
	return (int)result;
}

Uint32 blendColors(Uint32 c1, Uint32 c2, float howmuch)
{
	int r = mergeChannel(c1 & 0x0000FF, c2 & 0x0000FF, howmuch);
	int g = mergeChannel((c1 & 0x00FF00) >> 8, (c2 & 0x00FF00) >> 8, howmuch);
	int b = mergeChannel((c1 & 0xFF0000) >> 16, (c2 & 0xFF0000) >> 16, howmuch);
	return r + (g << 8) + (b << 16);
}

void drawRadius(int lid, int row, int col, int radius, int color)
{
	POINT pos = gameToScreen(row, col);
	int sx = pos.x;
	int sy = pos.y;

	int xoff = 0;
	int yoff = 0;

	if (lid != -1) {
		for (int i = 0; i < nummissiles; i++) {
			MissileStruct *mis = &missile[missileactive[i]];
			if (mis->_mlid == lid) {
				xoff = mis->_mixoff;
				yoff = mis->_miyoff;
				break;
			}
		}
	}

	if (lid != plr[myplr]._plid) {
		xoff -= plr[myplr]._pxoff;
		yoff -= plr[myplr]._pyoff;
	}

	sx += xoff;
	sy += yoff;

	int srcx = width;
	int srcy = height;
	int targetx = sx;
	int targety = sy;
	int offsetx = targetx - srcx;
	int offsety = targety - srcy;

	SDL_Rect rect;
	rect.x = offsetx;
	rect.y = offsety;
	rect.w = width * 2;
	rect.h = height * 2;

	Uint8 r = (color & 0xFF0000) >> 16;
	Uint8 g = (color & 0x00FF00) >> 8;
	Uint8 b = (color & 0x0000FF);
	if (SDL_SetTextureColorMod(ellipsesTextures[radius], r, g, b) < 0)
		ErrSdl();
	if (SDL_RenderCopy(renderer, ellipsesTextures[radius], NULL, &rect) < 0)
		ErrSdl();
}

void lightLoop()
{
	for (int i = 0; i < numlights; i++) {
		int lid = lightactive[i];
		drawRadius(lid, LightList[lid]._lx, LightList[lid]._ly, LightList[lid]._lradius, LightList[lid]._lcolor);
	}

	for (int i = 0; i < 100; i++) {
		LightListStruct *it = &staticLights[currlevel][i];
		if (it->_lradius == 0) {
			break;
		}
		drawRadius(-1, it->_lx, it->_ly, it->_lradius, it->_lcolor);
	}
}

void predrawEllipse(int radius)
{
	int sx = width;
	int sy = height;
	int hey = radius * 16;
	for (int x = 0; x < width * 2; x++) {
		for (int y = 0; y < height * 2; y++) {
			//if (x >= 0 && x < SCREEN_WIDTH && y >= 0 && y < SCREEN_HEIGHT) {
			float howmuch;
			float diffx = sx - x;
			float diffy = sy - y;
			float sa = diffx / 32;
			float a = sa * sa;
			float sb = diffy / 16;
			float b = sb * sb;
			float c = hey;
			float ab = a + b;
			if (ab <= c) {
				howmuch = cbrt(ab / c);
				PutPixel32_nolock(predrawnEllipses[radius], x, y, blendColors(0x000000, 0xFFFFFF, howmuch));
			}
			//}
		}
	}
}

void prepareSpellColors()
{
	int orange = 0xff9900;
	//int blue = 0x6699ff;
	int blue = 0x0033cc;
	int green = 0x00ff00;
	//SPL_NULL = 0x0,
	spellColors[SPL_FIREBOLT] = orange;
	//SPL_HEAL = 0x2,
	spellColors[SPL_LIGHTNING] = blue;
	//spellColors[SPL_FLASH] = blue;
	//SPL_IDENTIFY = 0x5,
	spellColors[SPL_FIREWALL] = orange;
	spellColors[SPL_TOWN] = blue;
	//SPL_STONE = 0x8,
	//SPL_INFRA = 0x9,
	//SPL_RNDTELEPORT = 0xA,
	//SPL_MANASHIELD = 0xB,
	spellColors[SPL_FIREBALL] = orange;
	spellColors[SPL_GUARDIAN] = orange;
	spellColors[SPL_CHAIN] = blue;
	spellColors[SPL_WAVE] = orange;
	//SPL_DOOMSERP = 0x10,
	//SPL_BLODRIT = 0x11,
	spellColors[SPL_NOVA] = blue;
	//SPL_INVISIBIL = 0x13,
	//SPL_FLAME = 0x14,
	//SPL_GOLEM = 0x15,
	//SPL_BLODBOIL = 0x16,
	//SPL_TELEPORT = 0x17,
	spellColors[SPL_APOCA] = orange;
	//SPL_ETHEREALIZE = 0x19,
	//SPL_REPAIR = 0x1A,
	//SPL_RECHARGE = 0x1B,
	//SPL_DISARM = 0x1C,
	spellColors[SPL_ELEMENT] = orange;
	spellColors[SPL_CBOLT] = blue;
	spellColors[SPL_HBOLT] = blue;
	//SPL_RESURRECT = 0x20,
	//SPL_TELEKINESIS = 0x21,
	//SPL_HEALOTHER = 0x22,
	//SPL_FLARE = 0x23,
	spellColors[SPL_BONESPIRIT] = green;
}

void prepareLight()
{
	SDL_RenderGetLogicalSize(renderer, &width, &height);
	if (SDL_QueryTexture(texture, &format, nullptr, nullptr, nullptr) < 0)
		ErrSdl();
	for (int i = 1; i <= 15; i++) {
		predrawnEllipses[i] = SDL_CreateRGBSurfaceWithFormat(0, width * 2, height * 2, SDL_BITSPERPIXEL(format), format);
		if (predrawnEllipses[i] == NULL)
			ErrSdl();
		if (SDL_SetSurfaceBlendMode(predrawnEllipses[i], SDL_BLENDMODE_ADD) < 0)
			ErrSdl();
		if (SDL_FillRect(predrawnEllipses[i], NULL, SDL_MapRGB(predrawnEllipses[i]->format, 0, 0, 0)) < 0)
			ErrSdl();
		predrawEllipse(i);
		ellipsesTextures[i] = SDL_CreateTextureFromSurface(renderer, predrawnEllipses[i]);
		if (ellipsesTextures[i] == NULL)
			ErrSdl();
		if (SDL_SetTextureBlendMode(ellipsesTextures[i], SDL_BLENDMODE_ADD) < 0)
			ErrSdl();
	}
}

#endif
void RenderPresent()
{
	SDL_Surface *surface = GetOutputSurface();
	assert(!SDL_MUSTLOCK(surface));

	if (!gbActive) {
		LimitFrameRate();
		return;
	}

#ifndef USE_SDL1
	if (renderer) {
#ifdef PIXEL_LIGHT
		if (testvar3 != 0 && leveltype != DTYPE_TOWN && (redrawLights == 1 || (testvar1 == 1 && redrawLights != -1))) {
			if (lightReady != 1) {
				lightReady = 1;
				prepareSpellColors();
				prepareLight();
			}
			SDL_BlendMode bm;
			switch (testvar5) {
			case 0:
				bm = SDL_BLENDMODE_NONE;
				break;
			case 1:
				bm = SDL_BLENDMODE_BLEND;
				break;
			case 2:
				bm = SDL_BLENDMODE_ADD;
				break;
			case 3:
				bm = SDL_BLENDMODE_MOD;
				break;
			}
			if (SDL_SetTextureBlendMode(texture, bm) < 0)
				ErrSdl();
		} else {
			if (SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_NONE) < 0)
				ErrSdl();
		}
#endif

		if (SDL_UpdateTexture(texture, NULL, surface->pixels, surface->pitch) < 0) //pitch is 2560
			ErrSdl();

		// Clear buffer to avoid artifacts in case the window was resized
		if (SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255) < 0) // TODO only do this if window was resized
			ErrSdl();
		

		if (SDL_RenderClear(renderer) < 0) 
			ErrSdl();

#ifdef PIXEL_LIGHT
		if (testvar3 != 0 && leveltype != DTYPE_TOWN && (redrawLights == 1 || (testvar1 == 1 && redrawLights != -1))) {
			lightLoop();
		}

#endif
		if (SDL_RenderCopy(renderer, texture, NULL, NULL) < 0)
			ErrSdl();
#ifdef PIXEL_LIGHT
		if (testvar3 != 0 && leveltype != DTYPE_TOWN && (redrawLights == 1 || (testvar1 == 1 && redrawLights != -1))) {
			//Setting the color key here because it might change each frame during fadein/fadeout which modify palette
			if (SDL_SetColorKey(ui_surface, SDL_TRUE, PALETTE_TRANSPARENT_COLOR) < 0)
				ErrSdl();
			// Convert from 8-bit to 24-bit
			SDL_Surface *tmp = SDL_ConvertSurface(ui_surface, renderer_texture_surface->format, 0);
			if (tmp == NULL)
				ErrSdl();
			SDL_Texture *ui_texture = SDL_CreateTextureFromSurface(renderer, tmp);
			if (ui_texture == NULL)
				ErrSdl();
			if (SDL_SetTextureBlendMode(ui_texture, SDL_BLENDMODE_BLEND) < 0)
				ErrSdl();
			SDL_Rect rect;
			rect.x = BORDER_LEFT;
			rect.y = BORDER_TOP;
			rect.w = SCREEN_WIDTH;
			rect.h = SCREEN_HEIGHT;
			if (SDL_RenderCopy(renderer, ui_texture, &rect, NULL) > 0)
				ErrSdl();
			if (SDL_SetColorKey(ui_surface, SDL_FALSE, PALETTE_TRANSPARENT_COLOR) < 0)
				ErrSdl();
			SDL_DestroyTexture(ui_texture);
			SDL_FreeSurface(tmp);
			if (testvar1 != 1)
				redrawLights = 0;
		}
#endif
		SDL_RenderPresent(renderer);
	} else {
		if (SDL_UpdateWindowSurface(window) < 0) 
			ErrSdl();
		LimitFrameRate();
	}
#else
	if (SDL_Flip(surface) < 0) 
		ErrSdl();
	
	LimitFrameRate();
#endif
}

void PaletteGetEntries(DWORD dwNumEntries, SDL_Color *lpEntries)
{
	for (DWORD i = 0; i < dwNumEntries; i++) {
		lpEntries[i] = system_palette[i];
	}
}
} // namespace dvl
