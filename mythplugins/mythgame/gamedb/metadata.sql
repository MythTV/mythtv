USE mythconverg;
CREATE TABLE gamemetadata
(
    system VARCHAR(128) NOT NULL,
    romname VARCHAR(128) NOT NULL,
    gamename VARCHAR(128) NOT NULL,
    genre VARCHAR(128) NOT NULL,
    year INT UNSIGNED NOT NULL,
    INDEX (system),
    INDEX (year),
    INDEX (romname),
    INDEX (gamename),
    INDEX (genre)
);

CREATE TABLE mamemetadata
(
    romname VARCHAR(128) NOT NULL,
    manu VARCHAR(128) NOT NULL,
    cloneof VARCHAR(128) NOT NULL,
    romof VARCHAR(128) NOT NULL,
    driver VARCHAR(128) NOT NULL,
    cpu1 VARCHAR(128) NOT NULL,
    cpu2 VARCHAR(128) NOT NULL,
    cpu3 VARCHAR(128) NOT NULL,
    cpu4 VARCHAR(128) NOT NULL,
    sound1 VARCHAR(128) NOT NULL,
    sound2 VARCHAR(128) NOT NULL,
    sound3 VARCHAR(128) NOT NULL,
    sound4 VARCHAR(128) NOT NULL,
    players INT UNSIGNED NOT NULL,
    buttons INT UNSIGNED NOT NULL,
    INDEX (romname)
);

CREATE TABLE mamesettings
(
    romname VARCHAR(128) NOT NULL,
    usedefault BOOL NOT NULL,
    fullscreen BOOL NOT NULL,
    scanlines BOOL NOT NULL,
    extra_artwork BOOL NOT NULL,
    autoframeskip BOOL NOT NULL,
    autocolordepth BOOL NOT NULL,
    rotleft BOOL NOT NULL,
    rotright BOOL NOT NULL,
    flipx BOOL NOT NULL,
    flipy BOOL NOT NULL,
    scale TINYINT UNSIGNED NOT NULL,
    antialias BOOL NOT NULL,
    translucency BOOL NOT NULL,
    beam FLOAT NOT NULL,
    flicker FLOAT NOT NULL,
    vectorres INT NOT NULL,
    analogjoy BOOL NOT NULL,
    mouse BOOL NOT NULL,
    winkeys BOOL NOT NULL,
    grabmouse BOOL NOT NULL,
    joytype TINYINT UNSIGNED NOT NULL,
    sound BOOL NOT NULL,
    samples BOOL NOT NULL,
    fakesound BOOL NOT NULL,
    volume TINYINT NOT NULL,
    cheat BOOL NOT NULL,
    extraoption VARCHAR(128) NOT NULL
);

INSERT INTO mamesettings (romname,usedefault,fullscreen,scanlines,extra_artwork,
autoframeskip,autocolordepth,rotleft,rotright,
flipx,flipy,scale,antialias,translucency,beam,
flicker,vectorres,analogjoy,mouse,winkeys,
grabmouse,joytype,sound,samples,fakesound,
volume,cheat,extraoption) VALUES 
("default",1,1,0,0,0,0,0,0,0,0,1,0,0,1,0.0,0.0,0,0,0,0,4,
1,1,0,-16,0,"");