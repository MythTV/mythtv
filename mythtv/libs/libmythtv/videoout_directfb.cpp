/* videoout_directfb.cpp
*
* Use DirectFB for video output while watvhing TV
* Most of this is ripped from videoout_* and mplayer's vo_directfb */

#include <iostream>

using namespace std;

#include <qapplication.h>
#include <qwidget.h>
#include <qevent.h>
#include "videoout_directfb.h"
#include "filtermanager.h"

extern "C" {
#include "../libavcodec/avcodec.h"
}

#define DFB_KBID_OFFSET 62976
static const unsigned int QT_KEYS[DIKI_NUMBER_OF_KEYS][2] =
{
	{0xffff,0 }, // unknown key
	{0x41,	0x61},	// a...z
	{0x42,	0x62},
	{0x43,	0x63},
	{0x44,	0x64},
	{0x45,	0x65},
	{0x46,	0x66},
	{0x47,	0x67},
	{0x48,	0x68},
	{0x49,	0x69},
	{0x4a,	0x6a},
	{0x4b,	0x8b},
	{0x4c,	0x6c},
	{0x4d,	0x6d},
	{0x4e,	0x6e},
	{0x4f,	0x6f},
	{0x50,	0x70},
	{0x51,	0x71},
	{0x52,	0x72},
	{0x53,	0x73},
	{0x54,	0x74},
	{0x55,	0x75},
	{0x56,	0x76},
	{0x57,	0x77},
	{0x58,	0x78},
	{0x59,	0x79},
	{0x5a,	0x7a},
	{0x30,	0x30},	// 0-9
	{0x31,	0x31},
	{0x32,	0x32},
	{0x33,	0x33},
	{0x34,	0x34},
	{0x35,	0x35},
	{0x36,	0x36},
	{0x37,	0x37},
	{0x38,	0x38},
	{0x39,	0x39},
	{0x1030,0x00},	// function keys F1 - F12
	{0x1031,0x00},
	{0x1032,0x00},
	{0x1033,0x00},
	{0x1034,0x00},
	{0x1035,0x00},
	{0x1036,0x00},
	{0x1037,0x00},
	{0x1038,0x00},
	{0x1039,0x00},
	{0x103a,0x00},
	{0x103b,0x00},
	{0x1020,0x00},	// Shift Left
	{0x1020,0x00},	// Shift Right
	{0x1021,0x00}, // Control Left
	{0x1021,0x00},	// Control Right
	{0x1023,0x00},	// ALT Left
	{0x1023,0x00},	// ALT Right
	{0xffff,0x00}, // DIKS_ALTGR  not sure what QT Key is
	{0x1022,0x00},	// META Left
	{0x1022,0x00},	// META Right
	{0x1053,0x00}, // Super Left
	{0x1054,0x00},	// Super Right
	{0x1056,0x00},	// Hyper Left
	{0x1057,0x00},	// Hyper Right
	{0x1024,0x00},	// CAPS Lock
	{0x1025,0x00},	// NUM Locka
	{0x1026,0x00},	// Scroll Lock
	{0x1000,0x1b},	// Escape
	{0x1012,0x00},	// Left
	{0x1014,0x00},	// Right
	{0x1013,0x00},	// Up
	{0x1015,0x00},	// Down
	{0x1001,0x09},	// Tab
	{0x1004,0x0d},	// Enter
	{0x20,	0x20},	// 7 bit printable ASCII
	{0x1003,0x00},	// Backspace
	{0x1006,0x00},	// Insert
	{0x1007,0x7f},	// Delete
	{0x1010,0x00},	// Home
	{0x1011,0x00},	// End
	{0x1016,0x00},	// Page Up
	{0x1017,0x00},	// Page Down
	{0x1009,0x00},	// Print
	{0x1008,0x00},	// Pause
	{0x60,	0x27},	// Quote Left
	{0x2d,	0x2d},	// Minus
	{0x3d,	0x3d},	// Equals
	{0x5b,	0x5b},	// Bracket Left
	{0x5d,	0x5d},	// Bracket Right
	{0x5c,	0x5c},	// Back Slash
	{0x3b,	0x3b},	// Semicolon
	{0xffff,0x00},  // DIKS_QUOTE_RIGHT not sure what QT Key is...
	{0x2c,	0x2c},	// Comma
	{0x2e,	0x2e},	// Period
	{0x2f,	0x2f},	// Slash
	{0x3c,	0x3c},	// Less Than

	// keypad keys.
	// from what i can tell QT doiesnt have a seperate key code for them.

	{0xffff,0x00},
	{0xffff,0x00},
	{0xffff,0x00},
	{0xffff,0x00},
	{0xffff,0x00},
	{0xffff,0x00},
	{0xffff,0x00},
	{0xffff,0x00},
	{0xffff,0x00},
	{0xffff,0x00},
	{0xffff,0x00},
	{0xffff,0x00},
	{0xffff,0x00},
	{0xffff,0x00},
	{0xffff,0x00},
	{0xffff,0x00},
	{0xffff,0x00},
	{0xffff,0x00},
	{0xffff,0x00},
	{0xffff,0x00},
	{0xffff,0x00},
	{0xffff,0x00},
	{0xffff,0x00},
	{0xffff,0x00}
};



