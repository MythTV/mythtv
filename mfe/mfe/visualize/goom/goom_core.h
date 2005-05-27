#ifndef _GOOMCORE_H
#define _GOOMCORE_H

#include "goomconfig.h"

#define NB_FX 10

void    goom_init (guint32 resx, guint32 resy, int cinemascope);
void    goom_set_resolution (guint32 resx, guint32 resy, int cinemascope);

/*
 * forceMode == 0 : do nothing
 * forceMode == -1 : lock the FX
 * forceMode == 1..NB_FX : force a switch to FX n°forceMode
 */
guint32 *goom_update (gint16 data[2][256], int forceMode);
void    goom_close ();

#endif
