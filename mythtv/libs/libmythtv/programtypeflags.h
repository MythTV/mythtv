//
// Define flags/names for various ProgramInfo values.
//
// This file should be included once from programtypes.h with
// DEFINE_FLAGS_ENUM set to create the enums, and included again from
// programinfo.cpp with DEFINE_FLAGS_NAMES set to create the enum
// value to string mappings.
//
#undef FLAGS_PREAMBLE
#undef FLAGS_POSTAMBLE
#undef FLAGS_DATA

//NOLINTBEGIN(cppcoreguidelines-macro-usage)
#ifdef DEFINE_FLAGS_ENUM
#define FLAGS_PREAMBLE(NAME, TYPE) \
    enum NAME : TYPE {
#define FLAGS_POSTAMBLE(NAME, TYPE) \
    }; \
    using NAME##Type = TYPE;
#define FLAGS_DATA(PREFIX, NAME, VALUE) \
    PREFIX##_##NAME = (VALUE),
#endif

#ifdef DEFINE_FLAGS_NAMES
#define FLAGS_PREAMBLE(NAME, TYPE) \
    static const QMap<TYPE,QString> NAME##Names {
#define FLAGS_POSTAMBLE(NAME, TYPE) \
    };
#define FLAGS_DATA(PREFIX, NAME, VALUE) \
    { VALUE, #NAME },
#endif
//NOLINTEND(cppcoreguidelines-macro-usage)


/// If you change these please update:
/// mythplugins/mythweb/modules/tv/classes/Program.php
/// mythtv/bindings/perl/MythTV/Program.pm
/// (search for "Assign the program flags" in both)
FLAGS_PREAMBLE(ProgramFlag, uint32_t)
FLAGS_DATA(FL, NONE,             0x00000000)
FLAGS_DATA(FL, COMMFLAG,         0x00000001)
FLAGS_DATA(FL, CUTLIST,          0x00000002)
FLAGS_DATA(FL, AUTOEXP,          0x00000004)
FLAGS_DATA(FL, EDITING,          0x00000008)
FLAGS_DATA(FL, BOOKMARK,         0x00000010)
FLAGS_DATA(FL, REALLYEDITING,    0x00000020)
FLAGS_DATA(FL, COMMPROCESSING,   0x00000040)
FLAGS_DATA(FL, DELETEPENDING,    0x00000080)
FLAGS_DATA(FL, TRANSCODED,       0x00000100)
FLAGS_DATA(FL, WATCHED,          0x00000200)
FLAGS_DATA(FL, PRESERVED,        0x00000400)
FLAGS_DATA(FL, CHANCOMMFREE,     0x00000800)
FLAGS_DATA(FL, REPEAT,           0x00001000)
FLAGS_DATA(FL, DUPLICATE,        0x00002000)
FLAGS_DATA(FL, REACTIVATE,       0x00004000)
FLAGS_DATA(FL, IGNOREBOOKMARK,   0x00008000)
FLAGS_DATA(FL, IGNOREPROGSTART,  0x00010000)
FLAGS_DATA(FL, IGNORELASTPLAYPOS,0x00020000)
FLAGS_DATA(FL, LASTPLAYPOS,      0x00040000)
// if you move the type mask please edit {Set,Get}ProgramInfoType()
FLAGS_DATA(FL, TYPEMASK,         0x00F00000)
FLAGS_DATA(FL, INUSERECORDING,   0x01000000)
FLAGS_DATA(FL, INUSEPLAYING,     0x02000000)
FLAGS_DATA(FL, INUSEOTHER,       0x04000000)
FLAGS_POSTAMBLE(ProgramFlag, uint32_t)

/// if AudioProps changes, the audioprop column in program and
/// recordedprogram has to changed accordingly
 // For backwards compatibility do not change 0 or 1
FLAGS_PREAMBLE(AudioProps, uint8_t)
FLAGS_DATA(AUD, UNKNOWN,      0x00)
FLAGS_DATA(AUD, STEREO,       0x01)
FLAGS_DATA(AUD, MONO,         0x02)
FLAGS_DATA(AUD, SURROUND,     0x04)
FLAGS_DATA(AUD, DOLBY,        0x08)
FLAGS_DATA(AUD, HARDHEAR,     0x10)
FLAGS_DATA(AUD, VISUALIMPAIR, 0x20)
FLAGS_POSTAMBLE(AudioProps, uint8_t)

/// if VideoProps changes, the videoprop column in program and
/// recordedprogram has to changed accordingly
// For backwards compatibility do not change 0 or 1
FLAGS_PREAMBLE(VideoProps, uint16_t)
FLAGS_DATA(VID, UNKNOWN,     0x000)
FLAGS_DATA(VID, WIDESCREEN,  0x001)
FLAGS_DATA(VID, HDTV,        0x002)
FLAGS_DATA(VID, MPEG2,       0x004)
FLAGS_DATA(VID, AVC,         0x008)
FLAGS_DATA(VID, HEVC,        0x010)
FLAGS_DATA(VID, 720,         0x020)
FLAGS_DATA(VID, 1080,        0x040)
FLAGS_DATA(VID, 4K,          0x080)
FLAGS_DATA(VID, 3DTV,        0x100)
FLAGS_DATA(VID, PROGRESSIVE, 0x200)
FLAGS_DATA(VID, DAMAGED,     0x400)
FLAGS_POSTAMBLE(VideoProps, uint16_t)

/// if SubtitleTypes changes, the subtitletypes column in program and
/// recordedprogram has to changed accordingly
// For backwards compatibility do not change 0 or 1
FLAGS_PREAMBLE(SubtitleProps, uint8_t)
FLAGS_DATA(SUB, UNKNOWN,  0x00)
FLAGS_DATA(SUB, HARDHEAR, 0x01)
FLAGS_DATA(SUB, NORMAL,   0x02)
FLAGS_DATA(SUB, ONSCREEN, 0x04)
FLAGS_DATA(SUB, SIGNED,   0x08)
FLAGS_POSTAMBLE(SubtitleProps, uint8_t)
