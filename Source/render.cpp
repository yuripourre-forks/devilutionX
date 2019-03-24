//HEADER_GOES_HERE

#include "../types.h"

DEVILUTION_BEGIN_NAMESPACE

int WorldBoolFlag = 0;
unsigned int gdwCurrentMask = 0;
// char world_4B3264 = 0;
unsigned char *gpCelFrame = NULL;
unsigned int *gpDrawMask = NULL;
BYTE *gpDrawTbl = NULL;
// char world_4B326D[16] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };

unsigned int RightMask[32] = {
	0xE0000000, 0xF0000000,
	0xFE000000, 0xFF000000,
	0xFFE00000, 0xFFF00000,
	0xFFFE0000, 0xFFFF0000,
	0xFFFFE000, 0xFFFFF000,
	0xFFFFFE00, 0xFFFFFF00,
	0xFFFFFFE0, 0xFFFFFFF0,
	0xFFFFFFFE, 0xFFFFFFFF,
	0xFFFFFFFF, 0xFFFFFFFF,
	0xFFFFFFFF, 0xFFFFFFFF,
	0xFFFFFFFF, 0xFFFFFFFF,
	0xFFFFFFFF, 0xFFFFFFFF,
	0xFFFFFFFF, 0xFFFFFFFF,
	0xFFFFFFFF, 0xFFFFFFFF,
	0xFFFFFFFF, 0xFFFFFFFF,
	0xFFFFFFFF, 0xFFFFFFFF
};

unsigned int LeftMask[32] = {
	0x00000003, 0x0000000F,
	0x0000003F, 0x000000FF,
	0x000003FF, 0x00000FFF,
	0x00003FFF, 0x0000FFFF,
	0x0003FFFF, 0x000FFFFF,
	0x003FFFFF, 0x00FFFFFF,
	0x03FFFFFF, 0x0FFFFFFF,
	0x3FFFFFFF, 0xFFFFFFFF,
	0xFFFFFFFF, 0xFFFFFFFF,
	0xFFFFFFFF, 0xFFFFFFFF,
	0xFFFFFFFF, 0xFFFFFFFF,
	0xFFFFFFFF, 0xFFFFFFFF,
	0xFFFFFFFF, 0xFFFFFFFF,
	0xFFFFFFFF, 0xFFFFFFFF,
	0xFFFFFFFF, 0xFFFFFFFF,
	0xFFFFFFFF, 0xFFFFFFFF
};

unsigned int WallMask[32] = {
	0x00000000, 0x00000000,
	0x00000000, 0x00000000,
	0x00000000, 0x00000000,
	0x00000000, 0x00000000,
	0x00000000, 0x00000000,
	0x00000000, 0x00000000,
	0x00000000, 0x00000000,
	0x00000000, 0x00000000,
	0x00000000, 0x00000000,
	0x00000000, 0x00000000,
	0x00000000, 0x00000000,
	0x00000000, 0x00000000,
	0x00000000, 0x00000000,
	0x00000000, 0x00000000,
	0x00000000, 0x00000000,
	0x00000000, 0x00000000
};

int WorldTbl3x16[48] = {
	0, 0, 0,
	4, 4, 4,
	8, 8, 8,
	12, 12, 12,
	16, 16, 16,
	20, 20, 20,
	24, 24, 24,
	28, 28, 28,
	32, 32, 32,
	36, 36, 36,
	40, 40, 40,
	44, 44, 44,
	48, 48, 48,
	52, 52, 52,
	56, 56, 56,
	60, 60, 60
};

// slope/angle tables, left and right
int WorldTbl17_1[17] = { 0, 4, 8, 16, 24, 36, 48, 64, 80, 100, 120, 144, 168, 196, 224, 256, 288 };
int WorldTbl17_2[17] = { 0, 32, 60, 88, 112, 136, 156, 176, 192, 208, 220, 232, 240, 248, 252, 256, 288 };

/*
 32x32 arch types
 add 8 if light index is 0

	|-| 0x8 (0)
	|-|

	/\  0x9 (1)
	\/

	 /| 0xA (2)
	 \|

	|\  0xB (3)
	|/

	|-| 0xC (4)
	 \|

	|-| 0xD (5)
	|/
*/

__inline void __fastcall CopyLine(BYTE **dst, BYTE **src, int w)
{
	int i;
	unsigned int mask;

	if (gpDrawMask != NULL) {
		mask = *gpDrawMask;
		gpDrawMask--;
	}
	for (i = 0; i < w; i++, (*src)++, (*dst)++, mask <<= 1) {
		if (gpDrawMask != NULL && !(mask & 0x80000000)) {
			if (light_table_index == lightmax) {
				(*dst)[0] = pTransTbl[0][(*dst)[0]];
			} else if (gpDrawTbl) {
				(*dst)[0] = pTransTbl[gpDrawTbl[(*src)[0]]][(*dst)[0]];
			} else {
				(*dst)[0] = pTransTbl[(*src)[0]][(*dst)[0]];
			}
		} else {
			if (light_table_index == lightmax) {
				(*dst)[0] = 0;
			} else if (gpDrawTbl) {
				(*dst)[0] = gpDrawTbl[(*src)[0]];
			} else {
				(*dst)[0] = (*src)[0];
			}
		}
	}
}

