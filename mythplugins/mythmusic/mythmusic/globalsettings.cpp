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

class GlobalSetting: public SimpleDBStorage, virtual public Configurable {
public:
    GlobalSetting(QString name):
        SimpleDBStorage("settings", "data") {
        setName(name);
    };

protected:
    virtual QString whereClause(void) {
        return QString("value = '%1' AND hostname = '%2'")
                       .arg(getName()).arg(gContext->GetHostName());
    };

    virtual QString setClause(void) {
        return QString("value = '%1', data = '%2', hostname = '%3'")
                       .arg(getName()).arg(getValue())
                       .arg(gContext->GetHostName());
    };
};

// General Settings

class SetMusicDirectory: public LineEditSetting, public GlobalSetting {
public:
    SetMusicDirectory():
        GlobalSetting("MusicLocation") {
        setLabel(QObject::tr("Directory to hold music"));
        setValue("/mnt/store/music/");
        setHelpText(QObject::tr("This directory must exist, and the user "
                    "running MythMusic needs to have write permission "
                    "to the directory."));
    };
};

class MusicAudioDevice: public ComboBoxSetting, public GlobalSetting {
public:
    MusicAudioDevice() : ComboBoxSetting(true),
                         GlobalSetting("AudioDevice") 
    {
        setLabel(QObject::tr("Audio device"));
        QDir dev("/dev", "dsp*", QDir::Name, QDir::System);
        fillSelectionsFromDir(dev);
        dev.setNameFilter("adsp*");
        fillSelectionsFromDir(dev);

        dev.setNameFilter("dsp*");
        dev.setPath("/dev/sound");
        fillSelectionsFromDir(dev);
        dev.setNameFilter("adsp*");
        fillSelectionsFromDir(dev);
        setHelpText(QObject::tr("Audio Device used for playback."));
    }
};

class CDDevice: public ComboBoxSetting, public GlobalSetting {
public:
    CDDevice() : ComboBoxSetting(true),
      GlobalSetting("CDDevice") {
        setLabel(QObject::tr("CD device"));
        QDir dev("/dev", "cdrom*", QDir::Name, QDir::System);
        fillSelectionsFromDir(dev);
        dev.setNameFilter("scd*");
        fillSelectionsFromDir(dev);
        dev.setNameFilter("hd*");
        fillSelectionsFromDir(dev);

        dev.setNameFilter("cdrom*");
        dev.setPath("/dev/cdroms");
        fillSelectionsFromDir(dev);
        setHelpText(QObject::tr("CDRom device used for ripping/playback."));
    }
};

class TreeLevels: public LineEditSetting, public GlobalSetting {
public:
    TreeLevels():
        GlobalSetting("TreeLevels") {
        setLabel(QObject::tr("Tree Sorting"));
        setValue("splitartist artist album title");
        setHelpText(QObject::tr("Order in which to sort the Music "
                    "Tree. Possible values are a space-separated list of "
                    "genre, splitartist, artist, album, and title OR the "
                    "keyword \"directory\" to indicate that "
                    "the onscreen tree mirrors the filesystem."));
    };
};

class PostCDRipScript: public LineEditSetting, public GlobalSetting {
public:
    PostCDRipScript():
        GlobalSetting("PostCDRipScript") {
        setLabel(QObject::tr("Script Path"));
        setValue("");
        setHelpText(QObject::tr("If present this script will be executed "
                    "after a CD Rip is completed."));
    };
};

class NonID3FileNameFormat: public LineEditSetting, public GlobalSetting {
public:
    NonID3FileNameFormat():
        GlobalSetting("NonID3FileNameFormat") {
        setLabel(QObject::tr("Filename Format"));
        setValue("GENRE/ARTIST/ALBUM/TRACK_TITLE");
        setHelpText(QObject::tr("Directory and filename Format used to grab "
                    "information if no ID3 information is found."));
    };
};


class IgnoreID3Tags: public CheckBoxSetting, public GlobalSetting {
public:
    IgnoreID3Tags():
        GlobalSetting("Ignore_ID3") {
        setLabel(QObject::tr("Ignore ID3 Tags"));
        setValue(false);
        setHelpText(QObject::tr("If set, MythMusic will skip checking ID3 tags "
                    "in files and just try to determine Genre, Artist, "
                    "Album, and Track number and title from the "
                    "filename."));
    };
};

