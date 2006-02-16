#include "mythtv/mythcontext.h"

#include <unistd.h>

#include "globalsettings.h"
#include <qfile.h>
#include <qdialog.h>
#include <qcursor.h>
#include <qdir.h>
#include <qimage.h>
#include <qstringlist.h>
#include <qprocess.h>
#include <qapplication.h>
#include <mythtv/util.h>

// General Settings

static HostLineEdit *SetMusicDirectory()
{
    HostLineEdit *gc = new HostLineEdit("MusicLocation");
    gc->setLabel(QObject::tr("Directory to hold music"));
    gc->setValue("/mnt/store/music/");
    gc->setHelpText(QObject::tr("This directory must exist, and the user "
                    "running MythMusic needs to have write permission "
                    "to the directory."));
    return gc;
};

static HostComboBox *MusicAudioDevice()
{
    HostComboBox *gc = new HostComboBox("MusicAudioDevice", true);
    gc->setLabel(QObject::tr("Audio device"));
    gc->addSelection(QObject::tr("default"), "default");
    QDir dev("/dev", "dsp*", QDir::Name, QDir::System);
    gc->fillSelectionsFromDir(dev);
    dev.setNameFilter("adsp*");
    gc->fillSelectionsFromDir(dev);

    dev.setNameFilter("dsp*");
    dev.setPath("/dev/sound");
    gc->fillSelectionsFromDir(dev);
    dev.setNameFilter("adsp*");
    gc->fillSelectionsFromDir(dev);
    gc->setHelpText(QObject::tr("Audio Device used for playback. 'default' will use the device specified in MythTV"));
    return gc;
};

static HostComboBox *CDDevice()
{
    HostComboBox *gc = new HostComboBox("CDDevice", true);
    gc->setLabel(QObject::tr("CD device"));
    QDir dev("/dev", "cdrom*", QDir::Name, QDir::System);
    gc->fillSelectionsFromDir(dev);
    dev.setNameFilter("scd*");
    gc->fillSelectionsFromDir(dev);
    dev.setNameFilter("hd*");
    gc->fillSelectionsFromDir(dev);

    dev.setNameFilter("cdrom*");
    dev.setPath("/dev/cdroms");
    gc->fillSelectionsFromDir(dev);
    gc->setHelpText(QObject::tr("CDRom device used for ripping/playback."));
    return gc;
};

static HostLineEdit *TreeLevels()
{
    HostLineEdit *gc = new HostLineEdit("TreeLevels", true);
    gc->setLabel(QObject::tr("Tree Sorting"));
    gc->setValue("splitartist artist album title");
    gc->setHelpText(QObject::tr("Order in which to sort the Music "
                    "Tree. Possible values are a space-separated list of "
                    "genre, splitartist, splitartist1, artist, album, and "
                    "title OR the keyword \"directory\" to indicate that "
                    "the onscreen tree mirrors the filesystem."));
    return gc;
};

static HostLineEdit *PostCDRipScript()
{
    HostLineEdit *gc = new HostLineEdit("PostCDRipScript");
    gc->setLabel(QObject::tr("Script Path"));
    gc->setValue("");
    gc->setHelpText(QObject::tr("If present this script will be executed "
                    "after a CD Rip is completed."));
    return gc;
};

static HostLineEdit *NonID3FileNameFormat()
{
    HostLineEdit *gc = new HostLineEdit("NonID3FileNameFormat");
    gc->setLabel(QObject::tr("Filename Format"));
    gc->setValue("GENRE/ARTIST/ALBUM/TRACK_TITLE");
    gc->setHelpText(QObject::tr("Directory and filename Format used to grab "
                    "information if no ID3 information is found."));
    return gc;
};


static HostCheckBox *IgnoreID3Tags()
{
    HostCheckBox *gc = new HostCheckBox("Ignore_ID3");
    gc->setLabel(QObject::tr("Ignore ID3 Tags"));
    gc->setValue(false);
    gc->setHelpText(QObject::tr("If set, MythMusic will skip checking ID3 tags "
                    "in files and just try to determine Genre, Artist, "
                    "Album, and Track number and title from the "
                    "filename."));
    return gc;
};

static HostCheckBox *OnlyImportNewMusic()
{
    HostCheckBox *gc = new HostCheckBox("OnlyImportNewMusic");
    gc->setLabel(QObject::tr("Only Import new music."));
    gc->setValue(false);
    gc->setHelpText(QObject::tr("Checks the database for duplicates when importing/ripping CDs."));
    return gc;
};

