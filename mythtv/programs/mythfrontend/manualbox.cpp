#include <qlayout.h>
#include <qpushbutton.h>
#include <qbuttongroup.h>
#include <qlabel.h>
#include <qcursor.h>
#include <qlistview.h>
#include <qdatetime.h>
#include <qprogressbar.h>
#include <qapplication.h>
#include <qtimer.h>
#include <qimage.h>
#include <qpainter.h>
#include <qheader.h>
#include <qfile.h>
#include <qsqldatabase.h>
#include <qregexp.h>
#include <qhbox.h>

#include <unistd.h>

#include <iostream>
using namespace std;

#include "manualbox.h"
#include "tv.h"
#include "programlistitem.h"
#include "NuppelVideoPlayer.h"
#include "yuv2rgb.h"

#include "libmyth/mythcontext.h"
#include "libmyth/dialogbox.h"
#include "libmythtv/programinfo.h"
#include "libmythtv/remoteutil.h"

struct QtToXKeyMap
{
    int     xKey;
    int     qtKey;
};

// cheat and put the keycodes here
#define wsUp            0x52 + 256
#define wsDown          0x54 + 256
#define wsLeft          0x51 + 256
#define wsRight         0x53 + 256
#define wsPageUp        0x55 + 256
#define wsPageDown      0x56 + 256
#define wsEnd           0x57 + 256
#define wsBegin         0x58 + 256
#define wsEscape        0x1b + 256
#define wsZero          0xb0 + 256
#define wsOne           0xb1 + 256
#define wsTwo           0xb2 + 256
#define wsThree         0xb3 + 256
#define wsFour          0xb4 + 256
#define wsFive          0xb5 + 256
#define wsSix           0xb6 + 256
#define wsSeven         0xb7 + 256
#define wsEight         0xb8 + 256
#define wsNine          0xb9 + 256
#define wsEnter         0x8d + 256
#define wsReturn        0x0d + 256

QtToXKeyMap gKeyMap[] =
{
    {wsUp, Qt::Key_Up}, {wsDown, Qt::Key_Down}, {wsLeft, Qt::Key_Left}, 
    {wsRight, Qt::Key_Right}, {wsEnd, Qt::Key_End}, {wsBegin, Qt::Key_Home}, 
    {wsEscape, Qt::Key_Escape}, {wsEnter, Qt::Key_Enter}, {wsReturn, Qt::Key_Return},
};