//**FIXME - are these values OK?
const int kNumBuffers = 31;
const int kNeedFreeFrames = 1;
const int kPrebufferFrames = 12;
const int kKeepPrebuffer = 2;

struct DirectfbParams
{
    uint32_t format;
    int scale;
    int result;
    unsigned int id;
    unsigned int width;
    unsigned int height;
    int setsize;
};

struct DirectfbData
{
    IDirectFB *dfb;
    IDirectFBSurface *surface;
    struct DirectfbParams params;
    IDirectFBInputDevice *keyboard;
    IDirectFBEventBuffer *kbbuf;
};

VideoOutputDirectfb::VideoOutputDirectfb(void)
                   :VideoOutput()
{
    XJ_started = 0;
    data = new DirectfbData();
    pauseFrame.buf = NULL;
}

VideoOutputDirectfb::~VideoOutputDirectfb()
{
	DFBResult ret;

	// clear the surface
    ret = data->surface->Clear(data->surface, 0x00, 0x00, 0x00, 0x00);
    if (ret)
    {
        DirectFBError("Couldn't clear the buffers", ret);
    }

    ret = data->surface->Flip(data->surface, NULL, DSFLIP_ONSYNC);
    if (ret)
    {
        DirectFBError("Couldn't flip the buffers", ret);
    }

    ret = data->surface->Clear(data->surface, 0x00, 0x00, 0x00, 0x00);
    if (ret)
    {
        DirectFBError("Couldn't clear the buffers", ret);
    }

	// cleanup
    if (data->kbbuf)
        data->kbbuf->Release(data->kbbuf);
    if (data->keyboard)
        data->keyboard->Release(data->keyboard);
    if (data->surface)
        data->surface->Release(data->surface);
    if (data->dfb)
        data->dfb->Release(data->dfb);
    if (pauseFrame.buf)
        delete [] pauseFrame.buf;

    Exit();
}

void VideoOutputDirectfb::Exit(void)
{
    if (XJ_started)
    {
        XJ_started = false;

        DeleteDirectfbBuffers();
    }
}

int VideoOutputDirectfb::GetRefreshRate(void)
{
    return 60;
}

