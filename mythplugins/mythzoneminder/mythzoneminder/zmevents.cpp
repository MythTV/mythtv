/* ============================================================
 * This program is free software; you can redistribute it
 * and/or modify it under the terms of the GNU General
 * Public License as published bythe Free Software Foundation;
 * either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * ============================================================ */

#include <iostream>
#include <cstdlib>

// qt
#include <qdatetime.h>
#include <qpainter.h>
#include <qdir.h>
#include <qapplication.h>
#include <QPixmap>
#include <QLabel>
#include <QKeyEvent>

// myth
#include "mythtv/mythcontext.h"
#include "mythtv/mythdbcon.h"

// zoneminder
#include "zmevents.h"
#include "zmplayer.h"
#include "zmclient.h"

ZMEvents::ZMEvents(MythMainWindow *parent, 
                const QString &window_name, const QString &theme_filename, 
                const char *name)
    :MythThemedDialog(parent, window_name, theme_filename, name)
{
    m_eventListSize = 0;
    m_currentEvent = 0;
    m_eventList = new vector<Event*>;

    wireUpTheme();

    m_oldestFirst = (gContext->GetNumSetting("ZoneMinderOldestFirst", 1) == 1);
    setView(gContext->GetNumSetting("ZoneMinderGridView", 1) == 2);
    setGridLayout(gContext->GetNumSetting("ZoneMinderGridLayout", 6));

    getCameraList();
    getDateList();
    getEventList();
}

ZMEvents::~ZMEvents()
{
    if (!m_eventList)
        delete m_eventList;

    // remember how the user wants to display the event list
    gContext->SaveSetting("ZoneMinderOldestFirst", (m_oldestFirst ? "1" : "0"));
    gContext->SaveSetting("ZoneMinderGridView", getContext());
    gContext->SaveSetting("ZoneMinderGridLayout",  m_eventGrid->getVisibleCount());
}

UITextType* ZMEvents::getTextType(QString name)
{
    UITextType* type = getUITextType(name);

    if (!type)
    {
        VERBOSE(VB_IMPORTANT, "ERROR: Failed to find '" + name + "' UI element in theme file\n" +
                "              Bailing out!");
        exit(0);
    }

    return type;
}

void ZMEvents::keyPressEvent(QKeyEvent *e)
{
    if (!e) return;

    bool handled = false;
    QStringList actions;
    gContext->GetMainWindow()->TranslateKeyPress("TV Frontend", e, actions);

    for (int i = 0; i < actions.size() && !handled; i++)
    {
        QString action = actions[i];
        handled = true;

        if (getCurrentFocusWidget() == m_eventGrid)
        {
            if (action == "ESCAPE")
            {
                nextPrevWidgetFocus(true);
                return;
            }

            if (m_eventGrid->handleKeyPress(action))
                return;
        }

        if (action == "UP")
        {
            if (getCurrentFocusWidget() == m_event_list)
                eventListUp(false);
            else if (getCurrentFocusWidget() == m_cameraSelector)
                m_cameraSelector->push(false);
            else if (getCurrentFocusWidget() == m_dateSelector)
                m_dateSelector->push(false);
            else
                nextPrevWidgetFocus(true);
        }
        else if (action == "DOWN")
        {
            if (getCurrentFocusWidget() == m_event_list)
            {
                eventListDown(false);
            }
            else if (getCurrentFocusWidget() == m_cameraSelector)
                m_cameraSelector->push(true);
            else if (getCurrentFocusWidget() == m_dateSelector)
                m_dateSelector->push(true);
            else
                nextPrevWidgetFocus(true);
        }
        else if (action == "LEFT")
        {
            nextPrevWidgetFocus(false);
        }
        else if (action == "RIGHT")
        {
            nextPrevWidgetFocus(true);
        }
        else if (action == "PAGEUP")
        {
            if (getCurrentFocusWidget() == m_event_list)
            {
                eventListUp(true);
            }
            else
                nextPrevWidgetFocus(true);
        }
        else if (action == "PAGEDOWN")
        {
            if (getCurrentFocusWidget() == m_event_list)
            {
                eventListDown(true);
            }
            else
                nextPrevWidgetFocus(true);
        }
        else if (action == "SELECT")
        {
            if (getCurrentFocusWidget() == m_event_list || 
                getCurrentFocusWidget() == m_eventGrid)
            {
                if (m_playButton)
                    m_playButton->push();
            }
            else
                activateCurrent();
        }
        else if (action == "DELETE")
        {
            if (m_deleteButton)
                m_deleteButton->push();
        }
        else if (action == "INFO")
        {
            m_oldestFirst = !m_oldestFirst;
            getEventList();
        }
        else if (action == "MENU")
            showMenu();
        else if (action == "0")
        {
            if (getContext() == 1)
                setView(true);
            else
                setView(false);
        }
        else if (action == "1")
            setGridLayout(1);
        else if (action == "2")
            setGridLayout(2);
        else if (action == "6")
            setGridLayout(6);
        else
            handled = false;
    }

    if (!handled)
        MythThemedDialog::keyPressEvent(e);
}