class AutoLookupCD: public CheckBoxSetting, public GlobalSetting {
public:
    AutoLookupCD():
        GlobalSetting("AutoLookupCD") {
        setLabel(QObject::tr("Automatically lookup CDs"));
        setValue(true);
        setHelpText(QObject::tr("Automatically lookup an audio CD if it is "
                    "present and show its information in the "
                    "Music Selection Tree."));
    };
};

class KeyboardAccelerators: public CheckBoxSetting, public GlobalSetting {
public:
    KeyboardAccelerators():
        GlobalSetting("KeyboardAccelerators") {
        setLabel(QObject::tr("Use Keyboard/Remote Accelerated Buttons"));
        setValue(true);
        setHelpText(QObject::tr("If this is not set, you will need "
                    "to use arrow keys to select and activate "
                    "various functions."));
    };
};

// Encoder settings

class EncoderType: public ComboBoxSetting, public GlobalSetting {
public:
    EncoderType():
        GlobalSetting("EncoderType") {
        setLabel(QObject::tr("Encoding"));
        addSelection(QObject::tr("Ogg Vorbis"), "ogg");
        addSelection(QObject::tr("Lame (MP3)"), "mp3");
        setHelpText(QObject::tr("Audio encoder to use for CD ripping. "
                    "Note that the quality level 'Perfect' will use the "
		    "FLAC encoder."));
    };
};

class DefaultRipQuality: public ComboBoxSetting, public GlobalSetting {
public:
    DefaultRipQuality():
        GlobalSetting("DefaultRipQuality") {
        setLabel(QObject::tr("Default Rip Quality"));
        addSelection(QObject::tr("Low"), "0");
        addSelection(QObject::tr("Medium"), "1");
        addSelection(QObject::tr("High"), "2");
        addSelection(QObject::tr("Perfect"), "3");
        setHelpText(QObject::tr("Default quality for new CD rips."));
    };
};

class Mp3UseVBR: public CheckBoxSetting, public GlobalSetting {
public:
    Mp3UseVBR():
        GlobalSetting("Mp3UseVBR") {
        setLabel(QObject::tr("Use variable bitrates"));
        setValue(false);
        setHelpText(QObject::tr("If set, the MP3 encoder will use variable "
                    "bitrates (VBR) except for the low quality setting. "
                    "The Ogg encoder will always use variable bitrates."));
    };
};

class FilenameTemplate: public LineEditSetting, public GlobalSetting {
public:
    FilenameTemplate():
        GlobalSetting("FilenameTemplate") {
        setLabel(QObject::tr("File storage location"));
        setValue("ARTIST/ALBUM/TRACK-TITLE"); // Don't translate
        setHelpText(QObject::tr("Defines the location/name for new songs. "
                    "Valid tokens are: GENRE, ARTIST, ALBUM, "
                    "TRACK, TITLE, YEAR, / and -. '-' will be replaced "
                    "by the Token separator"));
    };
};

class TagSeparator: public LineEditSetting, public GlobalSetting {
public:
    TagSeparator():
        GlobalSetting("TagSeparator") {
        setLabel(QObject::tr("Token separator"));
        setValue(QObject::tr(" - "));
        setHelpText(QObject::tr("Filename tokens will be separated by "
                    "this string."));
    };
};

class NoWhitespace: public CheckBoxSetting, public GlobalSetting {
public:
    NoWhitespace():
    GlobalSetting("NoWhitespace") {
        setLabel(QObject::tr("Replace ' ' with '_'"));
        setValue(false);
        setHelpText(QObject::tr("If set, whitespace characters in filenames "
                    "will be replaced with underscore characters."));
    };
};

class ParanoiaLevel: public ComboBoxSetting, public GlobalSetting {
public:
    ParanoiaLevel():
        GlobalSetting("ParanoiaLevel") {
        setLabel(QObject::tr("Paranoia Level"));
        addSelection(QObject::tr("Full"), "Full");
        addSelection(QObject::tr("Faster"), "Faster");
        setHelpText(QObject::tr("Paranoia level of the CD ripper. Set to "
                    "faster if you're not concerned about "
                    "possible errors in the audio."));
    };
};

