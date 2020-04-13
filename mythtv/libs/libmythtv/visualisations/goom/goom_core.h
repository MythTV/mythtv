#ifndef GOOMCORE_H
#define GOOMCORE_H

#include "goomconfig.h"
#include "mythtvexp.h"

#define NB_FX 10

MTV_PUBLIC void goom_init (guint32 resx, guint32 resy, int cinemascope);
MTV_PUBLIC void goom_set_resolution (guint32 resx, guint32 resy, int cinemascope);

/*
 * forceMode == 0 : do nothing
 * forceMode == -1 : lock the FX
 * forceMode == 1..NB_FX : force a switch to FX n°forceMode
 */
MTV_PUBLIC guint32 *goom_update (gint16 data[2][512], int forceMode);
MTV_PUBLIC void    goom_close (void);

#endif // GOOMCORE_H
