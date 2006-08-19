/*
    editmetadata.cpp

    (c) 2003 Thor Sigvaldason, Isaac Richards, and ?? ??
    Part of the mythTV project


*/

#include <mythtv/mythcontext.h>
#include <mythtv/uitypes.h>

#include "globals.h"
#include "editmetadata.h"
#include "metadata.h"
#include "dbaccess.h"
#include "metadatalistmanager.h"
#include "videoutils.h"

EditMetadataDialog::EditMetadataDialog(Metadata *source_metadata,
                                       const MetadataListManager &cache,
                                       MythMainWindow *parent_,
                                       const QString &window_name,
                                       const QString &theme_filename,
                                       const char *name_)
    : MythThemedDialog(parent_, window_name, theme_filename, name_),
    m_orig_metadata(source_metadata), m_meta_cache(cache)
{
    //
    //  The only thing this screen does is let the
    //  user set (some) metadata information. It only
    //  works on a single metadata entry.
    //


    //
    //  Make a copy, so we can abandon changes if desired
    //

    working_metadata = new Metadata(*m_orig_metadata);

    title_hack = NULL;
    player_hack = NULL;
    category_select = NULL;
    level_select = NULL;
    child_check = NULL;
    child_select = NULL;
    browse_check = NULL;
    coverart_button = NULL;
    coverart_text = NULL;
    done_button = NULL;
    title_editor = NULL;
    player_editor = NULL;

    wireUpTheme();
    fillWidgets();
    assignFirstFocus();
}

namespace
{
    template <typename T>
    struct title_sort
    {
        bool operator()(const T &lhs, const T &rhs)
        {
            return QString::localeAwareCompare(lhs.second, rhs.second) < 0;
        }
    };
}

void EditMetadataDialog::fillWidgets()
{
    if (title_editor) title_editor->setText(working_metadata->Title());

    if (category_select)
    {
        category_select->addItem(0, VIDEO_CATEGORY_UNKNOWN);
        const VideoCategory::entry_list &vcl =
                VideoCategory::getCategory().getList();
        for (VideoCategory::entry_list::const_iterator p = vcl.begin();
             p != vcl.end(); ++p)
        {
            category_select->addItem(p->first, p->second);
        }
        category_select->setToItem(working_metadata->getCategoryID());
    }

    if (level_select)
    {
        for (int i = 1; i < 5; i++)
        {
            level_select->addItem(i, QString(tr("Level %1")).arg(i));
        }
        level_select->setToItem(working_metadata->ShowLevel());
    }

    if (child_select)
    {
        //
        //  Fill the "always play this video next" option
        //  with all available videos.
        //

        bool trip_catch = false;
        QString caught_name = "";
        int possible_starting_point = 0;
        child_select->addItem(0, tr("None"));

        // TODO: maybe make the title list have the same sort order
        // as elsewhere.
        typedef std::vector<std::pair<unsigned int, QString> > title_list;
        const MetadataListManager::metadata_list &mdl = m_meta_cache.getList();
        title_list tc;
        tc.reserve(mdl.size());
        for (MetadataListManager::metadata_list::const_iterator p = mdl.begin();
             p != mdl.end(); ++p)
        {
            tc.push_back(std::make_pair((*p)->ID(), (*p)->Title()));
        }
        std::sort(tc.begin(), tc.end(), title_sort<title_list::value_type>());

        for (title_list::const_iterator p = tc.begin(); p != tc.end(); ++p)
        {
            if (trip_catch)
            {
                //
                //  Previous loop told us to check if the two
                //  movie names are close enough to try and
                //  set a default starting point.
                //

                QString target_name = p->second;
                int length_compare = 0;
                if (target_name.length() < caught_name.length())
                {
                    length_compare = target_name.length();
                }
                else
                {
                    length_compare = caught_name.length();
                }

                QString caught_name_three_quarters =
                        caught_name.left((int)(length_compare * 0.75));
                QString target_name_three_quarters =
                        target_name.left((int)(length_compare * 0.75));

                if (caught_name_three_quarters == target_name_three_quarters &&
                    working_metadata->ChildID() == -1)
                {
                    possible_starting_point = p->first;
                    working_metadata->setChildID(possible_starting_point);
                }
                trip_catch = false;
            }

            if (p->first != working_metadata->ID())
            {
                child_select->addItem(p->first, p->second);
            }
            else
            {
                //
                //  This is the current file. Set a flag so the default
                //  selected child will be set next loop
                //

                trip_catch = true;
                caught_name = p->second;
            }
        }

        if (working_metadata->ChildID() > 0)
        {
            child_select->setToItem(working_metadata->ChildID());
            cachedChildSelection = working_metadata->ChildID();
        }
        else
        {
            child_select->setToItem(possible_starting_point);
            cachedChildSelection = possible_starting_point;
        }
    }

    if (child_select && child_check)
    {
        child_check->setState(cachedChildSelection > 0);
        child_select->allowFocus(cachedChildSelection > 0);
    }

    if (browse_check) browse_check->setState(working_metadata->Browse());
    checkedSetText(coverart_text, working_metadata->CoverFile());
    if (player_editor) player_editor->setText(working_metadata->PlayCommand());
}