class EjectCD: public CheckBoxSetting, public GlobalSetting {
public:
    EjectCD():
        GlobalSetting("EjectCDAfterRipping") {
        setLabel(QObject::tr("Automatically eject CDs after ripping"));
        setValue(true);
        setHelpText(QObject::tr("If set, the CD tray will automatically open "
                    "after the CD has been ripped."));
    };
};

class SetRatingWeight: public SpinBoxSetting, public GlobalSetting {
public:
    SetRatingWeight():
        SpinBoxSetting(0, 100, 1),
        GlobalSetting("IntelliRatingWeight") {
        setLabel(QObject::tr("Rating Weight"));
        setValue(35);
        setHelpText(QObject::tr("Used in \"Smart\" Shuffle mode. "
                    "This weighting affects how much strength is "
                    "given to your rating of a given track when "
                    "ordering a group of songs."));
    };
};

class SetPlayCountWeight: public SpinBoxSetting, public GlobalSetting {
public:
    SetPlayCountWeight():
        SpinBoxSetting(0, 100, 1),
        GlobalSetting("IntelliPlayCountWeight") {
        setLabel(QObject::tr("Play Count Weight"));
        setValue(25);
        setHelpText(QObject::tr("Used in \"Smart\" Shuffle mode. "
                    "This weighting affects how much strength is "
                    "given to how many times a given track has been "
                    "played when ordering a group of songs."));
    };
};

class SetLastPlayWeight: public SpinBoxSetting, public GlobalSetting {
public:
    SetLastPlayWeight():
        SpinBoxSetting(0, 100, 1),
        GlobalSetting("IntelliLastPlayWeight") {
        setLabel(QObject::tr("Last Play Weight"));
        setValue(25);
        setHelpText(QObject::tr("Used in \"Smart\" Shuffle mode. "
                    "This weighting affects how much strength is "
                    "given to how long it has been since a given "
                    "track was played when ordering a group of songs."));
    };
};

class SetRandomWeight: public SpinBoxSetting, public GlobalSetting {
public:
    SetRandomWeight():
        SpinBoxSetting(0, 100, 1),
        GlobalSetting("IntelliRandomWeight") {
        setLabel(QObject::tr("Random Weight"));
        setValue(15);
        setHelpText(QObject::tr("Used in \"Smart\" Shuffle mode. "
                    "This weighting affects how much strength is "
                    "given to good old (peudo-)randomness "
                    "when ordering a group of songs."));
    };
};

class UseShowRatings: public CheckBoxSetting, public GlobalSetting {
public:
    UseShowRatings():
        GlobalSetting("MusicShowRatings") {
        setLabel(QObject::tr("Show Song Ratings"));
        setValue(false);
        setHelpText(QObject::tr("Show song ratings on the playback screen."));
    };
};

class UseListShuffled: public CheckBoxSetting, public GlobalSetting {
public:
    UseListShuffled():
        GlobalSetting("ListAsShuffled") {
        setLabel(QObject::tr("List as Shuffled"));
        setValue(false);
        setHelpText(QObject::tr("List songs on the playback screen "
                    "in the order they will be played."));
    };
};

class UseShowWholeTree: public CheckBoxSetting, public GlobalSetting {
public:
    UseShowWholeTree():
        GlobalSetting("ShowWholeTree") {
        setLabel(QObject::tr("Show entire music tree"));
        setValue(false);
        setHelpText(QObject::tr("If selected, you can navigate your entire music "
                    "tree from the playing screen."));
    };
};

//Player Settings

class PlayMode: public ComboBoxSetting, public GlobalSetting {
public:
    PlayMode():
        GlobalSetting("PlayMode") {
        setLabel(QObject::tr("Play mode"));
        addSelection(QObject::tr("Normal"), "Normal");
        addSelection(QObject::tr("Random"), "Random");
        addSelection(QObject::tr("Intelligent"), "Intelligent");
        setHelpText(QObject::tr("Starting shuffle mode for the player.  Can be "
                    "either normal, random, or intelligent (random)."));
    };
};

class VisualModeDelay: public SliderSetting, public GlobalSetting {
public:
    VisualModeDelay():
	SliderSetting(0, 100, 1),
        GlobalSetting("VisualModeDelay") {
        setLabel(QObject::tr("Delay before Visualizations start (seconds)"));
        setValue(0);
        setHelpText(QObject::tr("If set to 0, visualizations will never "
                    "automatically start."));
        };
};

