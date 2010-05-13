#include <QRegExp>

#include <QTcpSocket>

#include <util.h>
#include <mythcontext.h>
#include <mythmediamonitor.h>

#include <mythuitext.h>
#include <mythuitextedit.h>
#include <mythuibutton.h>
#include <mythuibuttonlist.h>
#include <mythuicheckbox.h>

#include "videoutils.h"
#include "titledialog.h"

TitleDialog::TitleDialog(MythScreenStack *lparent, QString lname,
        QTcpSocket *a_socket, QString d_name, QList<DVDTitleInfo*> *titles) :
    MythScreenType(lparent, lname), m_discName(d_name), m_dvdTitles(titles),
    m_currentTitle(0), m_socketToMtd(a_socket), m_nameEdit(0),
    m_audioList(0), m_qualityList(0), m_subtitleList(0),
    m_ripCheck(0), m_ripacthreeCheck(0), m_playlengthText(0),
    m_numbtitlesText(0), m_viewButton(0), m_nexttitleButton(0),
    m_prevtitleButton(0), m_ripawayButton(0)
{
    if(m_discName.length() < 1)
        m_discName = tr("Unknown");

    //
    //  Find the longest title and set some defaults
    //

    uint longest = 0;
    uint longest_time = 0;

    for(int i = 0; i < m_dvdTitles->size(); i++)
    {
        if(m_dvdTitles->at(i)->getPlayLength() >= longest_time)
        {
            longest = i;
            longest_time = m_dvdTitles->at(i)->getPlayLength();
            m_currentTitle = m_dvdTitles->at(i);
        }
    }

    for(int i = 0; i < m_dvdTitles->size(); i++)
    {
        if(m_dvdTitles->at(i) == m_currentTitle)
        {
            m_dvdTitles->at(i)->setName(m_discName);
            m_dvdTitles->at(i)->setSelected(true);
        }
        else
        {
            m_dvdTitles->at(i)->setName(QString(tr("%1 - Title %2"))
                    .arg(m_discName).arg(i + 1));
        }
    }
}

bool TitleDialog::Create()
{
    if (!LoadWindowFromXML("dvd-ui.xml", "title_dialog", this))
        return false;

    bool err = false;
    UIUtilE::Assign(this, m_nameEdit, "name", &err);

    UIUtilE::Assign(this, m_playlengthText, "playlength", &err);
    UIUtilE::Assign(this, m_numbtitlesText, "numb_titles", &err);

    UIUtilE::Assign(this, m_ripCheck, "ripcheck", &err);
    UIUtilE::Assign(this, m_ripacthreeCheck, "ripacthree", &err);

    UIUtilE::Assign(this, m_nexttitleButton, "next_title", &err);
    UIUtilE::Assign(this, m_prevtitleButton, "prev_title", &err);
    UIUtilE::Assign(this, m_viewButton, "view", &err);
    UIUtilE::Assign(this, m_ripawayButton, "ripaway", &err);

    UIUtilE::Assign(this, m_audioList, "audio", &err);
    UIUtilE::Assign(this, m_qualityList, "quality", &err);
    UIUtilE::Assign(this, m_subtitleList, "subtitle", &err);

    if (err)
    {
        VERBOSE(VB_IMPORTANT, "Cannot load screen 'title_dialog'");
        return false;
    }

    if (m_dvdTitles->size() > 1)
    {
        m_nexttitleButton->SetVisible(true);
        m_prevtitleButton->SetVisible(true);
    }
    else
    {
        m_nexttitleButton->SetVisible(false);
        m_prevtitleButton->SetVisible(false);
    }

    m_ripawayButton->SetText(tr("Begin Ripping"));

    BuildFocusList();

    showCurrentTitle();

    connect(m_nameEdit, SIGNAL(valueChanged()), SLOT(changeName()));

    connect(m_audioList, SIGNAL(itemSelected(MythUIButtonListItem*)),
            SLOT(setAudio(MythUIButtonListItem*)));
    connect(m_qualityList, SIGNAL(itemSelected(MythUIButtonListItem*)),
            SLOT(setQuality(MythUIButtonListItem*)));
    connect(m_subtitleList, SIGNAL(itemSelected(MythUIButtonListItem*)),
            SLOT(setSubTitle(MythUIButtonListItem*)));

    connect(m_ripacthreeCheck, SIGNAL(valueChanged()), SLOT(toggleAC3()));
    connect(m_ripCheck, SIGNAL(valueChanged()), SLOT(toggleTitle()));

    connect(m_ripawayButton, SIGNAL(Clicked()), SLOT(ripTitles()));
    connect(m_viewButton, SIGNAL(Clicked()), SLOT(viewTitle()));
    connect(m_nexttitleButton, SIGNAL(Clicked()), SLOT(nextTitle()));
    connect(m_prevtitleButton, SIGNAL(Clicked()), SLOT(prevTitle()));

    return true;
}

