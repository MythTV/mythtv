LIBAVFORMAT_MAJOR {
    global:
        av*;
        ffurl_read;
        ffurl_read_complete;
        ffurl_seek;
        ffurl_size;
        ffurl_protocol_next;
        ffurl_open;
        ffurl_open_whitelist;
        ffurl_close;
        ffurl_write;
    local:
        *;
};