void EditMetadataDialog::toggleChild(bool yes_or_no)
{
    if (child_select)
    {
        if (yes_or_no)
        {
            child_select->setToItem(cachedChildSelection);
            working_metadata->setChildID(cachedChildSelection);
        }
        else
        {
            child_select->setToItem(0);
            working_metadata->setChildID(0);
        }
        child_select->allowFocus(yes_or_no);
    }
}

void EditMetadataDialog::keyPressEvent(QKeyEvent *e)
{
    bool handled = false;
    bool something_pushed = false;

    QStringList actions;
    gContext->GetMainWindow()->TranslateKeyPress("Video", e, actions);

    for (unsigned int i = 0; i < actions.size() && !handled; i++)
    {
        QString action = actions[i];
        handled = true;

        if (action == "UP")
            nextPrevWidgetFocus(false);
        else if (action == "DOWN")
            nextPrevWidgetFocus(true);
        else if (action == "LEFT")
        {
            something_pushed = false;
            if (category_select)
            {
                if (getCurrentFocusWidget() == category_select)
                {
                    category_select->push(false);
                    something_pushed = true;
                }
            }
            if (level_select)
            {
                if (getCurrentFocusWidget() == level_select)
                {
                    level_select->push(false);
                    something_pushed = true;
                }
            }
            if (child_select)
            {
                if (getCurrentFocusWidget() == child_select)
                {
                    child_select->push(false);
                    something_pushed = true;
                }
            }
            if (!something_pushed)
            {
                activateCurrent();
            }
        }
        else if (action == "RIGHT")
        {
            something_pushed = false;
            if (category_select)
            {
                if (getCurrentFocusWidget() == category_select)
                {
                    category_select->push(true);
                    something_pushed = true;
                }
            }
            if (level_select)
            {
                if (getCurrentFocusWidget() == level_select)
                {
                    level_select->push(true);
                    something_pushed = true;
                }
            }
            if (child_select)
            {
                if (getCurrentFocusWidget() == child_select)
                {
                    child_select->push(true);
                    something_pushed = true;
                }
            }
            if (!something_pushed)
            {
                activateCurrent();
            }
        }
        else if (action == "SELECT")
        {
            something_pushed = false;
            if (category_select)
            {
                if (getCurrentFocusWidget() == category_select)
                {
                    QString category = QString("");
                    if (MythPopupBox::showGetTextPopup(
                                gContext->GetMainWindow(),
                                "Enter category",
                                QObject::tr("New category"),
                                category))
                    {

                        int id = VideoCategory::getCategory().add(category);
                        working_metadata->setCategoryID(id);
                        category_select->addItem(id, category);
                        category_select->setToItem(id);
                    }
                    something_pushed = true;
                }
            }

            if (!something_pushed)
            {
                activateCurrent();
            }
        }
        else if (action == "0")
        {
            if (done_button)
                done_button->push();
        }
        else
            handled = false;
    }

    if (!handled)
        MythThemedDialog::keyPressEvent(e);
}


void EditMetadataDialog::takeFocusAwayFromEditor(bool up_or_down)
{
    nextPrevWidgetFocus(up_or_down);

    MythRemoteLineEdit *which_editor = (MythRemoteLineEdit *)sender();

    if (which_editor)
    {
        which_editor->clearFocus();
    }
}

void EditMetadataDialog::saveAndExit()
{
    //
    //  Persist to database
    //

    *m_orig_metadata = *working_metadata;
    m_orig_metadata->updateDatabase();

    //
    //  All done
    //

    done(0);
}

void EditMetadataDialog::setTitle(QString new_title)
{
    working_metadata->setTitle(new_title);
}

void EditMetadataDialog::setCategory(int new_category)
{
    working_metadata->setCategoryID(new_category);
}

void EditMetadataDialog::setPlayer(QString new_command)
{
    working_metadata->setPlayCommand(new_command);
}

void EditMetadataDialog::setLevel(int new_level)
{
    working_metadata->setShowLevel(new_level);
}