void TitleDialog::showCurrentTitle()
{
    //
    //  Set the widget to display correct info
    //
    if(m_currentTitle)
    {
        //
        //  We have to if() check everything, in case the
        //  theme designer left things out.
        //

        m_playlengthText->SetText(m_currentTitle->getTimeString());

        if (m_currentTitle->getSelected())
            m_ripCheck->SetCheckState(MythUIStateType::Full);
        else
            m_ripCheck->SetCheckState(MythUIStateType::Off);

        m_nameEdit->SetText(m_currentTitle->getName());

        m_audioList->Reset();
        QList<DVDAudioInfo*> *audio_tracks = m_currentTitle->getAudioTracks();
        for(int j = 0; j < audio_tracks->size(); j++)
        {
            new MythUIButtonListItem(m_audioList,
                                audio_tracks->at(j)->getAudioString(), j + 1);
        }
        m_audioList->SetValueByData(m_currentTitle->getAudio());

        m_qualityList->Reset();
        new MythUIButtonListItem(m_qualityList, tr("ISO Image"),                                                 qVariantFromValue(-1));
        new MythUIButtonListItem(m_qualityList, tr("Perfect"),
                                    qVariantFromValue(0));
        QString q_string = QString("SELECT name,intid FROM dvdtranscode "
                                    "WHERE input = %1 ;")
                                    .arg(m_currentTitle->getInputID());

        MSqlQuery a_query(MSqlQuery::InitCon());

        if(a_query.exec(q_string))
        {
            while(a_query.next())
            {
                new MythUIButtonListItem(m_qualityList,
                    a_query.value(0).toString(), a_query.value(1).toInt());
            }
        }
        m_qualityList->SetValueByData(m_currentTitle->getQuality());

        m_subtitleList->Reset();
        new MythUIButtonListItem(m_subtitleList, tr("None"), -1);
        QList<DVDSubTitleInfo*> *subtitles = m_currentTitle->getSubTitles();
        for(int j = 0; j < subtitles->size(); ++j)
        {
            new MythUIButtonListItem(m_subtitleList, subtitles->at(j)->getName(),
                                   subtitles->at(j)->getID());
        }
        m_subtitleList->SetValueByData(m_currentTitle->getSubTitle());

        if (m_currentTitle->getAC3())
            m_ripacthreeCheck->SetCheckState(MythUIStateType::Full);
        else
            m_ripacthreeCheck->SetCheckState(MythUIStateType::Off);

        m_numbtitlesText->SetText(QString(tr("Title %1 of %2")).arg(m_currentTitle->getTrack()).arg(m_dvdTitles->size()));

    }
}

void TitleDialog::nextTitle()
{
    int index = m_dvdTitles->indexOf(m_currentTitle);
    if (index+1 < m_dvdTitles->size())
        m_currentTitle = m_dvdTitles->at(index+1);
    else
        m_currentTitle = m_dvdTitles->first();

    showCurrentTitle();
}

void TitleDialog::prevTitle()
{
    int index = m_dvdTitles->indexOf(m_currentTitle);
    if(index > 0)
        m_currentTitle = m_dvdTitles->at(index-1);
    else
        m_currentTitle = m_dvdTitles->last();

    showCurrentTitle();
}

void TitleDialog::gotoTitle(uint title_number)
{
    for(int i = 0; i < m_dvdTitles->size(); i++)
    {
        if(m_dvdTitles->at(i)->getTrack() == title_number)
        {
            m_currentTitle = m_dvdTitles->at(i);
            i = m_dvdTitles->size() + 1;
            showCurrentTitle();
        }
    }
}

void TitleDialog::setQuality(MythUIButtonListItem *item)
{
    m_currentTitle->setQuality(item->GetData().toInt());
}

void TitleDialog::setSubTitle(MythUIButtonListItem *item)
{
    m_currentTitle->setSubTitle(item->GetData().toInt());
}

void TitleDialog::setAudio(MythUIButtonListItem *item)
{
    m_currentTitle->setAudio(item->GetData().toInt());
}

