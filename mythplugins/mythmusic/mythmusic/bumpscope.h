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

    void resize(const QSize &size) override; // VisualBase
    bool process(VisualNode *node) override; // VisualBase
    bool draw(QPainter *p, const QColor &back) override; // VisualBase
    void handleKeyPress(const QString &action) override // VisualBase
        {(void) action;}

private:
    static void blur_8(unsigned char *ptr, int w, int h, int bpl);

    void generate_cmap(unsigned int color);
    void generate_phongdat(void);

    void translate(int x, int y, int *xo, int *yo, int *xd, int *yd,
                   int *angle);

    inline void draw_vert_line(unsigned char *buffer, int x, int y1, int y2);
    void render_light(int lx, int ly);

    static void rgb_to_hsv(unsigned int color, double *h, double *s, double *v);
    static void hsv_to_rgb(double h, double s, double v, unsigned int *color);

    QImage        *m_image          {nullptr};

    QSize          m_size           {0,0};

    unsigned int   m_color          {0x2050FF};
    unsigned int   m_x              {0};
    unsigned int   m_y              {0};
    unsigned int   m_width          {800};
    unsigned int   m_height         {600};
    unsigned int   m_phongrad       {800};

    bool           m_color_cycle    {true};
    bool           m_moving_light   {true};
    //bool         m_diamond        {true};

    int            m_bpl            {0};

    vector<vector<unsigned char> > m_phongdat;
    unsigned char *m_rgb_buf        {nullptr};
    double         m_intense1[256];
    double         m_intense2[256];

    int            m_iangle         {0};
    int            m_ixo            {0};
    int            m_iyo            {0};
    int            m_ixd            {0};
    int            m_iyd            {0};
    int            m_ilx            {0};
    int            m_ily            {0};
    int            m_was_moving     {0};
    int            m_was_color      {0};
    double         m_ih             {0.0};
    double         m_is             {0.0};
    double         m_iv             {0.0};
    double         m_isd            {0.0};
    int            m_ihd            {0};
    unsigned int   m_icolor         {0};
};


#endif