void __fastcall drawUpperScreen(BYTE *pBuff)
{
	int i, j;
	BYTE width;
	BYTE *src, *dst, *tbl;
	short cel_type_16;
	DWORD *pFrameTable;

	gpDrawMask = NULL;
	gpDrawTbl = NULL;

	if (cel_transparency_active) {
		if (arch_draw_type == 0) {
			gpDrawMask = &WallMask[31];
		} else if (arch_draw_type == 1) {
			if (block_lvid[level_piece_id] == 1 || block_lvid[level_piece_id] == 3) {
				gpDrawMask = &LeftMask[31];
			}
		} else if (arch_draw_type == 2) {
			if (block_lvid[level_piece_id] == 2 || block_lvid[level_piece_id] == 3) {
				gpDrawMask = &RightMask[31];
			}
		}
	}

	gpCelFrame = (unsigned char *)speed_cel_frame_num_from_light_index_frame_num;
	dst = pBuff;
	pFrameTable = (DWORD *)pDungeonCels;

	if (light_table_index == lightmax) {
		if (level_cel_block & 0x8000)
			level_cel_block = *(_DWORD *)&gpCelFrame[64 * (level_cel_block & 0xFFF)] + (level_cel_block & 0xF000);
		src = &pDungeonCels[pFrameTable[level_cel_block & 0xFFF]];
		cel_type_16 = level_cel_block >> 12;
	} else if (!light_table_index) {
		if (level_cel_block & 0x8000)
			level_cel_block = *(_DWORD *)&gpCelFrame[64 * (level_cel_block & 0xFFF)] + (level_cel_block & 0xF000);
		src = &pDungeonCels[pFrameTable[level_cel_block & 0xFFF]];
		cel_type_16 = level_cel_block >> 12;
	} else if (!(level_cel_block & 0x8000)) {
		src = &pDungeonCels[pFrameTable[level_cel_block & 0xFFF]];
		gpDrawTbl = (BYTE *)&pLightTbl[256 * light_table_index];
		cel_type_16 = level_cel_block >> 12;
	} else {
		src = &pSpeedCels[*(_DWORD *)&gpCelFrame[4 * (light_table_index + 16 * (level_cel_block & 0xFFF))]];
		cel_type_16 = (level_cel_block >> 12) - 8;
	}

	switch (cel_type_16 & 7) {
	case 0:
		for (i = 32; i; i--, dst -= 768 + 32) {
			CopyLine(&dst, &src, 32);
		}
		break;
	case 1:
		for (i = 32; i; i--, dst -= 768 + 32) {
			for (j = 32; j;) {
				width = *src++;
				if (!(width & 0x80)) {
					j -= width;
					CopyLine(&dst, &src, width);
				} else {
					width = -(char)width;
					dst += width;
					j -= width;
				}
			}
		}
		break;
	case 2:
		for (i = 30; i >= 0; i -= 2, dst -= 768 + 32) {
			dst += i;
			src += i & 2;
			CopyLine(&dst, &src, 32 - i);
		}
		for (i = 2; i != 32; i += 2, dst -= 768 + 32) {
			dst += i;
			src += i & 2;
			CopyLine(&dst, &src, 32 - i);
		}
		break;
	case 3:
		for (i = 30; i >= 0; i -= 2, dst -= 768 + 32) {
			CopyLine(&dst, &src, 32 - i);
			src += i & 2;
			dst += i;
		}
		for (i = 2; i != 32; i += 2, dst -= 768 + 32) {
			CopyLine(&dst, &src, 32 - i);
			src += i & 2;
			dst += i;
		}
		break;
	case 4:
		for (i = 30; i >= 0; i -= 2, dst -= 768 + 32) {
			dst += i;
			src += i & 2;
			CopyLine(&dst, &src, 32 - i);
		}
		for (i = 16; i; i--, dst -= 768 + 32) {
			CopyLine(&dst, &src, 32);
		}
		break;
	default:
		for (i = 30; i >= 0; i -= 2, dst -= 768 + 32) {
			CopyLine(&dst, &src, 32 - i);
			src += i & 2;
			dst += i;
		}
		for (i = 16; i; i--, dst -= 768 + 32) {
			CopyLine(&dst, &src, 32);
		}
		break;
	}
}

void __fastcall drawLowerScreen(BYTE *pBuff)
{
	drawUpperScreen(pBuff);
}

void __fastcall world_draw_black_tile(BYTE *pBuff)
{
	int i, j, k;
	BYTE *dst;

	dst = pBuff;

	for (i = 30, j = 1; i >= 0; i -= 2, j++, dst -= 768 + 64) {
		dst += i;
		for (k = 0; k < 4 * j; k++)
			*dst++ = 0;
		dst += i;
	}
	for (i = 2, j = 15; i != 32; i += 2, j--, dst -= 768 + 64) {
		dst += i;
		for (k = 0; k < 4 * j; k++)
			*dst++ = 0;
		dst += i;
	}
}

void __fastcall trans_rect(int x, int y, int w, int h)
{
	int row, col;
	BYTE *dst = &gpBuffer[SCREENXY(x, y)];
	for (row = 0; row < h; row++) {
		for (col = 0; col < w; col++) {
			*dst = pTransTbl[0][*dst];
			dst++;
		}
		dst += ROW_PITCH - w;
	}
}

DEVILUTION_END_NAMESPACE
