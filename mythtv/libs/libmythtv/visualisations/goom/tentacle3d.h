#ifndef TENTACLE3D_H
#define TENTACLE3D_H

void tentacle_new (void);
void tentacle_update(int *buf, int *back, int W, int H, short[2][512], float, int drawit);
void tentacle_free (void);

#endif // TENTACLE3D_H
