/* Based on xqcam.c by Paul Chinn <loomer@svpal.org> */

// C headers
#include <cmath>

// POSIX headers
#include <sys/ipc.h>
#include <sys/shm.h>

// MythTV headers
#include "osd.h"
#include "osdchromakey.h"

#include "mythverbose.h"
#include "videoout_xv.h"
#include "mythxdisplay.h"

#ifdef MMX
extern "C" {
#include "libavcodec/x86/mmx.h"
}
#endif

#define LOC QString("OSDChroma: ")
#define LOC_ERR QString("OSDChroma Error: ")

ChromaKeyOSD::~ChromaKeyOSD(void)
{
    TearDown();
}

bool ChromaKeyOSD::CreateShmImage(QSize area)
{
    if (!videoOutput || area.isEmpty())
        return false;

    uint size = 0;
    MythXDisplay *disp = videoOutput->disp;
    MythXLocker lock(disp);
    Display *d         = disp->GetDisplay();
    int screen_num     = disp->GetScreen();

    XImage *shm_img =
        XShmCreateImage(d, DefaultVisual(d,screen_num),
                        disp->GetDepth(), ZPixmap, 0,
                        &shm_infos, area.width(),area.height());
    if (shm_img)
        size = shm_img->bytes_per_line * (shm_img->height+1) + 128;

    shm_infos.shmid   = 0;
    shm_infos.shmaddr = NULL;
    if (shm_img)
    {
        shm_infos.shmid = shmget(IPC_PRIVATE, size, IPC_CREAT|0777);
        if (shm_infos.shmid >= 0)
        {
            shm_infos.shmaddr = (char*) shmat(shm_infos.shmid, 0, 0);

            shm_img->data = shm_infos.shmaddr;
            shm_infos.readOnly = False;

            XShmAttach(d, &shm_infos);
            disp->Sync(); // needed for FreeBSD?

            // Mark for delete immediately.
            // It won't actually be removed until after we detach it.
            shmctl(shm_infos.shmid, IPC_RMID, 0);
            img = shm_img;
            return true;
        }
    }
    return false;
}

void ChromaKeyOSD::DestroyShmImage(void)
{
    if (!img || !videoOutput)
        return;

    MythXDisplay *disp = videoOutput->disp;
    disp->Lock();
    XShmDetach(disp->GetDisplay(), &shm_infos);
    XFree(img);
    img = NULL;
    disp->Unlock();

    if (shm_infos.shmaddr)
        shmdt(shm_infos.shmaddr);
    if (shm_infos.shmid > 0)
        shmctl(shm_infos.shmid, IPC_RMID, 0);

    memset(&shm_infos, 0, sizeof(XShmSegmentInfo));
}

bool ChromaKeyOSD::Init(QSize new_size)
{
    if (current_size == new_size)
        return true;

    TearDown();

    bool success = CreateShmImage(new_size);
    image = new QImage(new_size, QImage::Format_ARGB32_Premultiplied);
    painter = new MythQImagePainter();

    if (success && image && painter)
    {
        current_size = new_size;
        image->fill(0);
        VERBOSE(VB_PLAYBACK, LOC + QString("Created ChromaOSD size %1x%2")
                                   .arg(current_size.width())
                                   .arg(current_size.height()));
        return true;
    }

    VERBOSE(VB_PLAYBACK, LOC_ERR + QString("Failed to create ChromaOSD."));
    return false;
}

void ChromaKeyOSD::TearDown(void)
{
    DestroyShmImage();

    if (image)
    {
        delete image;
        image = NULL;
    }

    if (painter)
    {
        delete painter;
        painter = NULL;
    }
}

#define MASK     0xFE000000
#define MMX_MASK 0xFE000000FE000000LL

