#ifndef BUMPSCOPE
#define BUMPSCOPE

#include "mainvisual.h"
#include "config.h"

#include <vector>
using namespace std;

#ifdef SDL_SUPPORT
#include <SDL.h>

#define MAX_PHONGRES 1024

class BumpScope : public VisualBase
{
public:
    BumpScope(long int winid);
    virtual ~BumpScope();

    void resize(const QSize &size);
    bool process(VisualNode *node);
    bool draw(QPainter *p, const QColor &back);
    void handleKeyPress(const QString &action) {(void) action;}

private:
    void blur_8(unsigned char *ptr, int w, int h, int bpl);

    void generate_cmap(unsigned int color);
    void generate_phongdat(void);

    void translate(int x, int y, int *xo, int *yo, int *xd, int *yd, 
                   int *angle);

    inline void draw_vert_line(unsigned char *buffer, int x, int y1, int y2);
    void render_light(int lx, int ly);

    void rgb_to_hsv(unsigned int color, double *h, double *s, double *v);
    void hsv_to_rgb(double h, double s, double v, unsigned int *color);

    QSize size;

    SDL_Surface *surface;

    unsigned int m_color;
    unsigned int m_x;
    unsigned int m_y;
    unsigned int m_width;
    unsigned int m_height;
    unsigned int m_phongrad;

    bool color_cycle;
    bool moving_light;
    bool diamond;

    int bpl;

    vector<vector<unsigned char> > phongdat;
    unsigned char *rgb_buf;
    double intense1[256];
    double intense2[256];

    int iangle;
    int ixo;
    int iyo;
    int ixd;
    int iyd;
    int ilx;
    int ily;
    int was_moving;
    int was_color;
    double ih;
    double is;
    double iv;
    double isd;
    int ihd;
    unsigned int icolor;
};


#endif

#endif // __mainvisual_h