static HostCheckBox *AutoLookupCD()
{
    HostCheckBox *gc = new HostCheckBox("AutoLookupCD");
    gc->setLabel(QObject::tr("Automatically lookup CDs"));
    gc->setValue(true);
    gc->setHelpText(QObject::tr("Automatically lookup an audio CD if it is "
                    "present and show its information in the "
                    "Music Selection Tree."));
    return gc;
};

static HostCheckBox *AutoPlayCD()
{
    HostCheckBox *gc = new HostCheckBox("AutoPlayCD");
    gc->setLabel(QObject::tr("Automatically play CDs"));
    gc->setValue(false);
    gc->setHelpText(QObject::tr("Automatically put a new CD on the "
                                "playlist and start playing the CD."));
    return gc;
};

static HostCheckBox *KeyboardAccelerators()
{
    HostCheckBox *gc = new HostCheckBox("KeyboardAccelerators");
    gc->setLabel(QObject::tr("Use Keyboard/Remote Accelerated Buttons"));
    gc->setValue(true);
    gc->setHelpText(QObject::tr("If this is not set, you will need "
                    "to use arrow keys to select and activate "
                    "various functions."));
    return gc;
};

// Encoder settings

static HostComboBox *EncoderType()
{
    HostComboBox *gc = new HostComboBox("EncoderType");
    gc->setLabel(QObject::tr("Encoding"));
    gc->addSelection(QObject::tr("Ogg Vorbis"), "ogg");
    gc->addSelection(QObject::tr("Lame (MP3)"), "mp3");
    gc->setHelpText(QObject::tr("Audio encoder to use for CD ripping. "
                    "Note that the quality level 'Perfect' will use the "
		                "FLAC encoder."));
    return gc;
};

static HostComboBox *DefaultRipQuality()
{
    HostComboBox *gc = new HostComboBox("DefaultRipQuality");
    gc->setLabel(QObject::tr("Default Rip Quality"));
    gc->addSelection(QObject::tr("Low"), "0");
    gc->addSelection(QObject::tr("Medium"), "1");
    gc->addSelection(QObject::tr("High"), "2");
    gc->addSelection(QObject::tr("Perfect"), "3");
    gc->setHelpText(QObject::tr("Default quality for new CD rips."));
    return gc;
};

static HostCheckBox *Mp3UseVBR()
{
    HostCheckBox *gc = new HostCheckBox("Mp3UseVBR");
    gc->setLabel(QObject::tr("Use variable bitrates"));
    gc->setValue(false);
    gc->setHelpText(QObject::tr("If set, the MP3 encoder will use variable "
                    "bitrates (VBR) except for the low quality setting. "
                    "The Ogg encoder will always use variable bitrates."));
    return gc;
};

static HostLineEdit *FilenameTemplate()
{
    HostLineEdit *gc = new HostLineEdit("FilenameTemplate");
    gc->setLabel(QObject::tr("File storage location"));
    gc->setValue("ARTIST/ALBUM/TRACK-TITLE"); // Don't translate
    gc->setHelpText(QObject::tr("Defines the location/name for new songs. "
                    "Valid tokens are: GENRE, ARTIST, ALBUM, "
                    "TRACK, TITLE, YEAR, / and -. '-' will be replaced "
                    "by the Token separator"));
    return gc;
};

static HostLineEdit *TagSeparator()
{
    HostLineEdit *gc = new HostLineEdit("TagSeparator");
    gc->setLabel(QObject::tr("Token separator"));
    gc->setValue(" - ");
    gc->setHelpText(QObject::tr("Filename tokens will be separated by "
                    "this string."));
    return gc;
};

static HostCheckBox *NoWhitespace()
{
    HostCheckBox *gc = new HostCheckBox("NoWhitespace");
    gc->setLabel(QObject::tr("Replace ' ' with '_'"));
    gc->setValue(false);
    gc->setHelpText(QObject::tr("If set, whitespace characters in filenames "
                    "will be replaced with underscore characters."));
    return gc;
};

static HostComboBox *ParanoiaLevel()
{
    HostComboBox *gc = new HostComboBox("ParanoiaLevel");
    gc->setLabel(QObject::tr("Paranoia Level"));
    gc->addSelection(QObject::tr("Full"), "Full");
    gc->addSelection(QObject::tr("Faster"), "Faster");
    gc->setHelpText(QObject::tr("Paranoia level of the CD ripper. Set to "
                    "faster if you're not concerned about "
                    "possible errors in the audio."));
    return gc;
};

