USE mythconverg;

CREATE TABLE snessettings
(
    romname VARCHAR(128) NOT NULL,
    usedefault BOOL NOT NULL,
    transparency BOOL NOT NULL,
    sixteen BOOL NOT NULL,
    hires BOOL NOT NULL,
    interpolate TINYINT UNSIGNED NOT NULL,
    nomodeswitch BOOL NOT NULL,
    fullscreen BOOL NOT NULL,
    stretch BOOL NOT NULL,
    nosound BOOL NOT NULL,
    soundskip TINYINT NOT NULL,
    stereo BOOL NOT NULL,
    soundquality TINYINT UNSIGNED NOT NULL,
    envx BOOL NOT NULL,
    threadsound BOOL NOT NULL,
    syncsound BOOL NOT NULL,
    interpolatedsound BOOL NOT NULL,
    buffersize INT UNSIGNED NOT NULL,
    nosamplecaching BOOL NOT NULL,
    altsampledecode BOOL NOT NULL,
    noecho BOOL NOT NULL,
    nomastervolume BOOL NOT NULL,
    nojoy BOOL NOT NULL,
    interleaved BOOL NOT NULL,
    altinterleaved BOOL NOT NULL,
    hirom BOOL NOT NULL,
    lowrom BOOL NOT NULL,
    header BOOL NOT NULL,
    noheader BOOL NOT NULL,
    pal BOOL NOT NULL,
    ntsc BOOL NOT NULL,
    layering BOOL NOT NULL,
    nohdma BOOL NOT NULL,
    nospeedhacks BOOL NOT NULL,
    nowindows BOOL NOT NULL,
    extraoption VARCHAR(128) NOT NULL
);

INSERT INTO snessettings (romname,usedefault,transparency,sixteen,hires,
interpolate,nomodeswitch,fullscreen,stretch,
nosound,soundskip,stereo,soundquality,envx,threadsound,
syncsound,interpolatedsound,buffersize,nosamplecaching,altsampledecode,
noecho,nomastervolume,nojoy,interleaved,altinterleaved,hirom,lowrom,header,
noheader,pal,ntsc,layering,nohdma,nospeedhacks,nowindows,extraoption) VALUES 
("default",1,0,0,0,0,0,0,0,0,0,1,4,0,0,0,0,0,0,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,"");