bool VideoOutputDirectfb::Init(int width, int height, float aspect, WId winid,
              int winx, int winy, int winw, int winh, WId embedid)
{
    DFBResult ret;
    DFBSurfaceDescription desc;
    DFBDisplayLayerConfig dlc;
    IDirectFBDisplayLayer *layer;
    IDirectFBSurface *primary;

	widget = QWidget::find(winid);

    VideoOutput::InitBuffers(kNumBuffers, true, kNeedFreeFrames,
                             kPrebufferFrames, kKeepPrebuffer);
    VideoOutput::Init(width, height, aspect, winid, winx, winy, winw, winh,
                      embedid);

    ret = DirectFBInit(NULL,NULL);

    if (ret)
    {
        DirectFBError( "Couldn't initialize DirectFB", ret );
        return false;
    }

    DirectFBSetOption("bg-none",NULL);
    DirectFBSetOption("no-cursor",NULL);

    ret = DirectFBCreate( &(data->dfb) );

    if (ret)
    {
        DirectFBError( "Couldn't create DirectFB subsystem", ret );
        return false;
    }

    ret = data->dfb->GetDisplayLayer(data->dfb, DLID_PRIMARY, &layer);

    if (ret)
    {
        DirectFBError("No primary display layer - WTF?", ret);
        return false;
    }

    layer->SetOpacity(layer, 0);
    ret = layer->GetSurface(layer, &primary);

    if (ret)
    {
        DirectFBError("Couldn't get primary display surface", ret);
        return false;
    }

    data->params.result = 0;

    ret = data->dfb->EnumDisplayLayers(data->dfb, LayerCallback, data);

    if (ret)
    {
        DirectFBError("Couldn't enumerate display layers", ret);
        return false;
    }

    ret = data->dfb->GetDisplayLayer(data->dfb, 1, &layer);

    if (ret)
    {
        DirectFBError("Couldn't get our display layer - this shouldn't happen", ret);
        return false;
    }

    ret = layer->SetCooperativeLevel(layer, DLSCL_EXCLUSIVE);

    if (ret)
    {
        DirectFBError("Couldn't get exclusive access", ret);
        return false;
    }

    //**FIXME set up size - should be based on width of video
    dlc.flags = (DFBDisplayLayerConfigFlags)(DLCONF_WIDTH | DLCONF_HEIGHT);
    dlc.width = width;
    dlc.height = height;

    ret = layer->SetConfiguration(layer, &dlc);

    if (ret)
    {
        DirectFBError("Couldn't set display size", ret);
        return false;
    }

    //**FIXME set up pixelformat - should be based on video
    dlc.flags = DLCONF_PIXELFORMAT;
    dlc.pixelformat = DSPF_YV12;

    ret = layer->SetConfiguration(layer, &dlc);

    if (ret)
    {
        DirectFBError("Couldn't set requested pixelformat", ret);
        return false;
    }

    dlc.flags = DLCONF_BUFFERMODE;
    dlc.buffermode = DLBM_TRIPLE;
    ret = layer->SetConfiguration(layer, &dlc);

    if (ret)
    {
        DirectFBError("Couldn't set up triple buffering, trying double", ret);

        dlc.buffermode = DLBM_BACKVIDEO;
        ret = layer->SetConfiguration(layer, &dlc);

        if (ret)
        {
            dlc.buffermode = DLBM_BACKSYSTEM;
            ret = layer->SetConfiguration(layer, &dlc);

            if (ret)
            {
                DirectFBError("Couldn't set up double buffering, falling back to single buffer", ret);
            }
        }
    }


    ret = layer->GetSurface(layer, &(data->surface));

    if (ret)
    {
        DirectFBError( "Couldn't get our layer's surface - this is bad", ret);
        return false;
    }

    ret = data->surface->SetBlittingFlags(data->surface, DSBLIT_NOFX);

    if (ret)
    {
        DirectFBError("Couldn't set up Blitting flags - continuing anyway", ret);
    }

    //**FIXME set surface properties - should use video
    desc.flags = (DFBSurfaceDescriptionFlags)(DSDESC_HEIGHT | DSDESC_WIDTH | DSDESC_PIXELFORMAT);
    desc.width = width;
    desc.height = height;
    desc.pixelformat = DSPF_YV12;


	// clear the surface
    ret = data->surface->Clear(data->surface, 0x00, 0x00, 0x00, 0x00);
    if (ret)
    {
        DirectFBError("Couldn't clear the buffers", ret);
    }

    ret = data->surface->Flip(data->surface, NULL, DSFLIP_ONSYNC);
    if (ret)
    {
        DirectFBError("Couldn't flip the buffers", ret);
    }

    ret = data->surface->Clear(data->surface, 0x00, 0x00, 0x00, 0x00);
    if (ret)
    {
        DirectFBError("Couldn't clear the buffers", ret);
    }

    //show the surface
    ret = layer->SetOpacity(layer, 255);

    if (ret)
    {
        DirectFBError("Couldn't make our layer visible - nothing to see here, go home", ret);
    }

    ret = data->dfb->GetInputDevice(data->dfb, DIDID_KEYBOARD, &(data->keyboard));

    if (ret)
    {
        DirectFBError("Couldn't initialize keyboard", ret);
    }
    else
    {
        ret = data->keyboard->CreateEventBuffer(data->keyboard, &(data->kbbuf));

        if (ret)
        {
            DirectFBError("Couldn't initialize keyboard", ret);
        }
        else
        {
            data->kbbuf->Reset(data->kbbuf);
        }
    }

    if (!CreateDirectfbBuffers())
        return false;

    //this stuff is right from Xv - look at this sometime
    scratchFrame = &(vbuffers[kNumBuffers]);

    pauseFrame.height = scratchFrame->height;
    pauseFrame.width = scratchFrame->width;
    pauseFrame.bpp = scratchFrame->bpp;
    pauseFrame.size = scratchFrame->size;
    pauseFrame.buf = new unsigned char[pauseFrame.size];

    XJ_started = true;

    return true;
}

