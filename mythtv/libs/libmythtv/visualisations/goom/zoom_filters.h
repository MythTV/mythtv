void    zoom_filter_xmmx (int prevX, int prevY, const unsigned int *expix1, const unsigned int *expix2, const int *brutS, const int *brutD, int buffratio, int precalCoef[16][16]);
int 	zoom_filter_xmmx_supported (void);
void    zoom_filter_mmx (int prevX, int prevY, const unsigned int *expix1, unsigned int *expix2, const int *brutS, const int *brutD, int buffratio, int precalCoef[16][16]);
int 	zoom_filter_mmx_supported (void);

