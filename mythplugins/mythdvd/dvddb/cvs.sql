USE mythconverg;

#
#  This table classifies
#  data on a DVD
#

CREATE TABLE dvdtranscode
(
    intid       INT AUTO_INCREMENT NOT NULL PRIMARY KEY,
    input       INT UNSIGNED,
    name        VARCHAR(128) NOT NULL,
    sync_mode   INT UNSIGNED,
    use_yv12    BOOL,
    cliptop     INT,
    clipbottom  INT,
    clipleft    INT,
    clipright   INT,
    f_resize_h  INT,
    f_resize_w  INT,
    hq_resize_h INT,
    hq_resize_w INT,
    grow_h      INT,
    grow_w      INT,
    clip2top    INT,
    clip2bottom INT,
    clip2left   INT,
    clip2right  INT,
    codec	VARCHAR(128) NOT NULL,
    codec_param VARCHAR(128),
    bitrate     INT,
    a_sample_r  INT,
    a_bitrate   INT,
    two_pass    BOOL
);        

#
#   ntsc 16:9 letterbox --> Good
#
INSERT INTO dvdtranscode
    (input, name, sync_mode, use_yv12, 
     cliptop, clipbottom, clipleft, clipright,
     f_resize_h, f_resize_w, 
     hq_resize_h, hq_resize_w,
     grow_h, grow_w,
     clip2top, clip2bottom, clip2left, clip2right,
     codec, bitrate, two_pass)
    VALUES
    (1, "Good", 2, 0,
     16, 16, 0, 0,
     2, 0, 
     0, 0,
     0, 0,
     32, 32, 8, 8,
     "divx5", 1618, 0);
     


#
#   ntsc 16:9 --> Excellent
#
INSERT INTO dvdtranscode
    (input, name, sync_mode, use_yv12, 
     cliptop, clipbottom, clipleft, clipright,
     f_resize_h, f_resize_w, 
     hq_resize_h, hq_resize_w,
     grow_h, grow_w,
     clip2top, clip2bottom, clip2left, clip2right,
     codec, bitrate, two_pass)
    VALUES
    (2, "Excellent", 2, 0,
     0, 0, 0, 0,
     0, 0, 
     0, 0,
     0, 0,
     0, 0, 0, 0,
     "divx5", 0, 1);
     
#
#   ntsc 16:9 --> Good
#
INSERT INTO dvdtranscode
    (input, name, sync_mode, use_yv12, 
     cliptop, clipbottom, clipleft, clipright,
     f_resize_h, f_resize_w, 
     hq_resize_h, hq_resize_w,
     grow_h, grow_w,
     clip2top, clip2bottom, clip2left, clip2right,
     codec, bitrate, two_pass)
    VALUES
    (2, "Good", 2, 1,
     0, 0, 8, 8,
     0, 0, 
     0, 0,
     0, 0,
     0, 0, 0, 0,
     "divx5", 1618, 0);
     
#
#   ntsc 16:9 --> Medium
#
INSERT INTO dvdtranscode
    (input, name, sync_mode, use_yv12, 
     cliptop, clipbottom, clipleft, clipright,
     f_resize_h, f_resize_w, 
     hq_resize_h, hq_resize_w,
     grow_h, grow_w,
     clip2top, clip2bottom, clip2left, clip2right,
     codec, bitrate, two_pass)
    VALUES
    (2, "Medium", 2, 1,
     0, 0, 8, 8,
     5, 5, 
     0, 0,
     0, 0,
     0, 0, 0, 0,
     "divx5", 1200, 0);
     
#
#   ntsc 4:3 letterbox --> Good
#
INSERT INTO dvdtranscode
    (input, name, sync_mode, use_yv12, 
     cliptop, clipbottom, clipleft, clipright,
     f_resize_h, f_resize_w, 
     hq_resize_h, hq_resize_w,
     grow_h, grow_w,
     clip2top, clip2bottom, clip2left, clip2right,
     codec, bitrate, two_pass)
    VALUES
    (3, "Good", 2, 1,
     0, 0, 0, 0,
     0, 0, 
     0, 0,
     2, 0,
     80, 80, 8, 8,
     "divx5", 0, 0);