ManualBox::ManualBox(QWidget *parent, const char *name)
           : MythDialog(parent, name)
{
    m_tv = NULL;

    QVBoxLayout *vbox = new QVBoxLayout(this, (int)(15 * wmult));

    // Window title
    QString message = "Manual Recording";
    QLabel *label = new QLabel(message, this);
    label->setBackgroundOrigin(WindowOrigin);
    label->setSizePolicy(QSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed));
    vbox->addWidget(label);


    // Video
    m_boxframe = new QHBox(this);
    m_boxframe->setFrameStyle(QFrame::Panel | QFrame::Sunken);
    m_boxframe->setLineWidth(2);
    m_boxframe->setBackgroundOrigin(WindowOrigin);
    m_boxframe->setFocusPolicy(QWidget::StrongFocus);
    m_boxframe->setSizePolicy(QSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed));
    m_boxframe->setMargin((int)(5 * wmult));
    m_boxframe->installEventFilter(this);

    QPixmap temp((int)(320 * wmult), (int)(240 * hmult));
    temp.fill(black);

    m_pixlabel = new QLabel(m_boxframe);
    m_pixlabel->setBackgroundOrigin(WindowOrigin);
    m_pixlabel->setPixmap(temp);
    m_pixlabel->setSizePolicy(QSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed));

    QHBoxLayout *hbox = new QHBoxLayout(vbox, (int)(10 * wmult));
    hbox->addWidget(m_boxframe);

    //hbox->addWidget(m_pixlabel);

    
    // Title edit box
    hbox = new QHBoxLayout(vbox, (int)(10 * wmult));

    message = "Title:";
    label = new QLabel(message, this);
    label->setBackgroundOrigin(WindowOrigin);
    label->setSizePolicy(QSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed));
    hbox->addWidget(label);

    m_title = new MythLineEdit( this, "title" );
    m_title->setBackgroundOrigin(WindowOrigin);
    hbox->addWidget(m_title);

    
    // Subtitle edit box
    hbox = new QHBoxLayout(vbox, (int)(10 * wmult));

    message = "Subtitle:";
    label = new QLabel(message, this);
    label->setBackgroundOrigin(WindowOrigin);
    label->setSizePolicy(QSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed));
    hbox->addWidget(label);

    m_subtitle = new MythLineEdit( this, "subtitle" );
    m_subtitle->setBackgroundOrigin(WindowOrigin);
    hbox->addWidget(m_subtitle);


    // Duration spin box
    hbox = new QHBoxLayout(vbox, (int)(10 * wmult));

    message = "Duration:";
    label = new QLabel(message, this);
    label->setBackgroundOrigin(WindowOrigin);
    label->setSizePolicy(QSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed));
    hbox->addWidget(label);

    m_duration = new MythSpinBox( this, "duration" );
    m_duration->setMinValue(1);
    m_duration->setMaxValue(300);
    m_duration->setValue(60);
    m_duration->setBackgroundOrigin(WindowOrigin);
    hbox->addWidget(m_duration);

    message = "minutes";
    label = new QLabel(message, this);
    label->setBackgroundOrigin(WindowOrigin);
    hbox->addWidget(label);


    // Start and stop buttons
    hbox = new QHBoxLayout(vbox, (int)(10 * wmult));

    m_startButton = new MythPushButton( this, "start" );
    m_startButton->setBackgroundOrigin(WindowOrigin);
    m_startButton->setText( tr( "Start" ) );
    m_startButton->setEnabled(false);

    m_stopButton = new MythPushButton( this, "stop" );
    m_stopButton->setBackgroundOrigin(WindowOrigin);
    m_stopButton->setText( tr( "Stop" ) );

    hbox->addWidget(m_startButton);
    hbox->addWidget(m_stopButton);


    m_timer = new QTimer(this);
    m_refreshTimer = new QTimer(this);

    showFullScreen();
    setActiveWindow();

    connect(this, SIGNAL(dismissWindow()), this, SLOT(accept()));

    connect(m_title, SIGNAL(textChanged(const QString &)), this, SLOT(textChanged(const QString &)));
    connect(m_startButton, SIGNAL(clicked()), this, SLOT(startClicked()));
    connect(m_stopButton, SIGNAL(clicked()), this, SLOT(stopClicked()));
    connect(m_timer, SIGNAL(timeout()), this, SLOT(timeout()));
    connect(m_refreshTimer, SIGNAL(timeout()), this, SLOT(refreshTimeout()));
    m_timer->start(300);

    gContext->addListener(this);
}

ManualBox::~ManualBox(void)
{
    gContext->removeListener(this);
    killPlayer();
}

bool ManualBox::eventFilter(QObject *, QEvent *e)
{
    if (e->type() == QEvent::FocusIn)
    {
        m_boxframe->setFrameStyle(QFrame::Panel | QFrame::Raised);
    }
    else if (e->type() == QEvent::FocusOut)
    {
        m_boxframe->setFrameStyle(QFrame::Panel | QFrame::Sunken);
    }
    else if (e->type() == QEvent::KeyPress)
    {
        if (NULL != m_tv)
        {
            QKeyEvent* k = (QKeyEvent*)e;
            int key = k->ascii();
            unsigned int index;

            if (0 == key)
            {
                index = 0;

                while (index < sizeof(gKeyMap) / sizeof(gKeyMap[0]))
                {
                    if (gKeyMap[index].qtKey == k->key())
                    {
                        key = gKeyMap[index].xKey;
                        break;
                    }
                    index++;
                }
            }

            int validKeys[] = {
                wsUp, wsDown,
                'c', 'C',
                'i', 'I',
                '/', '?',
                '0', '1', '2', '3', '4', '5', '6', '7', '8', '9',
                wsEnter, wsReturn, ' ',
                'j', 'J', 'k', 'K', 'l', 'L',
            };
            
            index = 0;
            while (index < sizeof(validKeys) / sizeof(validKeys[0]))
            {
                if (key == validKeys[index])
                {
                    qApp->unlock();
                    m_tv->ProcessKeypress(key);
                    qApp->lock();
                    break;
                }
                index++;
            }
        }
    }

    return false;
}