void ZMEvents::wireUpTheme()
{
    // event list
    m_event_list = (UIListType*) getUIObject("event_list");
    if (m_event_list)
    {
        m_eventListSize = m_event_list->GetItems();
        m_event_list->SetItemCurrent(0);
    }

    m_eventGrid = getUIImageGridType("event_grid");
    if (m_eventGrid)
    {
        connect(m_eventGrid, SIGNAL(itemChanged(ImageGridItem *)),
                this, SLOT(gridItemChanged(ImageGridItem *)));
    }

    m_eventNoText = getUITextType("eventno_text");

    // play button
    m_playButton = getUITextButtonType("play_button");
    if (m_playButton)
    {
        m_playButton->setText(tr("Play"));
        connect(m_playButton, SIGNAL(pushed()), this, SLOT(playPressed()));
    }

    // delete button
    m_deleteButton = getUITextButtonType("delete_button");
    if (m_deleteButton)
    {
        m_deleteButton->setText(tr("Delete"));
        connect(m_deleteButton, SIGNAL(pushed()), this, SLOT(deletePressed()));
    }

    // cameras selector
    m_cameraSelector = getUISelectorType("camera_selector");
    if (m_cameraSelector)
    {
        connect(m_cameraSelector, SIGNAL(pushed(int)),
                this, SLOT(setCamera(int)));
    }

    // date selector
    m_dateSelector = getUISelectorType("date_selector");
    if (m_dateSelector)
    {
        connect(m_dateSelector, SIGNAL(pushed(int)),
                this, SLOT(setDate(int)));
    }

    buildFocusList();
    assignFirstFocus();
}

void ZMEvents::getEventList(void)
{
    if (class ZMClient *zm = ZMClient::get())
    {
        QString monitorName = "<ANY>";
        QString date = "<ANY>";

        if (m_cameraSelector && m_cameraSelector->getCurrentString() != tr("All Cameras") && 
            m_cameraSelector->getCurrentString() != "")
        {
            monitorName = m_cameraSelector->getCurrentString();
        }

        if (m_dateSelector && m_dateSelector->getCurrentString() != tr("All Dates") && 
            m_dateSelector->getCurrentString() != "")
        {
            date = m_dateList[m_dateSelector->getCurrentInt() - 1];
        }

        zm->getEventList(monitorName, m_oldestFirst, date, m_eventList);

        updateImageGrid();
        updateUIList();
    }
}

void ZMEvents::updateUIList()
{
    if (!m_eventList)
        return;

    QString tmptitle;
    if (m_event_list)
    {
        m_event_list->ResetList();
        if (m_event_list->isFocused())
            m_event_list->SetActive(true);

        int skip;
        if ((int)m_eventList->size() <= m_eventListSize || m_currentEvent <= m_eventListSize / 2)
            skip = 0;
        else if (m_currentEvent >= (int)m_eventList->size() - m_eventListSize + m_eventListSize / 2)
            skip = m_eventList->size() - m_eventListSize;
        else
            skip = m_currentEvent - m_eventListSize / 2;
        m_event_list->SetUpArrow(skip > 0);
        m_event_list->SetDownArrow(skip + m_eventListSize < (int)m_eventList->size());

        int i;
        for (i = 0; i < m_eventListSize; i++)
        {
            if (i + skip >= (int)m_eventList->size())
                break;

            Event *event = m_eventList->at(i + skip);

            m_event_list->SetItemText(i, 1, event->eventName);
            m_event_list->SetItemText(i, 2, event->monitorName);
            m_event_list->SetItemText(i, 3, event->startTime);
            m_event_list->SetItemText(i, 4, event->length);
            if (i + skip == m_currentEvent)
                m_event_list->SetItemCurrent(i);
        }

        m_event_list->refresh();
    }

    if (m_eventNoText)
        if (m_eventList->size() > 0)
            m_eventNoText->SetText(QString("%1/%2")
                    .arg(m_currentEvent + 1).arg(m_eventList->size()));
        else
            m_eventNoText->SetText("0/0");
}

