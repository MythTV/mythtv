USE mythconverg;

CREATE TABLE dvdinput
(
    intid       INT UNSIGNED NOT NULL PRIMARY KEY,
    hsize       INT UNSIGNED,
    vsize       INT UNSIGNED,
    ar_num      INT UNSIGNED,
    ar_denom    INT UNSIGNED,
    fr_code     INT UNSIGNED,
    letterbox   BOOL,
    v_format    VARCHAR(16)
);

#
#   This doesn't work yet
#
#CREATE TABLE dvdtranscode
#(
#    intid       INT NOT NULL PRIMARY KEY,
#    input       INT UNSIGNED,
#    name        VARCHAR(128) NOT NULL,
#    codec       VARCHAR(128) NOT NULL,
#    twopass     BOOL NOT NULL DEFAULT 0,
#    ac3         BOOL NOT NULL DEFAULT 0,
#    fvresize    INT UNSIGNED,
#    fhresize    INT UNSIGNED,
#    svresize    INT UNSIGNED,
#    shresize    INT UNSIGNED,
#    syncmode    INT UNSIGNED,
#    use_yv12    BOOL
#);        
#


#
#   ntsc 16:9 letterbox
#
INSERT INTO dvdinput
    (intid, hsize, vsize, ar_num, ar_denom, fr_code, letterbox, v_format)
    VALUES
    (1, 720, 480, 16, 9, 1, 1, "ntsc"); 
  
#
#   ntsc 16:9
#
INSERT INTO dvdinput
    (intid, hsize, vsize, ar_num, ar_denom, fr_code, letterbox, v_format)
    VALUES
    (2, 720, 480, 16, 9, 1, 0, "ntsc"); 

#
#   ntsc 4:3 letterbox
#

INSERT INTO dvdinput
    (intid, hsize, vsize, ar_num, ar_denom, fr_code, letterbox, v_format)
    VALUES
    (3, 720, 480, 4, 3, 1, 1, "ntsc"); 

#
#  ntsc 4:3 
#  

INSERT INTO dvdinput
    (intid, hsize, vsize, ar_num, ar_denom, fr_code, letterbox, v_format)
    VALUES
    (4, 720, 480, 4, 3, 1, 0, "ntsc"); 

#
#   pal 16:9 letterbox
#

INSERT INTO dvdinput
    (intid, hsize, vsize, ar_num, ar_denom, fr_code, letterbox, v_format)
    VALUES
    (5, 720, 576, 16, 9, 3, 1, "pal"); 

#
#   pal 16:9
#

INSERT INTO dvdinput
    (intid, hsize, vsize, ar_num, ar_denom, fr_code, letterbox, v_format)
    VALUES
    (6, 720, 576, 16, 9, 3, 0, "pal"); 

#
#   pal 4:3 letterbox
#

INSERT INTO dvdinput
    (intid, hsize, vsize, ar_num, ar_denom, fr_code, letterbox, v_format)
    VALUES
    (7, 720, 576, 4, 3, 3, 1, "pal"); 

#
#   pal 4:3 
#

INSERT INTO dvdinput
    (intid, hsize, vsize, ar_num, ar_denom, fr_code, letterbox, v_format)
    VALUES
    (8, 720, 576, 4, 3, 3, 0, "pal"); 