static HostCheckBox *EjectCD()
{
    HostCheckBox *gc = new HostCheckBox("EjectCDAfterRipping");
    gc->setLabel(QObject::tr("Automatically eject CDs after ripping"));
    gc->setValue(true);
    gc->setHelpText(QObject::tr("If set, the CD tray will automatically open "
                    "after the CD has been ripped."));
    return gc;
};

static HostSpinBox *SetRatingWeight()
{
    HostSpinBox *gc = new HostSpinBox("IntelliRatingWeight", 0, 100, 1);
    gc->setLabel(QObject::tr("Rating Weight"));
    gc->setValue(35);
    gc->setHelpText(QObject::tr("Used in \"Smart\" Shuffle mode. "
                    "This weighting affects how much strength is "
                    "given to your rating of a given track when "
                    "ordering a group of songs."));
    return gc;
};

static HostSpinBox *SetPlayCountWeight()
{
    HostSpinBox *gc = new HostSpinBox("IntelliPlayCountWeight", 0, 100, 1);
    gc->setLabel(QObject::tr("Play Count Weight"));
    gc->setValue(25);
    gc->setHelpText(QObject::tr("Used in \"Smart\" Shuffle mode. "
                    "This weighting affects how much strength is "
                    "given to how many times a given track has been "
                    "played when ordering a group of songs."));
    return gc;
};

static HostSpinBox *SetLastPlayWeight()
{
    HostSpinBox *gc = new HostSpinBox("IntelliLastPlayWeight", 0, 100, 1);
    gc->setLabel(QObject::tr("Last Play Weight"));
    gc->setValue(25);
    gc->setHelpText(QObject::tr("Used in \"Smart\" Shuffle mode. "
                    "This weighting affects how much strength is "
                    "given to how long it has been since a given "
                    "track was played when ordering a group of songs."));
    return gc;
};

static HostSpinBox *SetRandomWeight()
{
    HostSpinBox *gc = new HostSpinBox("IntelliRandomWeight", 0, 100, 1);
    gc->setLabel(QObject::tr("Random Weight"));
    gc->setValue(15);
    gc->setHelpText(QObject::tr("Used in \"Smart\" Shuffle mode. "
                    "This weighting affects how much strength is "
                    "given to good old (peudo-)randomness "
                    "when ordering a group of songs."));
    return gc;
};

static HostCheckBox *UseShowRatings()
{
    HostCheckBox *gc = new HostCheckBox("MusicShowRatings");
    gc->setLabel(QObject::tr("Show Song Ratings"));
    gc->setValue(false);
    gc->setHelpText(QObject::tr("Show song ratings on the playback screen."));
    return gc;
};

static HostCheckBox *UseListShuffled()
{
    HostCheckBox *gc = new HostCheckBox("ListAsShuffled");
    gc->setLabel(QObject::tr("List as Shuffled"));
    gc->setValue(false);
    gc->setHelpText(QObject::tr("List songs on the playback screen "
                    "in the order they will be played."));
    return gc;
};

static HostCheckBox *UseShowWholeTree()
{
    HostCheckBox *gc = new HostCheckBox("ShowWholeTree");
    gc->setLabel(QObject::tr("Show entire music tree"));
    gc->setValue(false);
    gc->setHelpText(QObject::tr("If selected, you can navigate your entire "
                    "music tree from the playing screen."));
    return gc;
};

//Player Settings

static HostComboBox *PlayMode()
{
    HostComboBox *gc = new HostComboBox("PlayMode");
    gc->setLabel(QObject::tr("Play mode"));
    gc->addSelection(QObject::tr("Normal"), "Normal");
    gc->addSelection(QObject::tr("Random"), "Random");
    gc->addSelection(QObject::tr("Intelligent"), "Intelligent");
    gc->setHelpText(QObject::tr("Starting shuffle mode for the player.  Can be "
                    "either normal, random, or intelligent (random)."));
    return gc;
};

static HostSlider *VisualModeDelay()
{
    HostSlider *gc = new HostSlider("VisualModeDelay", 0, 100, 1);
    gc->setLabel(QObject::tr("Delay before Visualizations start (seconds)"));
    gc->setValue(0);
    gc->setHelpText(QObject::tr("If set to 0, visualizations will never "
                    "automatically start."));
    return gc;
};

