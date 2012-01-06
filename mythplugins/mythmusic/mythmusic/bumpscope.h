#ifndef BUMPSCOPE
#define BUMPSCOPE

#include "mainvisual.h"
#include "config.h"

#include <vector>
using namespace std;

#define MAX_PHONGRES 1024

class BumpScope : public VisualBase
{
public:
    BumpScope();
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

    QImage *m_image;

    QSize m_size;

    unsigned int m_color;
    unsigned int m_x;
    unsigned int m_y;
    unsigned int m_width;
    unsigned int m_height;
    unsigned int m_phongrad;

    bool m_color_cycle;
    bool m_moving_light;
    bool m_diamond;

    int m_bpl;

    vector<vector<unsigned char> > m_phongdat;
    unsigned char *m_rgb_buf;
    double m_intense1[256];
    double m_intense2[256];

    int m_iangle;
    int m_ixo;
    int m_iyo;
    int m_ixd;
    int m_iyd;
    int m_ilx;
    int m_ily;
    int m_was_moving;
    int m_was_color;
    double m_ih;
    double m_is;
    double m_iv;
    double m_isd;
    int m_ihd;
    unsigned int m_icolor;
};


#endif
