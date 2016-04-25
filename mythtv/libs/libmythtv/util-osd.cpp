#include "util-osd.h"
#include "dithertable.h"

#if HAVE_BIGENDIAN
#define R_OI  1
#define G_OI  2
#define B_OI  3
#define A_OI  0
#else
#define R_OI  2
#define G_OI  1
#define B_OI  0
#define A_OI  3
#endif

void yuv888_to_yv12(VideoFrame *frame, MythImage *osd_image,
                    int left, int top, int right, int bottom)
{
    static bool s_bReported;
    bool c_aligned  = !(left % ALIGN_C || top % ALIGN_C);

#ifdef MMX
    if (c_aligned &&
        !(left % ALIGN_X_MMX || right % ALIGN_X_MMX || bottom % ALIGN_C) )
    {
        mmx_yuv888_to_yv12(frame, osd_image, left, top, right, bottom);
        return;
    }

    // This can happen with a non fullscreen window
    if (c_aligned && !s_bReported)
    {
        s_bReported = true;
        LOG(VB_GENERAL, LOG_WARNING, QString(
            "OSD image size is odd; performance will suffer.\n"
            "\t\t\tVideo window position and size must be a multiple of %1x%2.")
                .arg(ALIGN_X_MMX).arg(ALIGN_C));
    }
#endif

    if (c_aligned)
    {
        c_yuv888_to_yv12(frame, osd_image, left, top, right, bottom);
        return;
    }

    if (!s_bReported)
    {
        s_bReported = true;
        LOG(VB_GENERAL, LOG_WARNING, QString(
            "Unable to display the OSD.\n"
            "\t\t\tVideo window position and size must be a multiple of %1x%2.")
                .arg(ALIGN_C).arg(ALIGN_C));
    }
}