void ChromaKeyOSD::BlendOrCopy(uint32_t colour, const QRect &rect)
{
    int width  = rect.width();
    int height = rect.height();
    if (!width || !height)
        return;

    uint src_stride = image->bytesPerLine();
    uint dst_stride = img->bytes_per_line;
    unsigned char *src = image->bits() +
                         (rect.top() * src_stride) + (rect.left() << 2);
    unsigned char *dst = (unsigned char*)shm_infos.shmaddr +
                         (rect.top() * dst_stride) + (rect.left() << 2);

    if (colour == 0x0)
    {
        for (int i = 0; i < height; i++)
        {
            memcpy(dst, src, width << 2);
            src += src_stride;
            dst += dst_stride;
        }
        return;
    }

    src_stride = src_stride >> 2;
    dst_stride = dst_stride >> 2;

#ifdef MMX
    bool odd_start      = rect.left() & 0x1;
    bool odd_end        = (rect.left() + rect.width()) & 0x1;
    static mmx_t mask   = {MMX_MASK};
    static mmx_t zero   = {0x0000000000000000LL};
    uint32_t *src_start = (uint32_t*)src;
    uint32_t *dst_start = (uint32_t*)dst;
    uint32_t *src_end   = NULL;
    uint32_t *dst_end   = NULL;

    if (odd_end)
    {
        width--;
        src_end = src_start + width;
        dst_end = dst_start + width;
    }

    if (odd_start)
    {
        src += 4; dst += 4;
        width--;
    }

    uint64_t *source = (uint64_t*)src;
    uint64_t *dest   = (uint64_t*)dst;
#else
    uint32_t *source = (uint32_t*)src;
    uint32_t *dest   = (uint32_t*)dst;
#endif

    for (int i = 0; i < height; i++)
    {
#ifdef MMX
        if (odd_start)
        {
            dst_start[0] = (src_start[0] & MASK) ? src_start[0] : colour;
            src_start   += src_stride;
            dst_start   += dst_stride;
        }

        if (odd_end)
        {
            dst_end[0] = (src_end[0] & MASK) ? src_end[0] : colour;
            src_end   += src_stride;
            dst_end   += dst_stride;
        }

        punpckldq_m2r (colour, mm0);
        punpckhdq_r2r (mm0, mm0);
        for (int j = 0; j < (width >> 1); j++)
        {
            movq_m2r    (source[j], mm1);
            pand_m2r    (mask,      mm1);
            pcmpeqd_m2r (zero,      mm1);
            movq_r2r    (mm1,       mm2);
            pand_r2r    (mm0,       mm1);
            pandn_m2r   (source[j], mm2);
            por_r2r     (mm1,       mm2);
            movq_r2m    (mm2,       dest[j]);
        }
        source += src_stride >> 1;
        dest   += dst_stride >> 1;
#else
        for (uint j = 0; j < width; j++)
            dest[j] = (source[j] & MASK) ? source[j] : colour;
        source += src_stride;
        dest   += dst_stride;
#endif
    }

#ifdef MMX
    emms();
#endif
}

/** \fn ChromaKeyOSD::ProcessOSD(OSD*)
 *
 *  \return true if we need a repaint, false otherwise
 */
bool ChromaKeyOSD::ProcessOSD(OSD *osd)
{
    if (!osd || !videoOutput)
        return false;

    QRect osd_rect = videoOutput->GetTotalOSDBounds();
    if (!Init(osd_rect.size()))
        return false;

    bool was_visible = visible;
    QRect video_rect = videoOutput->window.GetDisplayVideoRect();
    QRegion dirty    = QRegion();
    QRegion vis_area = osd->Draw(painter, image, current_size, dirty);
    visible = !vis_area.isEmpty();

    if (dirty.isEmpty() && (video_rect == current_rect))
        return (visible || was_visible);

    if (video_rect != current_rect)
        dirty = osd_rect;

    current_rect       = video_rect;
    uint32_t letterbox = (uint32_t)videoOutput->XJ_letterbox_colour;
    uint32_t colorkey  = (uint32_t)videoOutput->xv_colorkey;

    int boboff = (int) round(
        ((double)current_size.height()) / 456 - 0.00001);
    boboff = (videoOutput->m_deinterlacing &&
              videoOutput->m_deintfiltername == "bobdeint") ? boboff : 0;

    video_rect.adjust(0, boboff, 0, -boboff);
    video_rect = video_rect.intersected(osd_rect);

    QRect top   = QRect(0, 0, osd_rect.width(), video_rect.top());
    QRect left  = QRect(0, video_rect.top(), video_rect.left(), video_rect.height());
    QRect right = QRect(video_rect.left() + video_rect.width(), video_rect.top(),
                        osd_rect.width() - video_rect.width() - video_rect.left(),
                        video_rect.height());
    QRect bot   = QRect(0, video_rect.top() + video_rect.height(), osd_rect.width(),
                        osd_rect.height() - video_rect.top() - video_rect.height());

    QVector<QRect> update = dirty.rects();
    for (int i = 0; i < update.size(); i++)
    {
        BlendOrCopy(letterbox, update[i].intersected(top));
        BlendOrCopy(letterbox, update[i].intersected(left));
        BlendOrCopy(colorkey,  update[i].intersected(video_rect));
        BlendOrCopy(letterbox, update[i].intersected(right));
        BlendOrCopy(letterbox, update[i].intersected(bot));
    }

    return (visible || was_visible);
}

