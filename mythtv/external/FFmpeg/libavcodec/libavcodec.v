LIBAVCODEC_MAJOR {
    global:
        av*;
        #deprecated, remove after next bump
        audio_resample;
        audio_resample_close;
        ff_ue_golomb_vlc_code;
        ff_golomb_vlc_len;
        ff_se_golomb_vlc_code;
        ff_codec_type_string;
        ff_codec_id_string;
        ff_zigzag_direct;
        avpriv_find_start_code;
        ff_fft_init;
        ff_fft_end;
    local: 
        *;
};
