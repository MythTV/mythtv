
static XvMCSurface* findPastSurface(MpegEncContext *s, 
                                    struct xvmc_pix_fmt *render)
{
    Picture *lastp = s->last_picture_ptr;
    struct xvmc_pix_fmt *last = NULL;

    if (NULL!=lastp) {
        last = (struct xvmc_pix_fmt*)(lastp->data[2]);
        if (FF_B_TYPE==last->pict_type)
        {
            last = NULL;
            av_log(s->avctx, AV_LOG_ERROR, "Past frame is a B frame in findPastSurface\n");
        }
        //assert(FF_B_TYPE!=last->pict_type);
    }

    if (NULL==last)
        if (!s->first_field)
            last = render; // predict second field from the first
        else
            return 0;

    if (last->xvmc_id != AV_XVMC_ID)
        return 0;

    return (last->state & AV_XVMC_STATE_PREDICTION) ? last->p_surface : 0;
}

static XvMCSurface* findFutureSurface(MpegEncContext *s) 
{
    Picture *nextp = s->next_picture_ptr;
    struct xvmc_pix_fmt *next = NULL;

    if (NULL!=nextp) {
        next = (struct xvmc_pix_fmt*)(nextp->data[2]);
        if (FF_B_TYPE==next->pict_type)
        {
            next = NULL;
            av_log(s->avctx, AV_LOG_ERROR, "Next frame is a B frame in findFutureSurface\n");
        }
        //assert(FF_B_TYPE!=next->pict_type);
    }

    if (NULL==next)
        return 0;

    //assert(NULL!=next);

    if (next->xvmc_id != AV_XVMC_ID)
        return 0;

    return (next->state & AV_XVMC_STATE_PREDICTION) ? next->p_surface : 0;
}