void ZMEvents::eventListDown(bool page)
{
    if (m_currentEvent < (int)m_eventList->size() - 1)
    {
        m_currentEvent += (page ? m_eventListSize : 1);
        if (m_currentEvent > (int)m_eventList->size() - 1)
            m_currentEvent = m_eventList->size() - 1;

        updateUIList();
    }
}

void ZMEvents::eventListUp(bool page)
{
    if (m_currentEvent > 0)
    {
        m_currentEvent -= (page ? m_eventListSize : 1);
        if (m_currentEvent < 0)
            m_currentEvent = 0;

        updateUIList();
    }
}

void ZMEvents::playPressed(void)
{
    if (!m_eventList || m_eventList->size() == 0)
        return;

    Event *event = m_eventList->at(m_currentEvent);
    if (event)
    {
        ZMPlayer *player = new ZMPlayer(m_eventList, &m_currentEvent, 
                gContext->GetMainWindow(), "zmplayer", "zoneminder-", "zmplayer");
        player->exec();
        player->deleteLater();

        if (m_currentEvent > (int)m_eventList->size() - 1)
            m_currentEvent = m_eventList->size() - 1;

        // refresh the image grid
        int currItem = m_currentEvent;
        updateImageGrid();
        m_eventGrid->setCurrentPos(currItem);
        gridItemChanged(m_eventGrid->getCurrentItem());
        updateUIList();
    }
}

void ZMEvents::deletePressed(void)
{
    if (!m_eventList || m_eventList->size() == 0)
        return;

    Event *event = m_eventList->at(m_currentEvent);
    if (event)
    {
        if (class ZMClient *zm = ZMClient::get())
            zm->deleteEvent(event->eventID);

        m_eventGrid->removeItem(m_currentEvent);

        vector<Event*>::iterator it;
        for (it = m_eventList->begin(); it != m_eventList->end(); it++)
        {
            if (*it == event)
            {
                m_eventList->erase(it);
                break;
            }
        }

        if (m_currentEvent > (int)m_eventList->size() - 1)
        {
            m_currentEvent = m_eventList->size() - 1;
            m_eventGrid->setCurrentPos(m_currentEvent);
        }

        gridItemChanged(m_eventGrid->getCurrentItem());

        updateUIList();
    }
}

void ZMEvents::getCameraList(void)
{
    if (class ZMClient *zm = ZMClient::get())
    {
        QStringList cameraList;
        zm->getCameraList(cameraList);
        if (!m_cameraSelector)
            return;

        m_cameraSelector->addItem(0, tr("All Cameras"));
        m_cameraSelector->setToItem(0);

        for (int x = 1; x <= cameraList.count(); x++)
        {
            m_cameraSelector->addItem(x, cameraList[x-1]);
        }

    }
}

void ZMEvents::setCamera(int item)
{
    (void) item;
    m_currentEvent = 0;
    getEventList();
}

void ZMEvents::getDateList(void)
{
    if (class ZMClient *zm = ZMClient::get())
    {
        QString monitorName = "<ANY>";

        if (m_cameraSelector && m_cameraSelector->getCurrentString() != tr("All Cameras") && 
            m_cameraSelector->getCurrentString() != "")
        {
            monitorName = m_cameraSelector->getCurrentString();
        }

        zm->getEventDates(monitorName, m_oldestFirst, m_dateList);
        if (!m_dateSelector)
            return;

        QString dateFormat = gContext->GetSetting("ZoneMinderDateFormat", "ddd - dd/MM");

        m_dateSelector->addItem(0, tr("All Dates"));
        m_dateSelector->setToItem(0);

        for (int x = 1; x <= m_dateList.count(); x++)
        {
            QDate date = QDate::fromString(m_dateList[x-1], Qt::ISODate);
            m_dateSelector->addItem(x, date.toString(dateFormat));
        }
    }
}

void ZMEvents::setDate(int item)
{
    (void) item;
    m_currentEvent = 0;
    getEventList();
}

void ZMEvents::updateImageGrid()
{
    m_eventGrid->reset();

    for (uint x = 0; x < m_eventList->size(); x++)
    {
        ImageGridItem *item = new ImageGridItem(m_eventList->at(x)->startTime,
                NULL, false, (void*) m_eventList->at(x));
        m_eventGrid->appendItem(item);
    }
    m_eventGrid->setItemCount(m_eventList->size());
    m_eventGrid->recalculateLayout();

    if (m_eventList->size() > 0)
        gridItemChanged(m_eventGrid->getItemAt(0));

    m_eventGrid->refresh();
}

