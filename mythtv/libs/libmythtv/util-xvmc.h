//#define DEBUG_PAUSE /* enable to debug XvMC pause frame */

#ifdef USING_XVMC

#   if defined(USING_XVMCW) || defined(USING_XVMC_VLD)
        extern "C" Status XvMCPutSlice2(Display*,XvMCContext*,char*,int,int);
#   else
        Status XvMCPutSlice2(Display*, XvMCContext*, char*, int, int)
            { return XvMCBadSurface; }
#   endif

static QString ErrorStringXvMC(int val)
{
    QString str = "unrecognized return value";
    switch (val)
    {
        case Success:   str = "Success"  ; break;
        case BadValue:  str = "BadValue" ; break;
        case BadMatch:  str = "BadMatch" ; break;
        case BadAlloc:  str = "BadAlloc" ; break;
    }
    return str;
}

static xvmc_render_state_t *GetRender(VideoFrame *frame)
{
    if (frame)
        return (xvmc_render_state_t*) frame->buf;
    return NULL;
}

static uint calcBPM(int chroma)
{
    int ret;
    switch (chroma)
    {
        case XVMC_CHROMA_FORMAT_420: ret = 6;
        case XVMC_CHROMA_FORMAT_422: ret = 4+2;
        case XVMC_CHROMA_FORMAT_444: ret = 4+4;
        default: ret = 6;
        // default unless gray, then 4 is the right number,
        // a bigger number just wastes a little memory.
    }
    return ret;
}
#endif // USING_XVMC


class XvMCBufferSettings
{
  public:
    XvMCBufferSettings() :
        num_xvmc_surf(1),
        needed_for_display(1),
        min_num_xvmc_surfaces(8),
        max_num_xvmc_surfaces(16),
        num_xvmc_surfaces(min_num_xvmc_surfaces),
        aggressive(false) {}

    void SetOSDNum(uint val)
    {
        num_xvmc_surf = val;
    }

    void SetNumSurf(uint val)
    {
        num_xvmc_surfaces = min(max(val, min_num_xvmc_surfaces),
                                max_num_xvmc_surfaces);
    }

    /// Returns number of XvMC OSD surfaces to allocate
    uint GetOSDNum(void)    const { return num_xvmc_surf; }

    /// Returns number of frames we want decoded before we
    /// try to display a frame.
    uint GetNeededBeforeDisplay(void)
        const { return needed_for_display; }

    /// Returns minumum number of XvMC surfaces we need
    uint GetMinSurf(void) const { return min_num_xvmc_surfaces; }

    /// Returns maximum number of XvMC surfaces should try to get
    uint GetMaxSurf(void) const { return max_num_xvmc_surfaces; }

    /// Returns number of XvMC surfaces we actually allocate
    uint GetNumSurf(void) const { return num_xvmc_surfaces; }

    /// Returns number of frames we want to try to prebuffer
    uint GetPreBufferGoal(void) const
    {
        uint reserved = GetFrameReserve() + XVMC_PRE_NUM +
            XVMC_POST_NUM + XVMC_SHOW_NUM;
        return num_xvmc_surfaces - reserved;
    }

    /// Returns number of frames reserved for the OSD blending process
    /// and for video display. This is the HARD reserve.
    uint GetFrameReserve(void) const
        { return num_xvmc_surf + XVMC_SHOW_NUM; }

    /// Returns true if we should be aggressive in freeing buffers
    bool IsAggressive(void)  const { return aggressive; }

  private:
    /// Number of XvMC OSD surface to allocate
    uint num_xvmc_surf;
    /// Frames needed before we try to display a frame, a larger
    /// number here ensures that we don't lose A/V Sync when a 
    /// frame takes longer than one frame interval to decode.
    uint needed_for_display;
    /// Minumum number of XvMC surfaces to get
    uint min_num_xvmc_surfaces;
    /// Maximum number of XvMC surfaces to get
    uint max_num_xvmc_surfaces;
    /// Number of XvMC surfaces we got
    uint num_xvmc_surfaces;
    /// Use aggressive buffer management
    bool aggressive;

    /// Allow for one I/P frame before us
    static const uint XVMC_PRE_NUM  = 1;
    /// Allow for one I/P frame after us
    static const uint XVMC_POST_NUM = 1;
    /// Allow for one B frame to be displayed
    static const uint XVMC_SHOW_NUM = 1;
};