static HostCheckBox *VisualCycleOnSongChange()
{
    HostCheckBox *gc = new HostCheckBox("VisualCycleOnSongChange");
    gc->setLabel(QObject::tr("Change Visualizer on each song"));
    gc->setValue(false);
    gc->setHelpText(QObject::tr("Change the visualizer when the song "
                    "change."));
    return gc;
};

static HostSpinBox *VisualScaleWidth()
{
    HostSpinBox *gc = new HostSpinBox("VisualScaleWidth", 1, 2, 1);
    gc->setLabel(QObject::tr("Width for Visual Scaling"));
    gc->setValue(1);
    gc->setHelpText(QObject::tr("If set to \"2\", visualizations will be "
                    "scaled in half.  Currently only used by "
                    "the goom visualization.  Reduces CPU load "
                    "on slower machines."));
    return gc;
};

static HostSpinBox *VisualScaleHeight()
{
    HostSpinBox *gc = new HostSpinBox("VisualScaleHeight", 1, 2, 1);
    gc->setLabel(QObject::tr("Height for Visual Scaling"));
    gc->setValue(1);
    gc->setHelpText(QObject::tr("If set to \"2\", visualizations will be "
                    "scaled in half.  Currently only used by "
                    "the goom visualization.  Reduces CPU load "
                    "on slower machines."));
    return gc;
};

static HostLineEdit *VisualizationMode()
{
    HostLineEdit *gc = new HostLineEdit("VisualMode");
    gc->setLabel(QObject::tr("Visualizations"));
    gc->setValue("Random");
    gc->setHelpText(QObject::tr("List of visualizations to use during playback. "
                    "Possible values are space-separated list of ") + "Random, "
                    "MonoScope, StereoScope, Spectrum, BumpScope, Goom, "
                    "Synaesthesia, AlbumArt, Gears, " + QObject::tr("and") +
                    " Blank");
    return gc;
};

// CD Writer Settings

static HostCheckBox *CDWriterEnabled()
{
    HostCheckBox *gc = new HostCheckBox("CDWriterEnabled");
    gc->setLabel(QObject::tr("Enable CD Writing."));
    gc->setValue(true);
    gc->setHelpText(QObject::tr("Requires a SCSI or an IDE-SCSI CD Writer."));
    return gc;
};

static HostComboBox *CDWriterDevice()
{
    HostComboBox *gc = new HostComboBox("CDWriterDevice");

    QString argadd[3]  = { "", "-dev=ATA", "-dev=ATAPI" };
    QString prepend[3] = { "", "ATA:", "ATAPI:" };

    for (int i = 0; i < 3; i++)
    {
        QStringList args;
        QStringList result;

        args = "cdrecord";
        args += "--scanbus";

        if (argadd[i].length() > 1)
            args += argadd[i];

        QProcess proc(args);

        MythTimer totaltimer;
    
        if (proc.start())
        {
            totaltimer.start();

            while (1)
            {
                while (proc.canReadLineStdout())
                    result += proc.readLineStdout();
                if (proc.isRunning())
                {
                    qApp->processEvents();
                    usleep(10000);
                }
                else
                {
                    if (!proc.normalExit())
                        cerr << "Failed to run 'cdrecord --scanbus'\n";
                    break;
                }

                if (totaltimer.elapsed() > 1500)
                    proc.kill();
            }
        }
        else
            cerr << "Failed to run 'cdrecord --scanbus'\n";

        while (proc.canReadLineStdout())
            result += proc.readLineStdout();

        for (QStringList::Iterator it = result.begin(); it != result.end();
             ++it)
        {
            QString line = *it;
            if (line.length() > 12)
            {
                if (line[10] == ')' && line[12] != '*')
                {
                    gc->addSelection(line.mid(24, 16), prepend[i] + 
                                                       line.mid(1, 5));
cout << "adding: " << prepend[i] + line.mid(1, 5) << " -- " << line.mid(24, 16) << endl;
                }
            }
        }
    }
    
    gc->setLabel(QObject::tr("CD-Writer Device"));
    gc->setHelpText(QObject::tr("Select the SCSI or IDE Device for CD Writing."));
    return gc;
};

static HostComboBox *CDDiskSize()
{
    HostComboBox *gc = new HostComboBox("CDDiskSize");
    gc->setLabel(QObject::tr("Disk Size"));
    gc->addSelection(QObject::tr("650MB/75min"), "1");
    gc->addSelection(QObject::tr("700MB/80min"), "2");
    gc->setHelpText(QObject::tr("Default CD Capacity."));
    return gc;
};