void VideoOutputDirectfb::PrepareFrame(VideoFrame *buffer)
{
    DFBResult ret;

    int width, height, ysize, uvsize;
    unsigned char *framebuf, *src, *dst;
    int pitch;

    if (!buffer)
        buffer=scratchFrame;

    width = buffer->width;
    height = buffer->height;
    ysize = width * height;
    uvsize = ysize / 4;

    ret = data->surface->Lock(data->surface, DSLF_WRITE, (void**)(&framebuf), &pitch);

    if (ret)
    {
        DirectFBError("Couldn't get write access to our surface", ret);
        return;
    }

    //memcpy(framebuf, buffer->buf, ysize * 3 / 2);
    //**FIXME copy the frame's planes onto the surface - should do different
    //behaviour depending on video?
    //y-plane
    memcpy(framebuf, buffer->buf, ysize);
    //u-plane
    memcpy(framebuf + width * height, buffer->buf + uvsize * 5, uvsize);
    //v-plane
    memcpy(framebuf + uvsize * 5, buffer->buf + ysize, uvsize);

    ret = data->surface->Unlock(data->surface);

    if (ret)
    {
      DirectFBError("Unlock() failed", ret);
    }
}

void VideoOutputDirectfb::Show(void)
{
    DFBResult ret;

    ret = data->surface->Flip(data->surface, NULL, DSFLIP_ONSYNC);

    if (ret)
    {
        DirectFBError("Couldn't flip the buffers", ret);
    }

    DFBInputEvent event;
    if (data->kbbuf->GetEvent( data->kbbuf, DFB_EVENT(&event) ) == DFB_OK)
    {
	    if (event.type == DIET_KEYPRESS) {
			QApplication::postEvent(widget, new QKeyEvent(QEvent::KeyPress, QT_KEYS[(event.key_id)-DFB_KBID_OFFSET][0], QT_KEYS[(event.key_id)-DFB_KBID_OFFSET][1], 0));
		}
		else if (event.type == DIET_KEYRELEASE)
		{
			QApplication::postEvent(widget, new QKeyEvent(QEvent::KeyRelease, QT_KEYS[(event.key_id)-DFB_KBID_OFFSET][0], QT_KEYS[(event.key_id)-DFB_KBID_OFFSET][1], 0));
		}
	}

}

void VideoOutputDirectfb::DrawUnusedRects(void)
{
    //**FIXME - should do something here
    cerr << "DrawUnusedRects()" << endl;
}