#define ASM(code) __asm__ __volatile__(code);
void inline mmx_yuv888_to_yv12(VideoFrame *frame, MythImage *osd_image,
                               int left, int top, int right, int bottom)
{
#ifdef MMX
    unsigned char *src1, *src2, *y1, *y2, *u, *v;
    int y_wrap, src_wrap, u_wrap, v_wrap, width, height;

    width  = right - left;
    height = bottom - top;
    src1 = osd_image->scanLine(top) + (left << 2);
    src2 = src1 + osd_image->bytesPerLine();
    src_wrap = (osd_image->bytesPerLine() << 1)- (width << 2);

    y1 = frame->buf + frame->offsets[0] + (frame->pitches[0] * top) + left;
    y2 = y1 + frame->pitches[0];
    u  = frame->buf + frame->offsets[1] +
        (frame->pitches[1] * (top >> 1)) + (left >> 1);
    v  = frame->buf + frame->offsets[2] +
        (frame->pitches[2] * (top >> 1)) + (left >> 1);
    y_wrap = (frame->pitches[0] << 1) - width;
    u_wrap = frame->pitches[1] - (width >> 1);
    v_wrap = frame->pitches[2] - (width >> 1);

    static long long MMX_MAX = 0xFFFFFFFFFFFFFFFFLL;
    static long long MMX_MIN = 0x0000000000000000LL;
    static long long MMX_255 = 0x00FF00FF00FF00FFLL;
    static long long tmp_u, tmp_v, tmp_a;

    for (int row = 0; row < height; row += 2)
    {
        for (int col = 0; col < (width >> 3); col++)
        {
            // here be pain
            // unpack and luminance - row 1                                     01234567
            ASM("movq %0, %%mm1"::"m"(src1[0]))       // mm1: A2Y2U2V2 A1Y1U1V1 .t......
            ASM("movq %mm1, %mm2")                    // mm2: A2Y2U2V2 A1Y1U1V1 .tt.....
            ASM("punpckhbw %0, %%mm1"::"m"(src1[8]))  // mm1: A4A2Y4Y2 U4U2V4V2 .tt.....
            ASM("punpcklbw %0, %%mm2"::"m"(src1[8]))  // mm2: A3A1Y3Y1 U3U1V3V1 .tt.....
            ASM("movq %mm2, %mm0")                    // mm0: A3A1Y3Y1 U3U1V3V1 ttt.....
            ASM("punpckhbw %mm1, %mm2")               // mm2: A4A3A2A1 Y4Y3Y2Y1 .tA..... AY stage 1
            ASM("punpcklbw %mm1, %mm0")               // mm0: U4U3U2U1 V4V3V2V1 U.A..... UV stage 1
            ASM("movq %0, %%mm3"::"m"(src1[16]))      // mm3: A6Y6U6V6 A5Y5U5V5 U.At....
            ASM("movq %mm3, %mm4")                    // mm4: A6Y6U6V6 A5YU55V5 U.Att...
            ASM("punpckhbw %0, %%mm3"::"m"(src1[24])) // mm3: A8A6Y8Y6 U8U6V8V6 U.Att...
            ASM("punpcklbw %0, %%mm4"::"m"(src1[24])) // mm4: A7A5Y7Y5 U7U5V7V5 U.Att...
            ASM("movq %mm4, %mm1")                    // mm1: A7A5Y7Y5 U7U5V7V5 UtAtt...
            ASM("punpckhbw %mm3, %mm1")               // mm1: A8A7A6A5 Y8Y7Y6Y5 UtAtt... AY stage 2
            ASM("punpcklbw %mm3, %mm4")               // mm4: U8U7U6U5 V8V7V6V5 UtA.V... UV stage 2
            ASM("movq %mm2, %mm3")                    // mm3: A4A3A2A1 Y4Y3Y2Y1 UtAtV...
            ASM("punpckldq %mm1, %mm3")               // mm3: Y8Y7Y6Y5 Y4Y3Y2Y1 UtAYV... 8-1 Y
            ASM("punpckhdq %mm1, %mm2")               // mm2: A8A7A6A5 A4A3A2A1 UtAYV... 8-1 A
            ASM("movq %0, %%mm7"::"m"(MMX_MAX))       // mm7: FFFFFFFF FFFFFFFF U.AYV..t 255
            ASM("psubusb %mm2, %mm7")                 // mm7: A8A7A6A5 A4A3A2A1 U.AYV..t 8-1 (255-a)
            ASM("movq %mm7, %mm6")                    // mm6: A8A7A6A5 A4A3A2A1 U.AYV.tt 8-1 (255-a)
            ASM("movq %mm7, %mm2")                    // mm2: A8A7A6A5 A4A3A2A1 U.AYV.tt 8-1 (255-a)
            ASM("punpckhbw %0, %%mm7"::"m"(MMX_MIN))  // mm7: 00A800A7 00A600A4 U.AYV.tt 8-5 (255-a)
            ASM("punpcklbw %0, %%mm6"::"m"(MMX_MIN))  // mm6: 00A400A3 00A200A1 U.AYV.tt 4-1 (255-a)
            ASM("movq %0, %%mm5"::"m"(*y1))           // mm5: D8D7D6D5 D4D3D2D1 U.AYVttt 8-1 dest
            ASM("movq %mm5, %mm1")                    // mm1: D8D7D6D5 D4D3D2D1 UtAYVttt
            ASM("punpckhbw %0, %%mm5"::"m"(MMX_MIN))  // mm5: 00D800D7 00D600D5 UtAYVttt
            ASM("punpcklbw %0, %%mm1"::"m"(MMX_MIN))  // mm1: 00D400D3 00D200D1 UtAYVttt
            ASM("pmullw %mm7, %mm5")                  // mm5: D8D8D7D7 D6D6D5D5 UtAYVttt 8-5 dest*(255-a)
            ASM("pmullw %mm6, %mm1")                  // mm1: D4D4D3D3 D2D2D1D1 UtAYVtt. 4-1 dest*(255-a)
            ASM("psrlw $8, %mm5")                     // mm5: 00D800D7 00D600D5 UtAYVt.. 8-5 (dest*(255-a))/256
            ASM("psrlw $8, %mm1")                     // mm1: 00D400D3 00D200D1 UtAYVt.. 4-1 (dest*(255-a))/256
            ASM("packuswb %mm5, %mm1")                // mm1: D8D7D6D5 D4D3D2D1 UtAYV... 8-1 (dest*(255-a))/256
            ASM("paddusb %mm1, %mm3")                 // mm3: D8D7D6D5 D4D3D2D1 U.AYV... 8-1 (dest*(255-a))/256+y
            ASM("movq %%mm3, %0":"=m"(*y1):)          //                        U.A.V... row 1 y
            ASM("movq %mm0, %mm1")                    // mm1: U4U3U2U1 V4V3V2V1 uuA.v...
            ASM("punpckhdq %mm4, %mm0")               // mm0: U8U7U6U5 U4U3U2U1 UuA.v... 8-1 u
            ASM("punpckldq %mm4, %mm1")               // mm1: V8V7V6V5 V4V3V2V1 UVA..... 9-1 v
            // store u,v and for sub-sampling
            ASM("movq %%mm0, %0":"=m"(tmp_u):)
            ASM("movq %%mm1, %0":"=m"(tmp_v):)
            ASM("movq %%mm2, %0":"=m"(tmp_a):)
            // unpack and luminance - row 2
            ASM("movq %0, %%mm1"::"m"(src2[0]))       // mm1: A2Y2U2V2 A1Y1U1V1 .t......
            ASM("movq %mm1, %mm2")                    // mm2: A2Y2U2V2 A1Y1U1V1 .tt.....
            ASM("punpckhbw %0, %%mm1"::"m"(src2[8]))  // mm1: A4A2Y4Y2 U4U2V4V2 .tt.....
            ASM("punpcklbw %0, %%mm2"::"m"(src2[8]))  // mm2: A3A1Y3Y1 U3U1V3V1 .tt.....
            ASM("movq %mm2, %mm0")                    // mm0: A3A1Y3Y1 U3U1V3V1 ttt.....
            ASM("punpckhbw %mm1, %mm2")               // mm2: A4A3A2A1 Y4Y3Y2Y1 .tA..... AY stage 1
            ASM("punpcklbw %mm1, %mm0")               // mm0: U4U3U2U1 V4V3V2V1 U.A..... UV stage 1
            ASM("movq %0, %%mm3"::"m"(src2[16]))      // mm3: A6Y6U6V6 A5Y5U5V5 U.At....
            ASM("movq %mm3, %mm4")                    // mm4: A6Y6U6V6 A5YU55V5 U.Att...
            ASM("punpckhbw %0, %%mm3"::"m"(src2[24])) // mm3: A8A6Y8Y6 U8U6V8V6 U.Att...
            ASM("punpcklbw %0, %%mm4"::"m"(src2[24])) // mm4: A7A5Y7Y5 U7U5V7V5 U.Att...
            ASM("movq %mm4, %mm1")                    // mm1: A7A5Y7Y5 U7U5V7V5 UtAtt...
            ASM("punpckhbw %mm3, %mm1")               // mm1: A8A7A6A5 Y8Y7Y6Y5 UtAtt... AY stage 2
            ASM("punpcklbw %mm3, %mm4")               // mm4: U8U7U6U5 V8V7V6V5 UtA.V... UV stage 2
            ASM("movq %mm2, %mm3")                    // mm3: A4A3A2A1 Y4Y3Y2Y1 UtAtV...
            ASM("punpckldq %mm1, %mm3")               // mm3: Y8Y7Y6Y5 Y4Y3Y2Y1 UtAYV... 8-1 Y
            ASM("punpckhdq %mm1, %mm2")               // mm2: A8A7A6A5 A4A3A2A1 UtAYV... 8-1 A
            ASM("movq %0, %%mm7"::"m"(MMX_MAX))       // mm7: FFFFFFFF FFFFFFFF U.AYV..t 255
            ASM("psubusb %mm2, %mm7")                 // mm7: A8A7A6A5 A4A3A2A1 U.AYV..t 8-1 (255-a)
            ASM("movq %mm7, %mm6")                    // mm6: A8A7A6A5 A4A3A2A1 U.AYV.tt 8-1 (255-a)
            ASM("movq %mm7, %mm2")                    // mm2: A8A7A6A5 A4A3A2A1 U.AYV.tt 8-1 (255-a)
            ASM("punpckhbw %0, %%mm7"::"m"(MMX_MIN))  // mm7: 00A800A7 00A600A4 U.AYV.tt 8-5 (255-a)
            ASM("punpcklbw %0, %%mm6"::"m"(MMX_MIN))  // mm6: 00A400A3 00A200A1 U.AYV.tt 4-1 (255-a)
            ASM("movq %0, %%mm5"::"m"(*y2))           // mm5: D8D7D6D5 D4D3D2D1 U.AYVttt 8-1 dest
            ASM("movq %mm5, %mm1")                    // mm1: D8D7D6D5 D4D3D2D1 UtAYVttt
            ASM("punpckhbw %0, %%mm5"::"m"(MMX_MIN))  // mm5: 00D800D7 00D600D5 UtAYVttt
            ASM("punpcklbw %0, %%mm1"::"m"(MMX_MIN))  // mm1: 00D400D3 00D200D1 UtAYVttt
            ASM("pmullw %mm7, %mm5")                  // mm5: D8D8D7D7 D6D6D5D5 UtAYVttt 8-5 dest*(255-a)
            ASM("pmullw %mm6, %mm1")                  // mm1: D4D4D3D3 D2D2D1D1 UtAYVtt. 4-1 dest*(255-a)
            ASM("psrlw $8, %mm5")                     // mm5: 00D800D7 00D600D5 UtAYVt.. 8-5 (dest*(255-a))/256
            ASM("psrlw $8, %mm1")                     // mm1: 00D400D3 00D200D1 UtAYVt.. 4-1 (dest*(255-a))/256
            ASM("packuswb %mm5, %mm1")                // mm1: D8D7D6D5 D4D3D2D1 UtAYV... 8-1 (dest*(255-a))/256
            ASM("paddusb %mm1, %mm3")                 // mm3: D8D7D6D5 D4D3D2D1 U.AYV... 8-1 (dest*(255-a))/256+y
            ASM("movq %%mm3, %0":"=m"(*y2):)          //                        U.A.V... row 1 y
            ASM("movq %mm0, %mm1")                    // mm1: U4U3U2U1 V4V3V2V1 uuA.v...
            ASM("punpckhdq %mm4, %mm0")               // mm0: U8U7U6U5 U4U3U2U1 UuA.v... 8-1 u
            ASM("punpckldq %mm4, %mm1")               // mm1: V8V7V6V5 V4V3V2V1 UVA..... 9-1 v
            // subsample alpha
            ASM("movq %mm2, %mm3")                    // mm3: A8A7A6A5 A4A3A2A1 UVAA.... copy row 2 a
            ASM("movq %0, %%mm4"::"m"(tmp_a))         // mm4: A8A7A6A5 A4A3A2A1 UVAA.... row 1 a
            ASM("movq %mm4, %mm5")                    // mm5: A8A7A6A5 A4A3A2A1 UVAAAA.. copy row 1 a
            ASM("psrlw $8, %mm2")                     // mm2: 00A800A6 00A400A2 UVAAAA.. row 2
            ASM("pand %0, %%mm3"::"m"(MMX_255))       // mm3: 00A700A5 00A300A1 UVAAAA.. row 2
            ASM("psrlw $8, %mm4")                     // mm4: 00A800A6 00A400A2 UVAAAA.. row 1
            ASM("pand %0, %%mm5"::"m"(MMX_255))       // mm5: 00A700A5 00A300A1 UVAAAA.. row 1
            ASM("paddusw %mm5, %mm4")                 // add
            ASM("paddusw %mm4, %mm3")
            ASM("paddusw %mm3, %mm2")
            ASM("psrlw $2, %mm2")                     // mm2: xxA4xxA3 xxA2xxA1 UVA..... /4
            ASM("pand %0, %%mm2"::"m"(MMX_255))       // mm2: 00A400A3 00A200A1 UVA.....
            // subsample u
            ASM("movq %mm0, %mm3")                    // mm3: U8U7U6U5 U4U3U2U1 UVAU.... copy row 2 u
            ASM("movq %0, %%mm4"::"m"(tmp_u))         // mm4: U8U7U6U5 U4U3U2U1 UVAUU... row 1 u
            ASM("movq %mm4, %mm5")                    // mm5: U8U7U6U5 U4U3U2U1 UVAUUU.. copy row 1 u
            ASM("psrlw $8, %mm0")                     // mm0: 00U800U6 00U400U2 UVAUUU.. row 2
            ASM("pand %0, %%mm3"::"m"(MMX_255))       // mm3: 00U700U5 00U300U1 UVAUUU.. row 2
            ASM("psrlw $8, %mm4")                     // mm4: 00U800U6 00U400U2 UVAUUU.. row 1
            ASM("pand %0, %%mm5"::"m"(MMX_255))       // mm5: 00U700U5 00U300U1 UVAUUU.. row 1
            ASM("paddusw %mm5, %mm4")                 // add
            ASM("paddusw %mm4, %mm3")
            ASM("paddusw %mm3, %mm0")
            ASM("psrlw $2, %mm0")                     // mm0: xxU4xxU3 xxU2xxU1 UVA..... /4
            ASM("pand %0, %%mm0"::"m"(MMX_255))       // mm0: 00U400U3 00U200U1 UVA.....
            // blend u
            ASM("movd %0, %%mm3"::"m"(*u))            // mm3: 00000000 D4D3D2D1 UVAt.... 4-1 dest u
            ASM("punpcklbw %0, %%mm3"::"m"(MMX_MIN))  // mm3: 00D400D3 00D200D1 UVAt.... 4-1 dest u
            ASM("pmullw %mm2, %mm3")                  // mm3: 00D400D3 00D200D1 UVAt.... destu * (255-a)
            ASM("psrlw $8, %mm3")                     // mm3: 00D400D3 00D200D1 UVAt.... (destu*(255-a))/256
            ASM("paddusb %mm3, %mm0")                 // mm0: xxR4xxR3 xxR2xxR1 UVA..... (destu*(255-a))/256 + u
            ASM("packuswb %mm1, %mm0")                // mm0: xxxxxxxx U4U3U2U1 UVA..... (destu*(255-a))/256 + u
            ASM("movd %%mm0, %0":"=m"(*u):)           // output                 .VA.....
            // subsample v
            ASM("movq %mm1, %mm3")                    // mm3: U8U7U6U5 U4U3U2U1 .VAV.... copy row 2 v
            ASM("movq %0, %%mm4"::"m"(tmp_v))         // mm4: U8U7U6U5 U4U3U2U1 .VAVV... row 1 v
            ASM("movq %mm4, %mm5")                    // mm5: U8U7U6U5 U4U3U2U1 .VAVVV.. copy row 1 v
            ASM("psrlw $8, %mm1")                     // mm1: 00U800U6 00U400U2 .VAVVV.. row 2
            ASM("pand %0, %%mm3"::"m"(MMX_255))       // mm3: 00U700U5 00U300U1 .VAVVV.. row 2
            ASM("psrlw $8, %mm4")                     // mm4: 00U800U6 00U400U2 .VAVVV.. row 1
            ASM("pand %0, %%mm5"::"m"(MMX_255))       // mm5: 00U700U5 00U300U1 .VAVVV.. row 1
            ASM("paddusw %mm5, %mm4")                 // add
            ASM("paddusw %mm4, %mm3")
            ASM("paddusw %mm3, %mm1")
            ASM("psrlw $2, %mm1")                     // mm1: xxU4xxU3 xxU2xxU1 .VA..... /4
            ASM("pand %0, %%mm1"::"m"(MMX_255))       // mm1: 00U400U3 00U200U1 .VA.....
            // blend v
            ASM("movd %0, %%mm3"::"m"(*v))            // mm3: 00000000 D4D3D2D1 .VAt.... 4-1 dest v
            ASM("punpcklbw %0, %%mm3"::"m"(MMX_MIN))  // mm3: 00D400D3 00D200D1 .VAt.... 4-1 dest v
            ASM("pmullw %mm2, %mm3")                  // mm3: 00D400D3 00D200D1 .V.t.... destv * (255-a)
            ASM("psrlw $8, %mm3")                     // mm3: 00D400D3 00D200D1 .V.t.... (destv*(255-a))/256
            ASM("paddusb %mm3, %mm1")                 // mm1: xxR4xxR3 xxR2xxR1 .V...... (destv*(255-a))/256 + v
            ASM("packuswb %mm2, %mm1")                // mm1: xxxxxxxx V4V3V2V1 .V...... (destv*(255-a))/256 + v
            ASM("movd %%mm1, %0":"=m"(*v):)           // output                 ........

            src1 += 32; src2 += 32; y1 += 8; y2 += 8; u += 4; v += 4;
        }
        y1 += y_wrap; y2 += y_wrap; u+= u_wrap; v += v_wrap;
        src1 += src_wrap; src2 += src_wrap;
    }
    ASM("emms")
#endif
}