void ManualBox::killPlayer(void)
{
    m_timer->stop();
    while (m_timer->isActive())
        usleep(50);

    if (NULL != m_tv)
    {
        m_tv->StopEmbeddingOutput();
        qApp->unlock();
        delete m_tv;
        qApp->lock();
        m_tv = NULL;
    }
}

void ManualBox::startPlayer(void)
{

    if (NULL == m_tv)
    {
        TVState nextstate;

        QSqlDatabase *db = QSqlDatabase::database();
        m_tv = new TV(db);
        m_tv->Init();
        m_tv->EmbedOutput(m_pixlabel->winId(), 0, 0, (int)(320 * wmult), (int)(240 * hmult));
        nextstate = m_tv->LiveTV();
        
        if (nextstate == kState_WatchingLiveTV ||
            nextstate == kState_WatchingRecording)
        {
            qApp->unlock();
            while (m_tv->ChangingState())
            {
                usleep(50);
                qApp->processEvents();
            }
            qApp->lock();
            setActiveWindow();
            m_wasRecording = !m_tv->IsRecording();
        }
        else
        {
            qApp->unlock();
            delete m_tv;
            qApp->lock();
            m_tv = NULL;
            emit dismissWindow();
        }
    }
}

void ManualBox::timeout(void)
{
    startPlayer();
    if (NULL != m_tv)
    {
        if (m_wasRecording != m_tv->IsRecording())
        {
            if (m_tv->IsRecording())
            {
                m_startButton->setEnabled(false);
                m_stopButton->setEnabled(true);
                m_title->setEnabled(false);
                m_subtitle->setEnabled(false);
                m_duration->setEnabled(false);
                m_stopButton->setFocus();
                m_refreshTimer->stop();
                m_wasRecording = true;
            }
            else
            {
                m_stopButton->setEnabled(false);
                m_title->setEnabled(true);
                m_subtitle->setEnabled(true);
                m_duration->setEnabled(true);
                m_startButton->setEnabled(!m_title->text().isEmpty());
                m_title->setFocus();
                m_refreshTimer->start(2000);
                m_wasRecording = false;
            }
        }
    }
}

void ManualBox::refreshTimeout(void)
{
    if (NULL != m_tv)
    {
        QString dummy;
        QString title;
        QString subtitle;

        m_tv->GetChannelInfo(NULL, title, subtitle, m_descString,
                             m_categoryString, m_startString, dummy,
                             dummy, dummy, dummy,
                             m_chanidString);

        if (m_lastStarttime != m_startString || m_lastChanid != m_chanidString)
        {
            // what we're watching has changed
            m_title->setText(title);
            m_subtitle->setText(subtitle);
            m_startButton->setEnabled(!m_title->text().isEmpty());
            m_lastStarttime = m_startString;
            m_lastChanid = m_chanidString;
        }
    }
}

void ManualBox::textChanged(const QString &title)
{
    m_startButton->setEnabled(!title.isEmpty());
}

void ManualBox::stopClicked(void)
{
    if (NULL != m_tv)
    {
        m_tv->FinishRecording();
    }
}

void ManualBox::startClicked(void)
{
    ProgramInfo progInfo;
    QSqlDatabase *db = QSqlDatabase::database();

    m_startButton->setEnabled(false);

    progInfo.title = m_title->text();
    progInfo.subtitle = m_subtitle->text();
    progInfo.description = m_descString;
    progInfo.category = m_categoryString;
    progInfo.chanid = m_chanidString;

    progInfo.startts = QDateTime::currentDateTime();
    progInfo.endts = QDateTime::currentDateTime().addSecs(m_duration->value() * 60);
    
    progInfo.Save();

    progInfo.ApplyRecordStateChange(db, ScheduledRecording::SingleRecord);
}

