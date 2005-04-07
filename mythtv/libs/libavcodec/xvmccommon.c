
static XvMCSurface* findPastSurface(MpegEncContext *s, 
                                    xvmc_render_state_t *render) 
{
    Picture *lastp = s->last_picture_ptr;
    xvmc_render_state_t *last = NULL;

    if (NULL!=lastp) {
        last = (xvmc_render_state_t*)(lastp->data[2]);
        if (B_TYPE==last->pict_type)
        {
            last = NULL;
            av_log(s->avctx, AV_LOG_ERROR, "Past frame is a B frame in findPastSurface\n");
        }
        //assert(B_TYPE!=last->pict_type);
    }

    if (NULL==last)
        if (!s->first_field)
            last = render; // predict second field from the first
        else
            return 0;

    if (last->magic != MP_XVMC_RENDER_MAGIC)
        return 0;

    return (last->state & MP_XVMC_STATE_PREDICTION) ? last->p_surface : 0;
}

static XvMCSurface* findFutureSurface(MpegEncContext *s) 
{
    Picture *nextp = s->next_picture_ptr;
    xvmc_render_state_t *next = NULL;

    if (NULL!=nextp) {
        next = (xvmc_render_state_t*)(nextp->data[2]);
        if (B_TYPE==next->pict_type)
        {
            next = NULL;
            av_log(s->avctx, AV_LOG_ERROR, "Next frame is a B frame in findFutureSurface\n");
        }
        //assert(B_TYPE!=next->pict_type);
    }

    if (NULL==next)
        return 0;

    //assert(NULL!=next);

    if (next->magic != MP_XVMC_RENDER_MAGIC)
        return 0;

    return (next->state & MP_XVMC_STATE_PREDICTION) ? next->p_surface : 0;
}