#
#   ntsc 4:3 --> Excellent
#
INSERT INTO dvdtranscode
    (input, name, sync_mode, use_yv12, 
     cliptop, clipbottom, clipleft, clipright,
     f_resize_h, f_resize_w, 
     hq_resize_h, hq_resize_w,
     grow_h, grow_w,
     clip2top, clip2bottom, clip2left, clip2right,
     codec, bitrate, two_pass)
    VALUES
    (4, "Excellent", 2, 1,
     0, 0, 0, 0,
     0, 0, 
     0, 0,
     2, 0,
     0, 0, 0, 0,
     "divx5", 0, 1);

#
#   ntsc 4:3 --> Good
#
INSERT INTO dvdtranscode
    (input, name, sync_mode, use_yv12, 
     cliptop, clipbottom, clipleft, clipright,
     f_resize_h, f_resize_w, 
     hq_resize_h, hq_resize_w,
     grow_h, grow_w,
     clip2top, clip2bottom, clip2left, clip2right,
     codec, bitrate, two_pass)
    VALUES
    (4, "Good", 2, 1,
     0, 0, 8, 8,
     0, 2, 
     0, 0,
     0, 0,
     0, 0, 0, 0,
     "divx5", 1618, 0);

#
#   pal 16:9 letterbox --> Good
#
INSERT INTO dvdtranscode
    (input, name, sync_mode, use_yv12, 
     cliptop, clipbottom, clipleft, clipright,
     f_resize_h, f_resize_w, 
     hq_resize_h, hq_resize_w,
     grow_h, grow_w,
     clip2top, clip2bottom, clip2left, clip2right,
     codec, bitrate, two_pass)
    VALUES
    (5, "Good", 1, 1,
     16, 16, 0, 0,
     5, 0, 
     0, 0,
     0, 0,
     40, 40, 8, 8,
     "divx5", 1618, 0);
     

#
#   pal 16:9 --> Good
#
INSERT INTO dvdtranscode
    (input, name, sync_mode, use_yv12, 
     cliptop, clipbottom, clipleft, clipright,
     f_resize_h, f_resize_w, 
     hq_resize_h, hq_resize_w,
     grow_h, grow_w,
     clip2top, clip2bottom, clip2left, clip2right,
     codec, bitrate, two_pass)
    VALUES
    (6, "Good", 1, 1,
     0, 0, 16, 16,
     5, 0, 
     0, 0,
     0, 0,
     0, 0, 0, 0,
     "divx5", 1618, 0);
     


     

#
#   pal 4:3 letterbox --> Good
#
INSERT INTO dvdtranscode
    (input, name, sync_mode, use_yv12, 
     cliptop, clipbottom, clipleft, clipright,
     f_resize_h, f_resize_w, 
     hq_resize_h, hq_resize_w,
     grow_h, grow_w,
     clip2top, clip2bottom, clip2left, clip2right,
     codec, bitrate, two_pass)
    VALUES
    (7, "Good", 1, 1,
     0, 0, 0, 0,
     1, 0, 
     0, 0,
     0, 0,
     76, 76, 8, 8,
     "divx5", 1618, 0);
     
#
#   pal 4:3 --> Good
#
INSERT INTO dvdtranscode
    (input, name, sync_mode, use_yv12, 
     cliptop, clipbottom, clipleft, clipright,
     f_resize_h, f_resize_w, 
     hq_resize_h, hq_resize_w,
     grow_h, grow_w,
     clip2top, clip2bottom, clip2left, clip2right,
     codec, bitrate, two_pass)
    VALUES
    (8, "Good", 1, 1,
     0, 0, 0, 0,
     1, 0, 
     0, 0,
     0, 0,
     0, 0, 0, 0,
     "divx5", 1618, 0);
     