void VideoOutputDirectfb::UpdatePauseFrame(void)
{
    //**FIXME - is this all we need?
    VideoFrame *pauseb = scratchFrame;
    if (usedVideoBuffers.count() > 0)
        pauseb = usedVideoBuffers.head();

    memcpy(pauseFrame.buf, pauseb->buf, pauseb->size);
}

void VideoOutputDirectfb::ProcessFrame(VideoFrame *frame, OSD *osd,
                                       FilterChain *filterList,
                                       NuppelVideoPlayer *pipPlayer)
{
    if (!frame)
    {
        frame = scratchFrame;
        CopyFrame(scratchFrame, &pauseFrame);
    }

    if (filterList)
        filterList->ProcessFrame(frame);

    ShowPip(frame, pipPlayer);
    DisplayOSD(frame, osd);

}

void VideoOutputDirectfb::InputChanged(int width, int height, float aspect)
{
    //**FIXME - MoveResize()??
    VideoOutput::InputChanged(width, height, aspect);

    DeleteDirectfbBuffers();
    CreateDirectfbBuffers();

    scratchFrame = &(vbuffers[kNumBuffers]);

    if (pauseFrame.buf)
        delete [] pauseFrame.buf;

    pauseFrame.height = scratchFrame->height;
    pauseFrame.width = scratchFrame->width;
    pauseFrame.bpp = scratchFrame->bpp;
    pauseFrame.size = scratchFrame->size;
    pauseFrame.buf = new unsigned char[pauseFrame.size];
}

void VideoOutputDirectfb::AspectChanged(float aspect)
{
    //**FIXME - -do something here
    cerr << "AspectChanged()" << endl;
}

void VideoOutputDirectfb::Zoom(int direction)
{
    //**FIXME - -do something here
    cerr << "Zoom()" << endl;
}

bool VideoOutputDirectfb::CreateDirectfbBuffers(void)
{
    for (int i = 0; i < numbuffers + 1; i++)
    {
        vbuffers[i].height = XJ_height;
        vbuffers[i].width = XJ_width;
        vbuffers[i].bpp = 12;
        vbuffers[i].size = XJ_height * XJ_width * 3 / 2;
        vbuffers[i].codec = FMT_YV12;
        vbuffers[i].buf = new unsigned char[vbuffers[i].size];
    }

    return true;
}

void VideoOutputDirectfb::DeleteDirectfbBuffers(void)
{
    for (int i = 0 ; i < numbuffers +1; i++)
    {
        delete [] vbuffers[i].buf;
        vbuffers[i].buf = NULL;
    }
}

//**FIXME - is there a way to make this part of the class?
DFBEnumerationResult LayerCallback(unsigned int id,
  DFBDisplayLayerDescription desc, void *data)
{
    struct DirectfbData *vodata = (DirectfbData*)data;
    struct DirectfbParams *params = &(vodata->params);
    IDirectFBDisplayLayer *layer;
    DFBResult ret;

    ret = vodata->dfb->GetDisplayLayer(vodata->dfb, id, &layer);

    if (ret)
    {
        DirectFBError("Couldn't open layer", ret);
        return DFENUM_OK;
    }

    DFBDisplayLayerConfig dlc;

    if (params->setsize)
    {
        dlc.flags = (DFBDisplayLayerConfigFlags)(DLCONF_WIDTH | DLCONF_HEIGHT);
        dlc.width = params->width;
        dlc.height = params->height;

        ret = layer->SetConfiguration(layer, &dlc);

        if (ret)
        {
            DirectFBError("Couldn't set layer size", ret);
        }
    }

    dlc.flags = DLCONF_PIXELFORMAT;
    dlc.pixelformat = DSPF_YV12;

    layer->SetOpacity(layer, 0);

    ret = layer->TestConfiguration(layer, &dlc, NULL);

    layer->Release(layer);

    if (DFB_OK == ret)
    {
        if (params->result)
        {
            if (!params->scale && 0)
            {
                params->scale = 1;
                params->id = id;
            }
            else
            {
                params->result = 1;
	        params->id = id;
            }
        }

    }

    return DFENUM_OK;
}
