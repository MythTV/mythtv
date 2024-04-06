#ifndef BUMPSCOPE
#define BUMPSCOPE

#include "mainvisual.h"

#include <vector>

class BumpScope : public VisualBase
{
public:
    BumpScope();
    ~BumpScope() override;

    void resize(const QSize &size) override; // VisualBase
    bool process(VisualNode *node) override; // VisualBase
    bool draw(QPainter *p, const QColor &back) override; // VisualBase
    void handleKeyPress([[maybe_unused]] const QString &action) override {}; // VisualBase

private:
    static void blur_8(unsigned char *ptr, int w, int h, ptrdiff_t bpl);

    void generate_cmap(unsigned int color);
    void generate_phongdat(void);

    void translate(int x, int y, int *xo, int *yo, int *xd, int *yd,
                   int *angle) const;

    inline void draw_vert_line(unsigned char *buffer, int x, int y1, int y2) const;
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
    unsigned int   m_phongRad       {800};

    bool           m_colorCycle     {true};
    bool           m_movingLight    {true};
    //bool         m_diamond        {true};

    ptrdiff_t      m_bpl            {0};

    std::vector<std::vector<unsigned char> > m_phongDat;
    unsigned char *m_rgbBuf         {nullptr};
    std::array<double,256> m_intense1 {};
    std::array<double,256> m_intense2 {};

    int            m_iangle         {0};
    int            m_ixo            {0};
    int            m_iyo            {0};
    int            m_ixd            {0};
    int            m_iyd            {0};
    int            m_ilx            {0};
    int            m_ily            {0};
    int            m_wasMoving      {0};
    int            m_wasColor       {0};
    double         m_ih             {0.0};
    double         m_is             {0.0};
    double         m_iv             {0.0};
    double         m_isd            {0.0};
    int            m_ihd            {0};
    unsigned int   m_icolor         {0};
};


#endif
