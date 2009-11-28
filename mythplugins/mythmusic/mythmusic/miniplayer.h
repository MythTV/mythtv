#ifndef MINIPLAYER_H_
#define MINIPLAYER_H_

#include <mythscreentype.h>

class MusicPlayer;
class QTimer;
class Metadata;
class MythUIText;
class MythUIImage;
class MythUIStateType;
class MythUIProgressBar;

class MPUBLIC MiniPlayer : public MythScreenType
{
  Q_OBJECT

  public:
    MiniPlayer(MythScreenStack *parent, MusicPlayer *parentPlayer);
    ~MiniPlayer();

    bool Create(void);
    bool keyPressEvent(QKeyEvent *);
    void customEvent(QEvent*);

  public slots:
    void timerTimeout(void);
    void showInfoTimeout(void);

  private:
    QString getTimeString(int exTime, int maxTime);
    void    updateTrackInfo(Metadata *mdata);
    void    seekforward(void);
    void    seekback(void);
    void    seek(int pos);
    void    increaseRating(void);
    void    decreaseRating(void);
    void    showShuffleMode(void);
    void    showRepeatMode(void);
    void    showVolume(void);
    void    showSpeed(void);
    void    showAutoMode(void);

    int           m_currTime, m_maxTime;

    MusicPlayer  *m_parentPlayer;

    QTimer       *m_displayTimer;
    QTimer       *m_infoTimer;

    bool          m_showingInfo;

    MythUIText   *m_timeText;
    MythUIText   *m_infoText;
    MythUIText   *m_volText;
    MythUIImage  *m_coverImage;

    MythUIProgressBar     *m_progressBar;
    MythUIStateType       *m_ratingsState;

    QString       m_volFormat;
};

#endif