void EditMetadataDialog::setChild(int new_child)
{
    working_metadata->setChildID(new_child);
    if (child_check)
    {
        child_check->setState(new_child > 0);
        cachedChildSelection = new_child;
    }
}

void EditMetadataDialog::toggleBrowse(bool yes_or_no)
{
    working_metadata->setBrowse(yes_or_no);
}

void EditMetadataDialog::findCoverArt()
{
    QString new_coverart_file;
    if (!isDefaultCoverFile(working_metadata->CoverFile()))
    {
        new_coverart_file = working_metadata->CoverFile();
    }

    QString fileprefix = gContext->GetSetting("VideoArtworkDir");
    // If the video artwork setting hasn't been set default to
    // using ~/.mythtv/MythVideo
    if (fileprefix.length() == 0)
    {
        fileprefix = MythContext::GetConfDir() + "/MythVideo";
    }

    MythImageFileDialog *nca =
        new MythImageFileDialog(&new_coverart_file,
                                fileprefix,
                                gContext->GetMainWindow(),
                                "file_chooser",
                                "video-",
                                "image file chooser",
                                true);
    nca->exec();
    if (new_coverart_file.length() > 0)
    {
        working_metadata->setCoverFile(new_coverart_file);
        checkedSetText(coverart_text, new_coverart_file);
    }

    delete nca;
}

void EditMetadataDialog::wireUpTheme()
{
    title_hack = getUIBlackHoleType("title_hack");
    if (title_hack)
    {
        title_hack->allowFocus(true);
        QFont f = gContext->GetMediumFont();
        title_editor = new MythRemoteLineEdit(&f, this);
        title_editor->setFocusPolicy(QWidget::NoFocus);
        title_editor->setGeometry(title_hack->getScreenArea());
        connect(title_hack, SIGNAL(takingFocus()),
                title_editor, SLOT(setFocus()));
        connect(title_editor, SIGNAL(tryingToLooseFocus(bool)),
                this, SLOT(takeFocusAwayFromEditor(bool)));
        connect(title_editor, SIGNAL(textChanged(QString)),
                this, SLOT(setTitle(QString)));
    }

    category_select = getUISelectorType("category_select");
    if (category_select)
    {
        connect(category_select, SIGNAL(pushed(int)),
                this, SLOT(setCategory(int)));
    }

    player_hack = getUIBlackHoleType("player_hack");
    if (player_hack)
    {
        player_hack->allowFocus(true);
        QFont f = gContext->GetMediumFont();
        player_editor = new MythRemoteLineEdit(&f, this);
        player_editor->setFocusPolicy(QWidget::NoFocus);
        player_editor->setGeometry(player_hack->getScreenArea());
        connect(player_hack, SIGNAL(takingFocus()),
                player_editor, SLOT(setFocus()));
        connect(player_editor, SIGNAL(tryingToLooseFocus(bool)),
                this, SLOT(takeFocusAwayFromEditor(bool)));
        connect(player_editor, SIGNAL(textChanged(QString)),
                this, SLOT(setPlayer(QString)));
    }

    level_select = getUISelectorType("level_select");
    if (level_select)
    {
        connect(level_select, SIGNAL(pushed(int)),
                this, SLOT(setLevel(int)));
    }

    child_check = getUICheckBoxType("child_check");
    if (child_check)
    {
        connect(child_check, SIGNAL(pushed(bool)),
                this, SLOT(toggleChild(bool)));
    }

    child_select = getUISelectorType("child_select");
    if (child_select)
    {
        connect(child_select, SIGNAL(pushed(int)),
                this, SLOT(setChild(int)));
    }

    browse_check = getUICheckBoxType("browse_check");
    if (browse_check)
    {
        connect(browse_check, SIGNAL(pushed(bool)),
                this, SLOT(toggleBrowse(bool)));
    }

    coverart_button = getUIPushButtonType("coverart_button");
    if (coverart_button)
    {
        connect(coverart_button, SIGNAL(pushed()),
                this, SLOT(findCoverArt()));
    }
    coverart_text = getUITextType("coverart_text");

    done_button = getUITextButtonType("done_button");
    if (done_button)
    {
        done_button->setText(tr("Done"));
        connect(done_button, SIGNAL(pushed()), this, SLOT(saveAndExit()));
    }

    buildFocusList();
}


EditMetadataDialog::~EditMetadataDialog()
{
    if (title_editor)
    {
        delete title_editor;
    }
    if (player_editor)
    {
        delete player_editor;
    }
    if (working_metadata)
    {
        delete working_metadata;
    }
}
