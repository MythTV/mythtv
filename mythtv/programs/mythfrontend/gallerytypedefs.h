#ifndef TYPEDEFS_H
#define TYPEDEFS_H



enum ImageLoadStatusType {
    kStatusOk           = 0,
    kStatusNoFiles      = 1,
    kStatusNoBaseDir    = 2
};


enum ImageNodeState {
    kNodeStateSelect    = 0,
    kNodeStateDeselect  = 1,
    kNodeStateInvert    = 2,
    kNodeStateVisible   = 3,
    kNodeStateInvisible = 4
};


enum ImageSlideShowType {
    kNoSlideShow            = 0,
    kNormalSlideShow        = 1,
    kRandomSlideShow        = 2,
    kNormalRecSlideShow     = 3,
    kRandomRecSlideShow     = 4
};


enum ImageTransitionType {
    kNoTransition = 0,
    kFade = 1
};


#endif // TYPEDEFS_H
