/*
 * File created 11 april 2002 by JeKo <jeko@free.fr>
 */

#ifndef IFS_H
#define IFS_H

#include "goomconfig.h"

typedef struct _ifsPoint
{
	gint32  x, y;
}
IFSPoint;

// init ifs for a (width)x(height) output.
void    init_ifs (int width, int height);

// draw an ifs on the buffer (which size is width * height)
// increment means that we draw 1/increment of the ifs's points
void    ifs_update (guint32 * buffer, guint32 * back, int width, int height, int increment);

// free all ifs's data.
void    release_ifs (void);


/* DONT USE !!! deprecated
 * return a an array of points.
 * WARNING !!! do not free it !!! it also has an internal use..
 */
IFSPoint *draw_ifs (int *nbPoints);

#endif