void c_yuv888_to_yv12(VideoFrame *frame, MythImage *osd_image,
                             int left, int top, int right, int bottom)
{
    const int width  = right - left;
    const int height = bottom - top;

    unsigned char *udest = frame->buf + frame->offsets[1];
    unsigned char *vdest = frame->buf + frame->offsets[2];
    udest  += frame->pitches[1] * (top >> 1) + (left >> 1);
    vdest  += frame->pitches[2] * (top >> 1) + (left >> 1);

    unsigned char *y1 = frame->buf + frame->offsets[0]
                      + frame->pitches[0] * top + left;
    unsigned char *y3 = y1 + frame->pitches[0];
    const int y_wrap = (frame->pitches[0] << 1) - width;

    const unsigned char *src = osd_image->scanLine(top) + left * sizeof(QRgb);
    const int bpl = osd_image->bytesPerLine();

    for (int row = 0; row < height; row += 2)
    {
        const QRgb *p1 = reinterpret_cast<const QRgb* >(src);
        const QRgb *p3 = reinterpret_cast<const QRgb* >(src + bpl);

        for (int col = 0, maxcol = width / 2; col < maxcol; ++col)
        {
            QRgb rgb1 = p1[0], rgb2 = p1[1], rgb3 = p3[0], rgb4 = p3[1];
            int alpha1 = 255 - qAlpha(rgb1);
            int alpha2 = 255 - qAlpha(rgb2);
            int alpha3 = 255 - qAlpha(rgb3);
            int alpha4 = 255 - qAlpha(rgb4);
            int alphaUV = (alpha1 + alpha2 + alpha3 + alpha4) >> 2;

            // Note - in the code below qRed is not really red, it
            // is Y, or luminance. qGreen is U chrominance and qBlue
            // is V chrominance.
            if (alphaUV == 0)
            {
                // Special case optimized for raspberry pi
                // This code handles opaque images. In this
                // case it is not necessary to merge in the background.
                y1[0] = qRed(rgb1);
                y1[1] = qRed(rgb2);
                y3[0] = qRed(rgb3);
                y3[1] = qRed(rgb4);
                udest[col] = (qGreen(rgb1) + qGreen(rgb2) + qGreen(rgb3) + qGreen(rgb4)) >> 2;
                vdest[col] = (qBlue(rgb1)  + qBlue(rgb2)  + qBlue(rgb3)  + qBlue(rgb4)) >> 2;
            }
            else if (alphaUV < 255)
            {
                // This code handles transparency. it is skipped
                // if the image is invisible (alphaUV == 255)
                // This section could handle all cases, but for
                // optimizing CPU usage three cases are handled differently.
                y1[0] = ((y1[0] * alpha1) >> 8) + qRed(rgb1);
                y1[1] = ((y1[1] * alpha2) >> 8) + qRed(rgb2);
                y3[0] = ((y3[0] * alpha3) >> 8) + qRed(rgb3);
                y3[1] = ((y3[1] * alpha4) >> 8) + qRed(rgb4);

                int u = (qGreen(rgb1) + qGreen(rgb2) + qGreen(rgb3) + qGreen(rgb4)) >> 2;
                udest[col] = ((udest[col] * alphaUV) >> 8) + u;

                int v = (qBlue(rgb1)  + qBlue(rgb2)  + qBlue(rgb3)  + qBlue(rgb4)) >> 2;
                vdest[col] = ((vdest[col] * alphaUV) >> 8) + v;
            }
            y1 += 2, y3 += 2;
            p1 += 2, p3 += 2;
        }

        y1 += y_wrap, y3 += y_wrap;
        udest += frame->pitches[1];
        vdest += frame->pitches[2];
        src += bpl << 1;
    }
}

