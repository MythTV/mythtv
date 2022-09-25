LIBAVCODEC_MAJOR {
    global:
        av_*;
        avcodec_*;
        avpriv_*;
        avsubtitle_free;
        ff_ue_golomb_vlc_code;
        ff_golomb_vlc_len;
        ff_se_golomb_vlc_code;
    local:
        *;
};