static HostCheckBox *CDCreateDir()
{
    HostCheckBox *gc = new HostCheckBox("CDCreateDir");
    gc->setLabel(QObject::tr("Enable directories on MP3 Creation"));
    gc->setValue(true);
    gc->setHelpText("");
    return gc;
};

static HostComboBox *CDWriteSpeed()
{
    HostComboBox *gc = new HostComboBox("CDWriteSpeed");
    gc->setLabel(QObject::tr("CD Write Speed"));
    gc->addSelection(QObject::tr("Auto"), "0");
    gc->addSelection("1x", "1");
    gc->addSelection("2x", "2");
    gc->addSelection("4x", "4");
    gc->addSelection("8x", "8");
    gc->addSelection("16x", "16");
    gc->setHelpText(QObject::tr("CD Writer speed. Auto will use the recomended "
                    "speed."));
    return gc;
};

static HostComboBox *CDBlankType()
{
    HostComboBox *gc = new HostComboBox("CDBlankType");
    gc->setLabel(QObject::tr("CD Blanking Type"));
    gc->addSelection(QObject::tr("Fast"), "fast");
    gc->addSelection(QObject::tr("Complete"), "all");
    gc->setHelpText(QObject::tr("Blanking Method. Fast takes 1 minute. "
                    "Complete can take up to 20 minutes."));
    return gc;
};

MusicGeneralSettings::MusicGeneralSettings()
{
    VerticalConfigurationGroup* general = new VerticalConfigurationGroup(false);
    general->setLabel(QObject::tr("General Settings"));
    general->addChild(SetMusicDirectory());
    general->addChild(MusicAudioDevice());
    general->addChild(CDDevice());
    general->addChild(TreeLevels());
    general->addChild(NonID3FileNameFormat());
    general->addChild(IgnoreID3Tags());
    general->addChild(AutoLookupCD());
    general->addChild(AutoPlayCD());
    general->addChild(KeyboardAccelerators());
    addChild(general);

    VerticalConfigurationGroup* general2 = new VerticalConfigurationGroup(false);
    general2->setLabel(QObject::tr("CD Recording Settings"));
    general2->addChild(CDWriterEnabled());
    general2->addChild(CDWriterDevice());
    general2->addChild(CDDiskSize());
    general2->addChild(CDCreateDir());
    general2->addChild(CDWriteSpeed());
    general2->addChild(CDBlankType());
    addChild(general2);
}

MusicPlayerSettings::MusicPlayerSettings()
{
    VerticalConfigurationGroup* playersettings = new VerticalConfigurationGroup(false);
    playersettings->setLabel(QObject::tr("Playback Settings"));
    playersettings->addChild(PlayMode());
    playersettings->addChild(SetRatingWeight());
    playersettings->addChild(SetPlayCountWeight());
    playersettings->addChild(SetLastPlayWeight());
    playersettings->addChild(SetRandomWeight());
    playersettings->addChild(UseShowRatings());
    playersettings->addChild(UseShowWholeTree());
    playersettings->addChild(UseListShuffled());
    addChild(playersettings);

    VerticalConfigurationGroup* playersettings2 = new VerticalConfigurationGroup(false);
    playersettings2->setLabel(QObject::tr("Visualization Settings"));
    playersettings2->addChild(VisualizationMode());
    playersettings2->addChild(VisualCycleOnSongChange());
    playersettings2->addChild(VisualModeDelay());
    playersettings2->addChild(VisualScaleWidth());
    playersettings2->addChild(VisualScaleHeight());
    addChild(playersettings2);
}

MusicRipperSettings::MusicRipperSettings()
{
    VerticalConfigurationGroup* rippersettings = new VerticalConfigurationGroup(false);
    rippersettings->setLabel(QObject::tr("CD Ripper Settings"));
    rippersettings->addChild(ParanoiaLevel());
    rippersettings->addChild(FilenameTemplate());
    rippersettings->addChild(TagSeparator());
    rippersettings->addChild(NoWhitespace());
    rippersettings->addChild(PostCDRipScript());
    rippersettings->addChild(EjectCD());
    rippersettings->addChild(OnlyImportNewMusic());
    addChild(rippersettings);

    VerticalConfigurationGroup* encodersettings = new VerticalConfigurationGroup(false);
    encodersettings->setLabel(QObject::tr("CD Ripper Settings (part 2)"));
    encodersettings->addChild(EncoderType());
    encodersettings->addChild(DefaultRipQuality());
    encodersettings->addChild(Mp3UseVBR());
    addChild(encodersettings);
}