void yuv888_to_i44(unsigned char *dest, MythImage *osd_image, QSize dst_size,
                   int left, int top, int right, int bottom, bool ifirst)
{
    int width, ashift, amask, ishift, imask, src_wrap, dst_wrap;
    unsigned char *src, *alpha, *dst;
    const unsigned char *dmp;

    width  = right - left;
    ashift = ifirst ? 0 : 4;
    amask  = ifirst ? 0x0f : 0xf0;
    ishift = ifirst ? 4 : 0;
    imask  = ifirst ? 0xf0 : 0x0f;

    src   = osd_image->scanLine(top) + (left << 2) + R_OI;
    alpha = osd_image->scanLine(top) + (left << 2) + A_OI;
    dst   = dest + dst_size.width() * top + left;
    dst_wrap = dst_size.width() - width;
    src_wrap = osd_image->bytesPerLine() - (width << 2);

    for (int row = top; row < bottom; row++)
    {
        dmp = DM[row & (DM_HEIGHT - 1)];
        for (int col = left; col < right; col++)
        {
            int grey;

            grey = *src + ((dmp[col & (DM_WIDTH - 1)] << 2) >> 4);
            grey = (grey - (grey >> 4)) >> 4;

            *dst = (((*alpha >> 4) << ashift) & amask) |
                    (((grey) << ishift) & imask);

            alpha += 4;
            src   += 4;
            dst++;
        }
        alpha += src_wrap;
        src   += src_wrap;
        dst   += dst_wrap;
    }
}
