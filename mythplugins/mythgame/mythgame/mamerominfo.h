#ifndef MAMEROMINFO_H_
#define MAMEROMINFO_H_

#include <qstring.h>
#include "rominfo.h"

typedef enum _RomStatus {
        CORRECT,
        INCORRECT,
        MISSING,
        MAY_WORK
} RomStatus;


class MameRomInfo : public RomInfo
{
  public:
    MameRomInfo(QString lromname = "",
                QString lsystem = "",
                QString lgamename ="",
                QString lgenre = "",
                int lyear = 0,
                bool limage_searched = false) :
            RomInfo(lromname, lsystem, lgamename, lgenre, lyear, 
                    limage_searched)
            {}
    MameRomInfo(const MameRomInfo &lhs) :
                RomInfo(lhs)
            {
                manu = lhs.manu;
                cloneof = lhs.cloneof;
                romof = lhs.romof;
                driver = lhs.driver;
                cpu1 = lhs.cpu1;
                cpu2 = lhs.cpu2;
                cpu3 = lhs.cpu3;
                cpu4 = lhs.cpu4;
                sound1 = lhs.sound1;
                sound2 = lhs.sound2;
                sound3 = lhs.sound3;
                sound4 = lhs.sound4;
                control = lhs.control;
                category = lhs.category;
                mame_ver_added = lhs.mame_ver_added;
                num_players = lhs.num_players;
                num_buttons = lhs.num_buttons;
                status = lhs.status;
                working = lhs.working;
                vector = lhs.vector;
                favourite = lhs.favourite;
                timesplayed = lhs.timesplayed;
                image_searched = lhs.image_searched;
                rom_path = lhs.rom_path;
            }
    MameRomInfo(const RomInfo &lhs) :
                RomInfo(lhs) {}

    virtual ~MameRomInfo() {}

    QString Manu() const { return manu; }
    void setManu(const QString &lmanu) { manu = lmanu; }

    QString Cloneof() const { return cloneof; }
    void setCloneof(const QString &lcloneof) { cloneof = lcloneof; }

    QString Romof() const { return romof; }
    void setRomof(const QString &lromof) { romof = lromof; }

    QString Driver() const { return driver; }
    void setDriver(const QString &ldriver) { driver = ldriver; }

    QString Cpu1() const { return cpu1; }
    void setCpu1(const QString &lcpu1) { cpu1 = lcpu1; }

    QString Cpu2() const { return cpu2; }
    void setCpu2(const QString &lcpu2) { cpu2 = lcpu2; }

    QString Cpu3() const { return cpu3; }
    void setCpu3(const QString &lcpu3) { cpu3 = lcpu3; }

    QString Cpu4() const { return cpu4; }
    void setCpu4(const QString &lcpu4) { cpu4 = lcpu4; }

    QString Sound1() const { return sound1; }
    void setSound1(const QString &lsound1) { sound1 = lsound1; }

    QString Sound2() const { return sound2; }
    void setSound2(const QString &lsound2) { sound2 = lsound2; }

    QString Sound3() const { return sound3; }
    void setSound3(const QString &lsound3) { sound3 = lsound3; }

    QString Sound4() const { return sound4; }
    void setSound4(const QString &lsound4) { sound4 = lsound4; }

    QString Control() const { return control; }
    void setControl(const QString &lcontrol) { control = lcontrol; }

    QString Category() const { return category; }
    void setCategory(const QString &lcategory) { category = lcategory; }

    QString Mame_ver_added() const { return mame_ver_added; }
    void setMame_ver_added(const QString &lmame_ver_added) { mame_ver_added = lmame_ver_added; }

    int Num_players() const { return num_players; }
    void setNum_players(int lnum_players) { num_players = lnum_players; }

    int Num_buttons() const { return num_buttons; }
    void setNum_buttons(int lnum_buttons) { num_buttons = lnum_buttons; }

    RomStatus Status() const { return status; }
    void setStatus(RomStatus lstatus) { status = lstatus; }

    bool Working() const { return working; }
    void setWorking(bool lworking) { working = lworking; }

    bool Vector() const { return vector; }
    void setVector(bool lvector) { vector = lvector; }

    bool Favourite() const { return favourite; }
    void setFavourite(bool lfavourite) { favourite = lfavourite; }

    bool ImageSearched() const { return image_searched; }
    void setImageSearched(bool limage_searched) { image_searched = limage_searched; }

    int Timesplayed() const { return timesplayed; }
    void setTimesplayed(int ltimesplayed) { timesplayed = ltimesplayed; }
    virtual void fillData();

    virtual bool FindImage(QString type,QString *result);

    QString RomPath() const { return rom_path; }
    void setRomPath(QString lrom_path) { rom_path = lrom_path; }

  protected:
    QString manu;
    QString cloneof;
    QString romof;
    QString driver;
    QString cpu1;
    QString cpu2;
    QString cpu3;
    QString cpu4;
    QString sound1;
    QString sound2;
    QString sound3;
    QString sound4;
    QString control;
    QString category;
    QString mame_ver_added;
    QString rom_path;
    int num_players;
    int num_buttons;
    RomStatus status;
    bool working;
    bool vector;
    bool favourite;
    int timesplayed;
    bool image_searched;
};

#endif
