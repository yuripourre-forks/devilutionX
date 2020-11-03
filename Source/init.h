/**
 * @file init.h
 *
 * Interface of routines for initializing the environment, disable screen saver, load MPQ.
 */
#ifndef __INIT_H__
#define __INIT_H__

DEVILUTION_BEGIN_NAMESPACE

#ifdef __cplusplus
extern "C" {
#endif

extern _SNETVERSIONDATA fileinfo;
extern int gbActive;
extern HANDLE hellfire_mpq;
extern WNDPROC CurrentProc;
extern HANDLE diabdat_mpq;
extern bool gbIsSpawn;
extern HANDLE patch_rt_mpq;
#ifdef HELLFIRE
extern HANDLE hfmonk_mpq;
extern HANDLE hfbard_mpq;
extern HANDLE hfbarb_mpq;
#endif

void init_cleanup();
void init_archives();
void init_create_window();
void MainWndProc(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam);
WNDPROC SetWindowProc(WNDPROC NewProc);

/* data */

extern char gszVersionNumber[260];
extern char gszProductName[260];

#ifdef __cplusplus
}
#endif

DEVILUTION_END_NAMESPACE

#endif /* __INIT_H__ */