void ZMEvents::gridItemChanged(ImageGridItem *item)
{
    if (!item)
        return;

    m_currentEvent = m_eventGrid->getCurrentPos();

    if (m_eventNoText)
        if (m_eventList->size() > 0)
            m_eventNoText->SetText(QString("%1/%2")
                    .arg(m_currentEvent + 1).arg(m_eventList->size()));
        else
            m_eventNoText->SetText("0/0");

    // update the pixmaps for all the visible items
    for (int x = m_eventGrid->getTopItemPos(); 
         x < m_eventGrid->getTopItemPos() + m_eventGrid->getVisibleCount(); x++)
    {
        ImageGridItem *gridItem = m_eventGrid->getItemAt(x);
        if (gridItem && gridItem->pixmap == NULL)
        {
            if (x < 0 || x > (int)m_eventList->size() - 1)
                continue;

            Event *event = m_eventList->at(x);
            if (event)
            {
                QImage image;
                if (class ZMClient *zm = ZMClient::get())
                {
                    zm->getAnalyseFrame(event->monitorID,
                                        event->eventID,
                                        0, image);
                    if (!image.isNull())
                    {
                        QSize size = m_eventGrid->getImageItemSize();
                        QPixmap *pixmap = new QPixmap(image.smoothScale(
                                size.width(), size.height(), Qt::KeepAspectRatio));

                        gridItem->pixmap = pixmap;
                    }
                }
            }
        }
    }

    m_eventGrid->refresh();
}

void ZMEvents::setView(bool gridView)
{
    if (gridView)
    {
        setContext(2);
        buildFocusList();
        m_eventGrid->setCurrentPos(m_currentEvent);
        gridItemChanged(m_eventGrid->getCurrentItem());
        setCurrentFocusWidget(m_eventGrid);
    }
    else
    {
        setContext(1);
        buildFocusList();
        setCurrentFocusWidget(m_event_list);
    }

    updateForeground();
}

void ZMEvents::setGridLayout(int layout)
{
    switch (layout)
    {
        case 1:
            m_eventGrid->setRowCount(1);
            m_eventGrid->setColumnCount(1);
            break;
        case 2:
            m_eventGrid->setRowCount(1);
            m_eventGrid->setColumnCount(2);
            break;
        case 6:
            m_eventGrid->setRowCount(2);
            m_eventGrid->setColumnCount(3);
            break;
        default:
            m_eventGrid->setRowCount(2);
            m_eventGrid->setColumnCount(3);
            break;
    }

    m_eventGrid->recalculateLayout();
    updateImageGrid();
    m_eventGrid->refresh();
}

void ZMEvents::showMenu()
{
    MythPopupBox *popup = new MythPopupBox(gContext->GetMainWindow(),
                                      "popup_menu");

    QLabel *caption = popup->addLabel(tr("Event List Menu"), MythPopupBox::Medium);
    caption->setAlignment(Qt::AlignCenter);

    QAbstractButton *button = popup->addButton(tr("Refresh"));
    if (getContext() == 1)
        popup->addButton(tr("Show Image View"));
    else
        popup->addButton(tr("Show List View"));
    button->setFocus();

    QLabel *splitter = popup->addLabel(" ", MythPopupBox::Small);
    splitter->setLineWidth(2);
    splitter->setFrameShape(Q3Frame::HLine);
    splitter->setFrameShadow(Q3Frame::Sunken);
    splitter->setMinimumHeight((int) (25 * hmult));
    splitter->setMaximumHeight((int) (25 * hmult));

    popup->addButton(tr("Delete All"));

    DialogCode res = popup->ExecPopup();
    switch (res)
    {
        case kDialogCodeButton0:
            // refresh event list;
                getEventList();
            break;
        case kDialogCodeButton1:
            if (getContext() == 1)
            {
                // switch to grid view;
                setView(true);
            }
            else
            {
                // switch to list view
                setView(false);
            }
            break;
        case kDialogCodeButton2:
            //delete all events
            if (class ZMClient *zm = ZMClient::get())
            {
                MythBusyDialog *busy = new MythBusyDialog(
                        QObject::tr("Deleting events. Please wait ..."));
                for (int x = 0; x < 5; x++)
                {
                    usleep(1000);
                    qApp->processEvents();
                }

                zm->deleteEventList(m_eventList);

                getEventList();
                busy->Close();
                busy->deleteLater();
            }
            break;
        case kDialogCodeRejected:
        default:
            break;
    }

    popup->deleteLater();
}