void TitleDialog::toggleTitle()
{
    m_currentTitle->setSelected(m_ripCheck->GetBooleanCheckState());

    m_currentTitle->setAC3(m_ripacthreeCheck->GetBooleanCheckState());

    //
    //  Should we be showing the Process Title(s)
    //  Button ?
    //

    int numb_selected = 0;
    for(int i = 0; i < m_dvdTitles->size(); i++)
    {
        if(m_dvdTitles->at(i)->getSelected())
        {
            ++numb_selected;
        }
    }
    if(!m_ripawayButton)
    {
        return;
    }
    if(numb_selected == 0)
    {
        m_ripawayButton->SetVisible(false);
        return;
    }
    m_ripawayButton->SetVisible(true);
}

void TitleDialog::changeName()
{
    m_currentTitle->setName(m_nameEdit->GetText());
}

void TitleDialog::toggleAC3()
{
    m_currentTitle->setAC3(m_ripacthreeCheck->GetBooleanCheckState());
}

void TitleDialog::viewTitle()
{
    QString player_string = gCoreContext->GetSetting("TitlePlayCommand");
    if(player_string.length() < 1)
    {
        VERBOSE(VB_IMPORTANT, "No title player command defined");
        return;
    }

    QString dvd_device = MediaMonitor::defaultDVDdevice();

    int audio_track = 1;
    int channels = 2;
    if(m_currentTitle)
    {
        audio_track = m_currentTitle->getAudio();
        DVDAudioInfo *audio_in_question = m_currentTitle->getAudioTrack(audio_track - 1);
        if(audio_in_question)
        {
            channels = audio_in_question->getChannels();
        }
    }

    if(player_string.contains("mplayer"))
    {
        //
        //  Way to save a few bits mplayer
        //
        audio_track += 127;
    }

    player_string = player_string.replace(QRegExp("%d"), dvd_device);
    player_string = player_string.replace(QRegExp("%t"), QString("%1").arg(m_currentTitle->getTrack()));
    player_string = player_string.replace(QRegExp("%a"), QString("%1").arg(audio_track));
    player_string = player_string.replace(QRegExp("%c"), QString("%1").arg(channels));

    if(m_currentTitle->getSubTitle() > -1)
    {
        QString player_append = gCoreContext->GetSetting("SubTitleCommand");
        if(player_append.length() > 1)
        {
            player_append = player_append.replace(QRegExp("%s"), QString("%1").arg(m_currentTitle->getSubTitle()));
            player_string += " ";
            player_string += player_append;
        }
    }
    // VERBOSE(VB_IMPORTANT, QString("PLAYER STRING: %1").arg(player_string));

    myth_system(player_string);
}

void TitleDialog::ripTitles()
{
    //
    //  OK, the user has selected whatever they want
    //  We just need to build some job strings to
    //  send to the mtd, send them out the socket,
    //  and quit.
    //

    for(int i = 0; i < m_dvdTitles->size(); i++)
    {
        if(m_dvdTitles->at(i)->getSelected())
        {
            //
            //  The spec for this command, which I'm
            //  making up on the fly is:
            //
            //  job
            //  {type}
            //  {title #}
            //  {audio_track}
            //  {quality}
            //  {ac3 flag}
            //  {subtitle #}
            //  {directory to end up in and final name}
            //
            //  note that everything after subtitle #
            //  may include spaces (for subdirs and/or
            //  riptitle that includes spaces)
            //


            //
            //  we need to ask mythvideo stuff where it lives
            //

            QString destination_directory =
                    gCoreContext->GetSetting("mythdvd.LocalRipDirectory");

            if (!destination_directory.length())
            {
                // Assume import directory is first video scan directory
                QStringList videodirs =
                        gCoreContext->GetSetting("VideoStartupDir").split(":",
                                           QString::SkipEmptyParts);
                if (videodirs.size())
                    destination_directory = videodirs[0];
            }

            if(destination_directory.length() < 1)
            {
                VERBOSE(VB_IMPORTANT, "titledialog.o: I can't rip, as I have nowhere to put finished files. MythVideo installed?");
                return;
            }

            QString final_dir_and_file = destination_directory + "/" + m_dvdTitles->at(i)->getName();

            QString job_string = QString("job dvd %1 %2 %3 %4 %5 %6")
                                 .arg(m_dvdTitles->at(i)->getTrack())
                                 .arg(m_dvdTitles->at(i)->getAudio())
                                 .arg(m_dvdTitles->at(i)->getQuality())
                                 .arg(m_dvdTitles->at(i)->getAC3())
                                 .arg(m_dvdTitles->at(i)->getSubTitle())
                                 .arg(final_dir_and_file);

            QTextStream os(m_socketToMtd);
            os << job_string << "\n" ;
        }
    }
    Close();
}
