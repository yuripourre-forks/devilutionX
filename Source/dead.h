/**
 * @file dead.h
 *
 * Interface of functions for placing dead monsters.
 */
#ifndef __DEAD_H__
#define __DEAD_H__

DEVILUTION_BEGIN_NAMESPACE

#ifdef __cplusplus
extern "C" {
#endif

extern DeadStruct dead[MAXDEAD];
extern int stonendx;

void InitDead();
void AddDead(int dx, int dy, char dv, int ddir);
void SetDead();

#ifdef __cplusplus
}
#endif

DEVILUTION_END_NAMESPACE

#endif /* __DEAD_H__ */
