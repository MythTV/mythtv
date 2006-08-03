#include <unistd.h>
#include <iostream>

using namespace std;

// qt
#include <qlayout.h>
#include <qhbox.h>
#include <qfile.h>

// mythtv
#include <mythtv/mythcontext.h>
#include <mythtv/dialogbox.h>
#include <mythtv/mythdialogs.h>
#include <mythtv/mythdbcon.h>

// mytharchive
#include <logviewer.h>

const int DEFAULT_UPDATE_TIME = 5;

LogViewer::LogViewer(MythMainWindow *parent, const char *name)
              : MythDialog(parent, name)
{
    QVBoxLayout *vbox = new QVBoxLayout(this, (int)(15 * wmult));
    QHBoxLayout *hbox = new QHBoxLayout(vbox, (int)(0 * wmult));

    // Window title
    QString message = tr("Log Viewer");
    QLabel *label = new QLabel(message, this);
    QFont font = label->font();
    font.setPointSize(int (font.pointSize() * 1.2));
    font.setBold(true);
    label->setFont(font);
    label->setPaletteForegroundColor(QColor("yellow"));
    label->setBackgroundOrigin(WindowOrigin);
    label->setSizePolicy(QSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed));
    hbox->addWidget(label);

    hbox = new QHBoxLayout(vbox, (int)(10 * wmult));
    autoupdateCheck = new MythCheckBox( this );
    autoupdateCheck->setBackgroundOrigin(WindowOrigin);
    autoupdateCheck->setChecked(true);
    autoupdateCheck->setText("Auto Update Frequency");
    hbox->addWidget(autoupdateCheck);

    updateTimeSpin = new MythSpinBox( this );
    updateTimeSpin->setMinValue(1);
    updateTimeSpin->setValue(DEFAULT_UPDATE_TIME);
    hbox->addWidget(updateTimeSpin);

    message = tr("Seconds");
    label = new QLabel(message, this);
    label->setBackgroundOrigin(WindowOrigin);
    label->setSizePolicy(QSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed));
    hbox->addWidget(label);

    // listbox
    hbox = new QHBoxLayout(vbox, (int)(10 * wmult));

    listbox = new MythListBox( this );
    listbox->setBackgroundOrigin(WindowOrigin);
    listbox->setEnabled(true);
    font = listbox->font();
    font.setPointSize(int (font.pointSize() * 0.7));
    font.setBold(false);
    listbox->setFont(font);
    hbox->addWidget(listbox);


    hbox = new QHBoxLayout(vbox, (int)(10 * wmult));

    //  cancel Button
    hbox = new QHBoxLayout(vbox, (int)(10 * wmult));

    cancelButton = new MythPushButton( this, "cancel" );
    cancelButton->setBackgroundOrigin(WindowOrigin);
    cancelButton->setText( tr( "Cancel" ) );
    cancelButton->setEnabled(true);

    hbox->addWidget(cancelButton);

    //  update Button
    updateButton = new MythPushButton( this, "update" );
    updateButton->setBackgroundOrigin(WindowOrigin);
    updateButton->setText( tr( "Update" ) );
    updateButton->setEnabled(true);
    updateButton->setFocus();

    hbox->addWidget(updateButton);

    // exit button
    exitButton = new MythPushButton( this, "exit" );
    exitButton->setBackgroundOrigin(WindowOrigin);
    exitButton->setText( tr( "Exit" ) );
    exitButton->setEnabled(true);

    hbox->addWidget(exitButton);

    connect(exitButton, SIGNAL(clicked()), this, SLOT(exitClicked()));
    connect(cancelButton, SIGNAL(clicked()), this, SLOT(cancelClicked()));
    connect(updateButton, SIGNAL(clicked()), this, SLOT(updateClicked()));
    connect(autoupdateCheck, SIGNAL(toggled(bool)), this, SLOT(toggleAutoUpdate(bool)));
    connect(updateTimeSpin, SIGNAL(valueChanged(int)), 
            this, SLOT(updateTimeChanged(int)));

    updateTimer = NULL;
    updateTimer = new QTimer(this);
    connect(updateTimer, SIGNAL(timeout()), SLOT(updateTimerTimeout()) );
    updateTimer->start((int)(DEFAULT_UPDATE_TIME * 1000));
}

LogViewer::~LogViewer(void)
{
    if (updateTimer)
        delete updateTimer;
}

void LogViewer::updateTimerTimeout()
{
    updateClicked();
}

void LogViewer::toggleAutoUpdate(bool checked)
{
    if (checked)
    {
        updateTimeSpin->setEnabled(true);
        updateTimer->start(updateTimeSpin->value() * 1000);
    }
    else
    {
        updateTimeSpin->setEnabled(false);
        updateTimer->stop();
    }
}

void LogViewer::updateTimeChanged(int value)
{
    updateTimer->stop();
    updateTimer->changeInterval(value * 1000);
}

void LogViewer::exitClicked(void)
{
    done(-1);
}

void LogViewer::cancelClicked(void)
{
    QString tempDir = gContext->GetSetting("MythArchiveTempDir", "");

    system("echo Cancel > " + tempDir + "/logs/mythburncancel.lck" );

    MythPopupBox::showOkPopup(gContext->GetMainWindow(), 
        QObject::tr("Myth Burn"),
        QObject::tr("Background creation has been asked to stop.\n" 
                                "This may take a few minutes."));
}

void LogViewer::updateClicked(void)
{
    QStringList list;
    loadFile(m_filename, list, listbox->count());

    if (list.count() > 0)
    {
        bool bUpdateCurrent = listbox->currentItem() == (int) listbox->count() - 1;
        listbox->insertStringList(list);
        if (bUpdateCurrent)
            listbox->setCurrentItem(listbox->count() - 1);
    }
}

bool LogViewer::loadFile(QString filename, QStringList &list, int startline)
{
    list.clear();

    QFile file(filename);

    if (!file.exists())
        return false;

    if (file.open( IO_ReadOnly ))
    {
        QString s;
        QTextStream stream(&file);

         // ignore the first startline lines
        while ( !stream.atEnd() && startline > 0)
        {
            stream.readLine();
            startline--;
        }

         // read rest of file
        while ( !stream.atEnd() )
        {
            s = stream.readLine();
            list.append(s);
        }
        file.close();
    }
    else
        return false;

    return true;
}