class VisualCycleOnSongChange: public CheckBoxSetting, public GlobalSetting {
public:
    VisualCycleOnSongChange():
        GlobalSetting("VisualCycleOnSongChange") {
        setLabel(QObject::tr("Change Visualizer on each song"));
        setValue(false);
        setHelpText(QObject::tr("Change the visualizer when the song "
                    "change."));
    };
};

class VisualScaleWidth: public SpinBoxSetting, public GlobalSetting {
public:
    VisualScaleWidth():
        SpinBoxSetting(1, 2, 1),
        GlobalSetting("VisualScaleWidth") {
        setLabel(QObject::tr("Width for Visual Scaling"));
        setValue(1);
        setHelpText(QObject::tr("If set to \"2\", visualizations will be "
                    "scaled in half.  Currently only used by "
                    "the goom visualization.  Reduces CPU load "
                    "on slower machines."));
    };
};

class VisualScaleHeight: public SpinBoxSetting, public GlobalSetting {
public:
    VisualScaleHeight():
        SpinBoxSetting(1, 2, 1),
        GlobalSetting("VisualScaleHeight") {
        setLabel(QObject::tr("Height for Visual Scaling"));
        setValue(1);
        setHelpText(QObject::tr("If set to \"2\", visualizations will be "
                    "scaled in half.  Currently only used by "
                    "the goom visualization.  Reduces CPU load "
                    "on slower machines."));
    };
};

class VisualizationMode: public LineEditSetting, public GlobalSetting {
public:
    VisualizationMode():
        GlobalSetting("VisualMode") {
        setLabel(QObject::tr("Visualizations"));
        setValue(QObject::tr("Random"));
        setHelpText(QObject::tr("List of visualizations to use during playback."
                    " Possible values are space-separated list of ") +
                    QObject::tr("Random") + ", " + 
                    QObject::tr("MonoScope") + ", " + 
                    QObject::tr("StereoScope") + ", " + 
                    QObject::tr("Spectrum") + ", " + 
                    QObject::tr("BumpScope") + ", " + 
                    QObject::tr("Goom") + ", " + 
                    QObject::tr("Synaesthesia") + ", " +
                    QObject::tr("AlbumArt") + ", " +
                    QObject::tr("Gears") + ", " + QObject::tr("and") + " " + 
                    QObject::tr("Blank"));
    };
};

// CD Writer Settings

class CDWriterEnabled: public CheckBoxSetting, public GlobalSetting {
public:
    CDWriterEnabled():
        GlobalSetting("CDWriterEnabled") {
        setLabel(QObject::tr("Enable CD Writing."));
        setValue(true);
        setHelpText(QObject::tr("Requires a SCSI or an IDE-SCSI CD Writer."));
    };
};

class CDWriterDevice: public ComboBoxSetting, public GlobalSetting {
public:
    CDWriterDevice():
        GlobalSetting("CDWriterDevice") {

        QStringList args;
        QStringList result;

        args = "cdrecord";
        args += "--scanbus";

        QProcess proc(args);

        if (proc.start())
        {
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
                    addSelection(line.mid(24, 16), line.mid(1, 5));
                }
            }
        }

        setLabel(QObject::tr("CD-Writer Device"));
        setHelpText(QObject::tr("Select the SCSI Device for CD Writing.  If "
                    "your IDE device is not present, try adding "
                    "hdd(or hdc/hdb)=ide-scsi to your boot options"));
    };
};

class CDDiskSize: public ComboBoxSetting, public GlobalSetting {
public:
    CDDiskSize():
        GlobalSetting("CDDiskSize") {
        setLabel(QObject::tr("Disk Size"));
        addSelection(QObject::tr("650MB/75min"), "1");
        addSelection(QObject::tr("700MB/80min"), "2");
        setHelpText(QObject::tr("Default CD Capacity."));
    };
};

class CDCreateDir: public CheckBoxSetting, public GlobalSetting {
public:
    CDCreateDir():
        GlobalSetting("CDCreateDir") {
        setLabel(QObject::tr("Enable directories on MP3 Creation"));
        setValue(true);
        setHelpText("");
    };
};

