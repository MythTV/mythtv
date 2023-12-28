LIBAVFORMAT_MAJOR {
    global:
        av*;
        ffurl_read;
        ffurl_seek;
        ffurl_size;
        ffurl_open_whitelist;
        ffurl_close;
        ffurl_write;
    local:
        *;
};
