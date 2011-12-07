#ifndef _FINDINGDEFS_H
#define _FINDINGDEFS_H

#undef FINDINGS_PREAMBLE
#undef FINDINGS_POSTAMBLE
#undef FINDINGS_MAP

// names are expected to start with kFinding
// value is the enumerated value (0-xxxxx)
// desc is descriptive text used for dumping results

#ifdef _IMPLEMENT_FINDINGS

// This actually will implement the mapping in resultslist.cpp
#define FINDINGS_PREAMBLE
#define FINDINGS_POSTAMBLE
#define FINDINGS_MAP(name,value,desc) \
    findingsAdd((int)value,QString(desc));

#else

// This is used to define the enumerated type (used by all files)

#define FINDINGS_PREAMBLE \
    typedef enum {
#define FINDINGS_POSTAMBLE \
        kFindingsLastItem \
    } FlagFindingsType;
#define FINDINGS_MAP(name,value,desc) \
        name = (int)(value),

#endif

FINDINGS_PREAMBLE
FINDINGS_MAP(kFindingAudioHigh,         0, "Audio more than 6dB above average")
FINDINGS_MAP(kFindingAudioLow,          1, "Audio more than 12dB below average")
FINDINGS_MAP(kFindingVideoBlankFrame,   2, "Blank video frame")
FINDINGS_MAP(kFindingVideoSceneChange,  3, "Video scene change")
FINDINGS_MAP(kFindingVideoAspectChange, 4, "Video aspect ratio change")
FINDINGS_MAP(kFindingVideoLogoChange,   5, "Video logo presence change")
FINDINGS_POSTAMBLE

#endif

/*
 * vim:ts=4:sw=4:ai:et:si:sts=4
 */