class CDWriteSpeed: public ComboBoxSetting, public GlobalSetting {
public:
    CDWriteSpeed():
        GlobalSetting("CDWriteSpeed") {
        setLabel(QObject::tr("CD Write Speed"));
        addSelection(QObject::tr("Auto"), "0");
        addSelection(QObject::tr("1x"), "1");
        addSelection(QObject::tr("2x"), "2");
        addSelection(QObject::tr("4x"), "4");
        addSelection(QObject::tr("8x"), "8");
        addSelection(QObject::tr("16x"), "16");
        setHelpText(QObject::tr("CD Writer speed. Auto will use the recomended "
                    "speed."));
    };
};

class CDBlankType: public ComboBoxSetting, public GlobalSetting {
public:
    CDBlankType():
        GlobalSetting("CDBlankType") {
        setLabel(QObject::tr("CD Blanking Type"));
        addSelection(QObject::tr("Fast"), "fast");
        addSelection(QObject::tr("Complete"), "all");
        setHelpText(QObject::tr("Blanking Method. Fast takes 1 minute. "
                    "Complete can take up to 20 minutes."));
    };
};

MusicGeneralSettings::MusicGeneralSettings()
{
    VerticalConfigurationGroup* general = new VerticalConfigurationGroup(false);
    general->setLabel(QObject::tr("General Settings"));
    general->addChild(new SetMusicDirectory());
    general->addChild(new MusicAudioDevice());
    general->addChild(new CDDevice());
    general->addChild(new TreeLevels());
    general->addChild(new NonID3FileNameFormat());
    general->addChild(new IgnoreID3Tags());
    general->addChild(new AutoLookupCD());
    general->addChild(new KeyboardAccelerators());
    addChild(general);

    VerticalConfigurationGroup* general2 = new VerticalConfigurationGroup(false);
    general2->setLabel(QObject::tr("CD Recording Settings"));
    general2->addChild(new CDWriterEnabled());
    general2->addChild(new CDWriterDevice());
    general2->addChild(new CDDiskSize());
    general2->addChild(new CDCreateDir());
    general2->addChild(new CDWriteSpeed());
    general2->addChild(new CDBlankType());
    addChild(general2);
}

MusicPlayerSettings::MusicPlayerSettings()
{
    VerticalConfigurationGroup* playersettings = new VerticalConfigurationGroup(false);
    playersettings->setLabel(QObject::tr("Playback Settings"));
    playersettings->addChild(new PlayMode());
    playersettings->addChild(new SetRatingWeight());
    playersettings->addChild(new SetPlayCountWeight());
    playersettings->addChild(new SetLastPlayWeight());
    playersettings->addChild(new SetRandomWeight());
    playersettings->addChild(new UseShowRatings());
    playersettings->addChild(new UseShowWholeTree());
    playersettings->addChild(new UseListShuffled());
    addChild(playersettings);

    VerticalConfigurationGroup* playersettings2 = new VerticalConfigurationGroup(false);
    playersettings2->setLabel(QObject::tr("Visualization Settings"));
    playersettings2->addChild(new VisualizationMode());
    playersettings2->addChild(new VisualCycleOnSongChange());
    playersettings2->addChild(new VisualModeDelay());
    playersettings2->addChild(new VisualScaleWidth());
    playersettings2->addChild(new VisualScaleHeight());
    addChild(playersettings2);
}

MusicRipperSettings::MusicRipperSettings()
{
    VerticalConfigurationGroup* rippersettings = new VerticalConfigurationGroup(false);
    rippersettings->setLabel(QObject::tr("CD Ripper Settings"));
    rippersettings->addChild(new ParanoiaLevel());
    rippersettings->addChild(new FilenameTemplate());
    rippersettings->addChild(new TagSeparator());
    rippersettings->addChild(new NoWhitespace());
    rippersettings->addChild(new PostCDRipScript());
    rippersettings->addChild(new EjectCD());
    addChild(rippersettings);

    VerticalConfigurationGroup* encodersettings = new VerticalConfigurationGroup(false);
    encodersettings->setLabel(QObject::tr("CD Ripper Settings (part 2)"));
    encodersettings->addChild(new EncoderType());
    encodersettings->addChild(new DefaultRipQuality());
    encodersettings->addChild(new Mp3UseVBR());
    addChild(encodersettings);
}

