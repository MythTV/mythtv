#include <unistd.h>
#include <cstdlib>
#include <memory>
#include <map>
#include <list>
#include <stack>
#include <set>
#include <cmath>
#include <functional>
#include <algorithm>

#include <QApplication>
#include <QStringList>
#include <QPixmap>
#include <QDir>
#include <Q3UrlOperator>
#include <QFileInfo>
#include <Q3Process>
#include <QPainter>
#include <QKeyEvent>
#include <QEvent>
#include <QImageReader>

#include <mythtv/mythcontext.h>
#include <mythtv/xmlparse.h>
#include <mythtv/compat.h>
#include <mythtv/mythdirs.h>

#include "globals.h"
#include "videomanager.h"
#include "videolist.h"

#include "videofilter.h"
#include "metadata.h"
#include "editmetadata.h"
#include "videoutils.h"
#include "metadatalistmanager.h"

// AEW
#define VB_DEBUG VB_IMPORTANT // A way to mark VERBOSE calls that probably
                              // should not see a commit.

namespace mythvideo_videomanager
{
    class ListBehaviorManager
    {
      public:
        enum ListBehavior
        {
            lbNone = 0x0,
            lbScrollCenter = 0x1,
            lbWrapList = 0x2
        };

        // an iterator over the window area with accompanying item IDs
        struct const_iterator
        {
            const_iterator(unsigned int item_index_) :
                item_index(item_index_), m_window_index(0)
            {
            }

            const_iterator(const const_iterator &other) :
                item_index(other.item_index),
                m_window_index(other.m_window_index)
            {
            }

            unsigned int item_index;

            const_iterator &operator++() // pre
            {
                ++m_window_index;
                ++item_index;
                return *this;
            }

            bool operator!=(const const_iterator &rhs)
            {
                return item_index != rhs.item_index;
            }

            unsigned int operator*()
            {
                return m_window_index;
            }

          private:
            unsigned int m_window_index;
        };

      public:
        ListBehaviorManager(unsigned int window_size = 0,
                               int behavior = lbNone,
                               unsigned int item_count = 0) :
            m_item_count(item_count), m_item_index(0), m_skip_index(SKIP_MAX),
            m_window_size(window_size), m_window_start_index(0),
            m_window_display_count(0)
        {
            m_scroll_center = behavior & lbScrollCenter;
            m_wrap_list = behavior & lbWrapList;
        }

        const_iterator begin()
        {
            return const_iterator(m_window_start_index);
        }

        const_iterator end()
        {
            return const_iterator(m_window_start_index +
                                  m_window_display_count);
        }

        void SetWindowSize(unsigned int window_size)
        {
            m_window_size = window_size;
            m_window_display_count = std::min(m_item_count, m_window_size);
            m_window_start_index = 0;
            Update();
        }

        unsigned int GetWindowSize() const { return m_window_size; }

        unsigned int GetWindowIndex() const
        {
            return m_item_index - m_window_start_index;
        }

        bool ItemsAboveWindow() const
        {
            return m_window_start_index;
        }

        bool ItemsBelowWindow() const
        {
            return m_window_start_index + m_window_display_count <
                    m_item_count;
        }

        void SetItemCount(unsigned int item_count)
        {
            m_item_count = item_count;
            m_window_display_count = std::min(m_item_count, m_window_size);
            m_item_index = bounded_index(m_item_index);
            m_window_start_index = 0;
            Update();
        }

        unsigned int GetItemCount() const
        {
            return m_item_count;
        }

        void SetItemIndex(unsigned int index)
        {
            m_item_index = bounded_index(index);
            Update();
        }

        unsigned int GetItemIndex() const
        {
            return m_item_index;
        }

        void SetSkipIndex(unsigned int skip = SKIP_MAX)
        {
            m_skip_index = skip;
            Update();
        }

        void Up()
        {
            Update(-1);
        }

        void Down()
        {
            Update(1);
        }

        void PageUp()
        {
            Update(-m_window_size);
        }

        void PageDown()
        {
            Update(m_window_size);
        }

      private:
        void Update(int move_by = 0)
        {
            if (move_by && m_item_count)
            {
                const unsigned int last_item_index = m_item_count - 1;
                const bool is_negative = move_by < 0 &&
                        m_item_index < static_cast<unsigned int>
                                       (std::abs(move_by));
                unsigned int after_move = 0;
                if (!is_negative)
                    after_move = m_item_index + move_by;

                if (m_skip_index != SKIP_MAX &&
                    after_move == m_skip_index)
                {
                    if (move_by < 0)
                    {
                        if (after_move)
                            --after_move;
                        else
                            ++after_move;
                    }
                    else
                        ++after_move;
                }

                if (is_negative)
                {
                    if (m_wrap_list && m_item_index == 0)
                        m_item_index = last_item_index;
                    else
                        m_item_index = 0;
                }
                else if (after_move >= m_item_count)
                {
                    if (m_wrap_list && m_item_index == last_item_index)
                        m_item_index = 0;
                    else
                        m_item_index = last_item_index;
                }
                else
                    m_item_index = after_move;
            }

            // place window
            const unsigned int half_window_count =
                    static_cast<unsigned int>(std::ceil(m_window_size / 2.0));
            const unsigned int sc_end_count =
                    half_window_count > m_item_count ?
                    0 : m_item_count - half_window_count;
            if (m_scroll_center &&
                m_item_index >= half_window_count &&
                m_item_index <= sc_end_count)
            {
                m_window_start_index = m_item_index - half_window_count;
            }
            else
            {
                // If the index is outside the window, move
                // the window.
                const unsigned int end_window_index =
                        m_window_start_index + m_window_display_count;
                if (m_item_index < m_window_start_index)
                    m_window_start_index = m_item_index;
                else if (m_item_index >= end_window_index)
                {
                    m_window_start_index =
                            m_item_index >= m_window_display_count ?
                            m_item_index + 1 - m_window_display_count : 0;
                }
            }
        }

        unsigned int bounded_index(unsigned int index)
        {
            unsigned int ret = 0;

            if (m_item_count)
                ret = index >= m_item_count ?  m_item_count - 1 : index;

            return ret;
        }

      private:
        unsigned int m_item_count;
        unsigned int m_item_index;
        unsigned int m_skip_index;

        unsigned int m_window_size;
        unsigned int m_window_start_index;
        unsigned int m_window_display_count;

        bool m_scroll_center;
        bool m_wrap_list;

        static const unsigned int SKIP_MAX = -1;
    };

    const unsigned int ListBehaviorManager::SKIP_MAX;

    // These will be set to support older themes that may not be
    // aware of the new context.
    enum DefaultContext
    {
        edcHidden = -100,
        edcAlwaysShown = -1,
        edcWaitContext = 1,
        edcSearchListContext = 2,
        edcManualUIDSearchContext = 3,
        edcManualTitleSearchContext = 4,
        edcNoVideoContext = 20
    };

    // Note: Containers are control/dialog non-windows, they suck.
    // This whole thing is overly complex, MythThemedDialog
    // introduces inefficiencies and should have a constructor
    // to build from an existing QDomElement. Additionally
    // UIListType is useless, controls that don't take input
    // are annoying (see ListBehaviorManager). Ideally the list
    // control would take input, and emit interesting actions.
    // It should also allow for the item data to be from an external
    // source. In other words, one day all of this container stuff
    // will be deleted and replaced by MythUI.

    struct ContainerDoneEvent : public QEvent
    {
        enum MyType { etContainerDone = QEvent::User + 1977 };
        ContainerDoneEvent() : QEvent(QEvent::Type(etContainerDone)) {}
    };

    class ContainerHandler : public QObject
    {
        Q_OBJECT

      public:
        enum HandlerFlag
        {
            ehfCanTakeFocus = 0x1 << 1 // Container can take focus
        };

        typedef MythThemedDialog ParentWindowType;

        enum ExitType
        {
            etSuccess,
            etFailure
        };

      public:
        ContainerHandler(QObject *oparent, ParentWindowType *pwt,
                XMLParse &theme, const QString &container_name,
                unsigned int flags, int context_override = edcAlwaysShown) :
            QObject(oparent),
            m_container(0), m_theme(&theme), m_pwt(pwt), m_done(false),
            m_container_name(container_name),
            m_flags(flags), m_exit_type(etFailure)
        {
            if (m_theme)
            {
                m_container = m_theme->GetSet(m_container_name);
                if (m_container)
                {
                    m_rect = m_container->GetAreaRect();
                    if (m_container->GetContext() == edcAlwaysShown &&
                        context_override != edcAlwaysShown)
                    {
                        ForceContext(m_container, context_override);
                    }
                }
                else
                {
                    VERBOSE(VB_IMPORTANT,
                            QString("MythVideo: VideoManager : Failed to "
                                    "get %1 object.").arg(m_container_name));
                }
            }
        }

        ~ContainerHandler()
        {
        }

        unsigned int SetFlags(unsigned int flags)
        {
            m_flags = flags;
            return m_flags;
        }

        unsigned int GetFlags() const { return m_flags; }

        ParentWindowType *GetParentWindow() { return m_pwt; }

        const QString &GetName() const { return m_container_name; }

        const QRect &GetRect() const { return m_rect; }

        // returns true if action handled
        virtual bool KeyPress(const QString &action)
        {
            if (action == "ESCAPE")
            {
                SetDone(true, etFailure);
                return true;
            }
            return false;
        }

        virtual void OnGainFocus() {}
        virtual void OnLoseFocus() {}

        int GetContext()
        {
            if (m_container)
                return m_container->GetContext();
            return edcAlwaysShown;
        }

        // Called after the container has done Success() or Failure()
        // and before this object is destroyed.
        void DoExit()
        {
            OnExit(m_exit_type);
        }

        bool IsDone() const { return m_done; }

        virtual void Invalidate()
        {
            Invalidate(m_rect);
        }

      private:
        void ForceContext(LayerSet *on, int context)
        {
            on->SetContext(context);

            typedef std::vector<UIType *> ui_types_list;
            ui_types_list *items = on->getAllTypes();
            if (items)
            {
                for (ui_types_list::iterator p = items->begin();
                     p != items->end(); ++p)
                {
                    (*p)->SetContext(context);
                }
            }
        }

      protected:
        void Invalidate(const QRect &rect)
        {
            m_pwt->updateForeground(rect);
        }

        void Success()
        {
            SetDone(true, etSuccess);
        }

        void Failure()
        {
            SetDone(true, etFailure);
        }

        // override to do things like send results
        virtual void OnExit(ExitType et)
        {
            (void) et;
        }

        void SetDone(bool done, ExitType et)
        {
            m_done = done;
            m_exit_type = et;
            SetFlags(0);
            qApp->postEvent(parent(), new ContainerDoneEvent());
        }

      protected:
        LayerSet *m_container;
        XMLParse *m_theme;

      private:
        ParentWindowType *m_pwt;
        bool m_done; // set to true when the container should be removed
        QString m_container_name;
        unsigned int m_flags;
        QRect m_rect;
        ExitType m_exit_type;
    };

    struct ContainerEvent
    {
        enum EventType { cetNone, cetKeyPress };

        ContainerEvent(EventType event_type = cetNone) :
            m_handled(false), m_event_type(event_type) {}

        virtual ~ContainerEvent() {}

        EventType GetType() const { return m_event_type; }

        virtual void Do(ContainerHandler *handler) = 0;

        bool GetHandled() const { return m_handled; }
        void SetHandled(bool handled = true) { m_handled = handled; }

      private:
        bool m_handled;
        EventType m_event_type;
    };

    struct CEKeyPress : public ContainerEvent
    {
        CEKeyPress(const QString &action) :
            ContainerEvent(ContainerEvent::cetKeyPress), m_action(action)
        {
        }

        void Do(ContainerHandler *handler)
        {
            SetHandled(handler->KeyPress(m_action));
        }

      private:
        CEKeyPress(const CEKeyPress &rhs); // no copies

      private:
        QString m_action;
    };

    // Dispatch container events to the top container. If there
    // is no top container it dispatches the events to the base
    // handler.
    template <typename HandlerType, typename DialogType>
    class ContainerDispatch
    {
      public:
        ContainerDispatch(QObject *event_dest, DialogType *parent_dialog) :
            m_event_dest(event_dest), m_parent_dialog(parent_dialog), m_focus(0)
        {
        }

        void push(HandlerType *handler)
        {
            m_handlers.push_back(handler);
            attach_handler(handler);
        }

        HandlerType *pop()
        {
            HandlerType *ret = 0;
            if (m_handlers.size())
            {
                ret = m_handlers.back();
                m_handlers.pop_back();
                detach_handler(ret, true);
            }

            return ret;
        }

        bool DispatchEvent(ContainerEvent &event)
        {
            if (m_handlers.size())
            {
                bool do_dispatch = false;
                HandlerType *handler = m_handlers.back();
                switch (event.GetType())
                {
                    case ContainerEvent::cetNone:
                    {
                        do_dispatch = true;
                        break;
                    }
                    case ContainerEvent::cetKeyPress:
                    {
                        handler = GetFocusedContainer();
                        if (handler && (handler->GetFlags() &
                                        HandlerType::ehfCanTakeFocus))
                        {
                            do_dispatch = true;
                        }
                        break;
                    }
                }

                if (do_dispatch)
                {
                    event.Do(handler);
                }
            }

            return event.GetHandled();
        }

        void ProcessDone()
        {
            do_container_cleanup();
        }

      private:
        HandlerType *GetFocusedContainer()
        {
            return m_focus;
        }

        void attach_handler(HandlerType *handler)
        {
            if (m_parent_dialog->getContext() != handler->GetContext())
            {
                m_parent_dialog->setContext(handler->GetContext());
                m_parent_dialog->buildFocusList();
            }

            HandlerType *next_focus = get_next_focus();
            if (m_focus && next_focus != m_focus)
            {
                m_focus->OnLoseFocus();
            }
            if (next_focus && next_focus != m_focus)
            {
                m_focus = next_focus;
                m_focus->OnGainFocus();
            }

            handler->Invalidate();
        }

        void detach_handler(HandlerType *handler)
        {
            if (handler == m_focus)
            {
                handler->OnLoseFocus();
                m_parent_dialog->buildFocusList();
                m_focus = get_next_focus();
                if (m_focus)
                    m_focus->OnGainFocus();
            }
        }

        HandlerType *get_next_focus()
        {
            HandlerType *ret = 0;
            if (m_handlers.size())
            {
                for (typename handlers::reverse_iterator p =
                        m_handlers.rbegin(); p != m_handlers.rend(); ++p)
                {
                    if ((*p)->GetFlags() & ContainerHandler::ehfCanTakeFocus)
                    {
                        ret = *p;
                        break;
                    }
                }
            }

            return ret;
        }

        void do_container_cleanup()
        {
            for (typename handlers::iterator p = m_handlers.begin();
                    p != m_handlers.end();)
            {
                // The handler could be done, if it is, remove it and
                // mark it for deletion.
                if ((*p)->IsDone())
                {
                    HandlerType *ht = get_next_focus();
                    int next_context = ht ? ht->GetContext() : edcAlwaysShown;
                    if (m_parent_dialog->getContext() != next_context)
                    {
                        m_parent_dialog->setContext(next_context);
                    }

                    detach_handler(*p);

                    (*p)->DoExit();
                    (*p)->Invalidate();
                    (*p)->deleteLater();
                    p = m_handlers.erase(p);
                }
                else
                    ++p;
            }
        }

      private:
        typedef std::list<HandlerType *> handlers;

      private:
        QObject *m_event_dest;
        DialogType *m_parent_dialog;
        handlers m_handlers;
        HandlerType *m_focus;
    };

    /////////////////////////////////////////////////////////////////
    // handlers
    /////////////////////////////////////////////////////////////////

    class SearchListHandler : public ContainerHandler
    {
        Q_OBJECT

      signals:
        void SigItemSelected(const QString &uid, const QString &title);
        void SigCancel();
        void SigReset();
        void SigManual();
        void SigManualTitle();

      public:
        //                          video uid, video title
        typedef std::vector<std::pair<QString, QString> > item_list;

      public:
        SearchListHandler(QObject *oparent, ParentWindowType *pwt,
                XMLParse &theme, const item_list &items,
                bool has_manual_title) :
            ContainerHandler(oparent, pwt, theme, "moviesel", ehfCanTakeFocus,
                             edcSearchListContext),
            m_item_list(items), m_list(0)
        {
            const int initial_size = m_item_list.size();

            if (initial_size)
                m_item_list.push_back(item_list::value_type("", ""));

            m_item_list.push_back(item_list::value_type(Action_Manual,
                    QObject::tr("Manually Enter Video #")));
            if (has_manual_title)
                m_item_list.push_back(item_list::value_type(Action_Manual_Title,
                        QObject::tr("Manually Enter Video Title")));
            m_item_list.push_back(item_list::value_type(Action_Reset,
                    QObject::tr("Reset Entry")));
            m_item_list.push_back(item_list::value_type(Action_Cancel,
                    QObject::tr("Cancel")));

            if (m_container)
            {
                m_list = dynamic_cast<UIListType *>
                        (m_container->GetType("listing"));
                if (m_list)
                {
                    m_list_behave.SetWindowSize(m_list->GetItems());
                    m_list_behave.SetItemCount(m_item_list.size());

                    if (initial_size)
                        m_list_behave.SetSkipIndex(initial_size);
                    UpdateContents();
                }
            }
        }

        bool KeyPress(const QString &action)
        {
            bool ret = true;
            if (action == "SELECT")
            {
                Success();
            }
            else if (action == "UP")
            {
                m_list_behave.Up();
                UpdateContents();
            }
            else if (action == "DOWN")
            {
                m_list_behave.Down();
                UpdateContents();
            }
            else if (action == "PAGEUP")
            {
                m_list_behave.PageUp();
                UpdateContents();
            }
            else if (action == "PAGEDOWN")
            {
                m_list_behave.PageDown();
                UpdateContents();
            }
            else
            {
                ret = ContainerHandler::KeyPress(action);
            }

            return ret;
        }

        void OnGainFocus()
        {
            if (m_list)
            {
                GetParentWindow()->setCurrentFocusWidget(m_list);
                m_list->SetActive(true);
            }
        }

        void OnLoseFocus()
        {
            if (m_list)
            {
                m_list->SetActive(false);
            }
        }

      private:
        void UpdateContents()
        {
            if (m_list)
            {
                m_list->ResetList();
                m_list->SetActive(true);

                for (ListBehaviorManager::const_iterator p =
                     m_list_behave.begin(); p != m_list_behave.end(); ++p)
                {
                    m_list->SetItemText(*p, 1,
                                        m_item_list.at(p.item_index).second);
                }

                m_list->SetItemCurrent(m_list_behave.GetWindowIndex());
                m_list->SetDownArrow(m_list_behave.ItemsBelowWindow());
                m_list->SetUpArrow(m_list_behave.ItemsAboveWindow());
                m_list->refresh();
            }
        }

        void OnExit(ExitType et)
        {
            if (et == etSuccess)
            {
                const item_list::value_type sel_item =
                        m_item_list.at(m_list_behave.GetItemIndex());

                if (sel_item.first == Action_Manual)
                    emit SigManual();
                else if (sel_item.first == Action_Manual_Title)
                    emit SigManualTitle();
                else if (sel_item.first == Action_Reset)
                    emit SigReset();
                else if (sel_item.first == Action_Cancel)
                    emit SigCancel();
                else
                    emit SigItemSelected(sel_item.first, sel_item.second);
            }
            else
            {
                emit SigCancel();
            }

            ContainerHandler::OnExit(et);
        }

      private:
        ListBehaviorManager m_list_behave;
        item_list m_item_list;
        UIListType *m_list;

        static const QString Action_Cancel;
        static const QString Action_Manual;
        static const QString Action_Manual_Title;
        static const QString Action_Reset;
    };

    const QString SearchListHandler::Action_Cancel("cancel");
    const QString SearchListHandler::Action_Manual("manual");
    const QString SearchListHandler::Action_Manual_Title("manual_title");
    const QString SearchListHandler::Action_Reset("reset");

    class RemoteEditKeyFilter : public QObject
    {
        Q_OBJECT

      signals:
        void SigSelect();
        void SigCancel();

      public:
        enum FilerBehavior { efbNumbersOnly = 1 };

      public:
        RemoteEditKeyFilter(QObject *oparent, unsigned int flags = 0) :
            QObject(oparent), m_flags(flags) {}

      protected:
        bool eventFilter(QObject *dest, QEvent *levent)
        {
            // returns true when the even will NOT be sent
            bool filtered = false;

            if (levent->type() == QEvent::KeyPress)
            {
                QKeyEvent *kp = dynamic_cast<QKeyEvent *>(levent);
                switch (kp->key())
                {
                    case Qt::Key_Return:
                    case Qt::Key_Enter:
                    {
                        emit SigSelect();
                        filtered = true;
                        break;
                    }
                    // stop MythRemoteLineEdit from doing focus changes
                    case Qt::Key_Up:
                    case Qt::Key_Down:
                    {
                        filtered = true;
                        break;
                    }
                    case Qt::Key_Escape:
                    {
                        filtered = true;
                        emit SigCancel();
                        break;
                    }
                }

                if (!filtered && (m_flags & efbNumbersOnly))
                {
                    if (kp->key() != Qt::Key_Delete &&
                        kp->key() != Qt::Key_Backspace &&
                        kp->text().length())
                    {
                        filtered = true;
                        MythRemoteLineEdit *mrle =
                                dynamic_cast<MythRemoteLineEdit *>(dest);
                        bool converted = false;
                        unsigned int num = kp->text().toUInt(&converted);
                        if (converted && mrle)
                        {
                            mrle->insert(QString::number(num));
                        }
                    }
                }
            }

            return filtered;
        }

      private:
        unsigned int m_flags;
    };

    class ManualSearchUIDHandler : public ContainerHandler
    {
        // TODO: for mythui make something like UIRemoteEditType that
        // takes a validator (though there is no good reason UIDs be
        // restricted to ints)
        Q_OBJECT

      signals:
        void SigTextChanged(const QString &uid);

      public:
        ManualSearchUIDHandler(QObject *oparent, ParentWindowType *pwt,
                          XMLParse &theme) :
            ContainerHandler(oparent, pwt, theme, "enterimdb", ehfCanTakeFocus,
                             edcManualUIDSearchContext)
        {
            if (m_container)
            {
                m_edit = dynamic_cast<UIRemoteEditType *>
                        (m_container->GetType("numhold"));
                if (m_edit)
                {
                    QWidget *edit_control = m_edit->getEdit();

                    if (!edit_control)
                    {
                        m_edit->createEdit(GetParentWindow());
                        edit_control = m_edit->getEdit();
                    }
                    else
                    {
                        // edit control only created once, need to clear
                        m_edit->setText("");
                        m_edit->show();
                    }

                    m_key_filter = new RemoteEditKeyFilter(this,
                            RemoteEditKeyFilter::efbNumbersOnly);
                    connect(m_key_filter, SIGNAL(SigSelect()),
                            SLOT(OnEditSelect()));
                    connect(m_key_filter, SIGNAL(SigCancel()),
                            SLOT(OnEditCancel()));

                    if (edit_control)
                    {
                        edit_control->installEventFilter(m_key_filter);
                    }

                    connect(m_edit, SIGNAL(textChanged(QString)),
                            SLOT(OnTextChange(QString)));
                }
            }
        }

      private:
        void UpdateContents()
        {
//            checkedSetText(m_container, "numhold", m_number);
        }

        void OnExit(ExitType et)
        {
            if (m_edit)
            {
                m_edit->hide();
                QWidget *edit_control = m_edit->getEdit();
                if (edit_control)
                    edit_control->removeEventFilter(m_key_filter);
            }

            if (et == etSuccess)
            {
                emit SigTextChanged(m_number);
            }
        }

        void OnGainFocus()
        {
            if (m_edit)
            {
                GetParentWindow()->setCurrentFocusWidget(m_edit);
            }
        }

      private slots:
        void OnTextChange(QString text)
        {
            m_number = text;
        }

        void OnEditSelect()
        {
            Success();
        }

        void OnEditCancel()
        {
            Failure();
        }

      private:
        QString m_number;
        UIRemoteEditType *m_edit;
        RemoteEditKeyFilter *m_key_filter;
    };

    class ManualSearchHandler : public ContainerHandler
    {
        Q_OBJECT

      signals:
        void SigTextChanged(const QString &text);

      public:
        ManualSearchHandler(QObject *oparent, ParentWindowType *pwt,
                            XMLParse &theme) :
            ContainerHandler(oparent, pwt, theme, container_name,
                             ehfCanTakeFocus, edcManualTitleSearchContext),
            m_edit(0), m_key_filter(0)
        {
            if (m_container)
            {
                m_edit = dynamic_cast<UIRemoteEditType *>
                        (m_container->GetType("title"));
                if (m_edit)
                {
                    QWidget *edit_control = m_edit->getEdit();

                    if (!edit_control)
                    {
                        m_edit->createEdit(GetParentWindow());
                        edit_control = m_edit->getEdit();
                    }
                    else
                    {
                        // edit control only created once, need to clear
                        m_edit->setText("");
                        m_edit->show();
                    }

                    m_key_filter = new RemoteEditKeyFilter(this);
                    connect(m_key_filter, SIGNAL(SigSelect()),
                            SLOT(OnEditSelect()));
                    connect(m_key_filter, SIGNAL(SigCancel()),
                            SLOT(OnEditCancel()));

                    if (edit_control)
                    {
                        edit_control->installEventFilter(m_key_filter);
                    }

                    connect(m_edit, SIGNAL(textChanged(QString)),
                            SLOT(OnTextChange(QString)));
                }
            }
        }

        static bool Exists(XMLParse *theme)
        {
            return theme->GetSet(container_name);
        }

        bool KeyPress(const QString &action)
        {
            bool ret = ContainerHandler::KeyPress(action);
            return ret;
        }

        void UpdateContents()
        {
            // do nothing
            //checkedSetText(m_container, "title", m_title);
        }

      private slots:
        void OnTextChange(QString text)
        {
            m_title = text;
        }

        void OnEditSelect()
        {
            Success();
        }

        void OnEditCancel()
        {
            Failure();
        }

      private:
        void OnExit(ExitType et)
        {
            if (m_edit)
            {
                m_edit->hide();
                QWidget *edit_control = m_edit->getEdit();
                if (edit_control)
                    edit_control->removeEventFilter(m_key_filter);
            }

            if (et == etSuccess)
            {
                emit SigTextChanged(m_title);
            }
        }

        void OnGainFocus()
        {
            if (m_edit)
            {
                GetParentWindow()->setCurrentFocusWidget(m_edit);
            }
        }

      private:
        QString m_title;
        UIRemoteEditType *m_edit;
        static const QString container_name;
        RemoteEditKeyFilter *m_key_filter;
    };

    const QString ManualSearchHandler::container_name("entersearchtitle");

    class InfoHandler : public ContainerHandler
    {
        Q_OBJECT

      public:
        struct CurrentInfoItemGetter
        {
            virtual const Metadata *GetItem() = 0;
            virtual ~CurrentInfoItemGetter() {}
        };

      public:
        InfoHandler(QObject *oparent, ParentWindowType *pwt, XMLParse &theme,
               CurrentInfoItemGetter *item_get, const QString &art_dir) :
            ContainerHandler(oparent, pwt, theme, "info", 0),
            m_art_dir(art_dir), m_item_get(item_get)
        {
            m_norec_container = m_theme->GetSet("novideos_info");
            if (m_norec_container &&
                m_norec_container->GetContext() == edcAlwaysShown)
                m_norec_container->SetContext(edcNoVideoContext);
        }

        void Update()
        {
            UpdateContents();
            Invalidate();
        }

      private:
        void UpdateContents()
        {
            const Metadata *item = m_item_get->GetItem();

            if (m_container && m_norec_container)
            {
               m_container->SetContext(item ? edcAlwaysShown : edcHidden);
               m_norec_container->SetContext(item ? edcHidden : edcAlwaysShown);
            }

            if (item && m_container)
            {
                checkedSetText(m_container, "title", item->Title());
                checkedSetText(m_container, "filename",
                               item->getFilenameNoPrefix());
                checkedSetText(m_container, "video_player",
                               Metadata::getPlayer(item));
                checkedSetText(m_container, "director", item->Director());
                checkedSetText(m_container, "cast", GetCast(*item));
                checkedSetText(m_container, "plot", item->Plot());
                checkedSetText(m_container, "rating", item->Rating());
                checkedSetText(m_container, "inetref", item->InetRef());
                checkedSetText(m_container, "year",
                               getDisplayYear(item->Year()));
                checkedSetText(m_container, "userrating",
                               getDisplayUserRating(item->UserRating()));
                checkedSetText(m_container, "length",
                               getDisplayLength(item->Length()));

                QString coverfile = item->CoverFile();
                coverfile.remove(m_art_dir + "/");
                checkedSetText(m_container, "coverfile", coverfile);

                checkedSetText(m_container, "child_id",
                               QString::number(item->ChildID()));
                checkedSetText(m_container, "browseable",
                               getDisplayBrowse(item->Browse()));
                checkedSetText(m_container, "category", item->Category());
                checkedSetText(m_container, "level",
                               QString::number(item->ShowLevel()));
            }
        }

      protected:
        void Invalidate()
        {
            QRect ir;

            if (m_container && m_container->GetContext() == edcAlwaysShown)
                ir |= m_container->GetAreaRect();

            if (m_norec_container && m_norec_container->GetContext() ==
                     edcAlwaysShown)
                ir |= m_norec_container->GetAreaRect();

            if (ir.isValid())
                ContainerHandler::Invalidate(ir);
        }

      private:
        QString m_art_dir;
        CurrentInfoItemGetter *m_item_get;
        LayerSet *m_norec_container;
    };

    // Handles the video list part of the video manager
    class ListHandler : public ContainerHandler
    {
        Q_OBJECT

      signals:
        void SigSelectionChanged();
        void SigItemEdit();
        void SigItemDelete();
        void SigItemToggleBrowseable();
        void SigItemChangeParental(int);
        void SigDoFilter();
        void SigDoMenu();

        void ListHandlerExit(); // sent when the mail window should close

      public:
        ListHandler(QObject *oparent, ParentWindowType *pwt, XMLParse &theme,
                VideoList *video_list) :
            ContainerHandler(oparent, pwt, theme, "selector", ehfCanTakeFocus),
            m_list_behave(0, ListBehaviorManager::lbScrollCenter |
                          ListBehaviorManager::lbWrapList),
            m_video_list(video_list)
        {
            m_list =
                    dynamic_cast<UIListType *>(m_container->GetType("listing"));
            if (m_list)
                m_list_behave.SetWindowSize(m_list->GetItems());
            SetSelectedItem(0);
        }

        void SetSelectedItem(unsigned int index)
        {
            m_list_behave.SetItemIndex(index);
            UpdateContents();
            emit SigSelectionChanged();
        }

        // called then the underlying list may have changed
        void OnListChanged()
        {
            m_list_behave.SetItemCount(m_video_list->count());
            UpdateContents();
            emit SigSelectionChanged();
        }

        Metadata *GetCurrentItem()
        {
            return m_video_list->
                    getVideoListMetadata(m_list_behave.GetItemIndex());
        }

        bool KeyPress(const QString &action)
        {
            bool ret = true;
            const unsigned int curindex = m_list_behave.GetItemIndex();

            if (action == "SELECT")
                emit SigItemEdit();
            else if (action == "UP")
                m_list_behave.Up();
            else if (action == "DOWN")
                m_list_behave.Down();
            else if (action == "PAGEUP")
                m_list_behave.PageUp();
            else if (action == "PAGEDOWN")
                m_list_behave.PageDown();
            else if (action == "DELETE")
                emit SigItemDelete();
            else if (action == "BROWSE")
                emit SigItemToggleBrowseable();
            else if (action == "INCPARENT")
                emit SigItemChangeParental(1);
            else if (action == "DECPARENT")
                emit SigItemChangeParental(-1);
            else if (action == "FILTER")
                emit SigDoFilter();
            else if ((action == "INFO") || action == "MENU")
                emit SigDoMenu();
            else if (action == "LEFT" || action == "ESCAPE")
                Success();
            else if (action == "RIGHT")
                emit SigDoMenu();
            else
                ret = false;

            // if the list index changed, we need to emit a change event
            if (curindex != m_list_behave.GetItemIndex())
            {
                UpdateContents();
                emit SigSelectionChanged();
            }

            return ret;
        }

        void Update()
        {
            UpdateContents();
        }

        void OnGainFocus()
        {
            if (m_list)
            {
                GetParentWindow()->setCurrentFocusWidget(m_list);
                m_list->SetActive(true);
            }
        }

        void OnLoseFocus()
        {
            if (m_list)
            {
                m_list->SetActive(false);
            }
        }

      private:
        void UpdateContents()
        {
            if (m_list)
            {
                m_list->ResetList();
                m_list->SetActive(true);

                for (ListBehaviorManager::const_iterator p =
                     m_list_behave.begin(); p != m_list_behave.end(); ++p)
                {
                    Metadata *meta =
                            m_video_list->getVideoListMetadata(p.item_index);

                    QString title = meta->Title();
                    QString filename = meta->Filename();
                    if (0 == title.compare("title"))
                    {
                        title = filename.section('/', -1);
                        if (!gContext->GetNumSetting("ShowFileExtensions"))
                            title = title.section('.',0,-2);
                    }

                    m_list->SetItemText(*p, 1, title);
                    m_list->SetItemText(*p, 2, meta->Director());
                    m_list->SetItemText(*p, 3, getDisplayYear(meta->Year()));
                }

                m_list->SetItemCurrent(m_list_behave.GetWindowIndex());
                m_list->SetDownArrow(m_list_behave.ItemsBelowWindow());
                m_list->SetUpArrow(m_list_behave.ItemsAboveWindow());
                m_list->refresh();
            }
        }

        void OnExit(ExitType et)
        {
            (void) et;
            emit ListHandlerExit();
        }

      private:
        ListBehaviorManager m_list_behave;
        VideoList *m_video_list;
        UIListType *m_list;
    };

    class WaitBackgroundHandler : public ContainerHandler
    {
        Q_OBJECT

      public:
        WaitBackgroundHandler(QObject *oparent, ParentWindowType *pwt,
                XMLParse &theme) :
            ContainerHandler(oparent, pwt, theme, "inetwait", ehfCanTakeFocus,
                             edcWaitContext)
        {
        }

        void EnterMessage(const QString &message)
        {
            m_message.push(message);
            UpdateContents();
        }

        bool LeaveMessage()
        {
            m_message.pop();
            bool more = m_message.size();
            if (more)
            {
                UpdateContents();
            }

            return more;
        }

        void Close()
        {
            Success();
        }

        bool KeyPress(const QString &action)
        {
            (void) action;
            return true;
        }

      private:
        void UpdateContents()
        {
            // set the title for the wait background
            if (m_message.size())
            {
                checkedSetText(m_container, "title", m_message.top());
            }
        }

      private:
        std::stack<QString> m_message;
    };

    class ExecuteExternalCommand : public QObject
    {
        Q_OBJECT

      protected:
        ExecuteExternalCommand(QObject *oparent) : QObject(oparent),
            m_process(this), m_purpose(QObject::tr("Command"))
        {
            connect(&m_process, SIGNAL(readyReadStdout()),
                    SLOT(OnReadReadyStdout()));
            connect(&m_process, SIGNAL(readyReadStderr()),
                    SLOT(OnReadReadyStderr()));
            connect(&m_process, SIGNAL(processExited()),
                    SLOT(OnProcessExit()));
        }

        ~ExecuteExternalCommand()
        {
        }

        void StartRun(const QString &command, const QStringList &args,
                      const QString &purpose)
        {
            m_purpose = purpose;

            // TODO: punting on spaces in path to command
            QStringList split_args = QStringList::split(' ', command);
            split_args += args;

            m_process.clearArguments();
            m_process.setArguments(split_args);

            VERBOSE(VB_GENERAL, QString("%1: Executing '%2'").arg(purpose)
                    .arg(split_args.join(" ")));

            m_raw_cmd = split_args[0];
            QFileInfo fi(m_raw_cmd);

            QString err_msg;

            if (!fi.exists())
            {
                err_msg = QString("\"%1\" failed: does not exist")
                        .arg(m_raw_cmd);
            }
            else if (!fi.isExecutable())
            {
                err_msg = QString("\"%1\" failed: not executable")
                        .arg(m_raw_cmd);
            }
            else if (!m_process.start())
            {
                err_msg = QString("\"%1\" failed: Could not start process")
                        .arg(m_raw_cmd);
            }

            if (err_msg.length())
            {
                ShowError(err_msg);
            }
        }

        virtual void OnExecDone(bool normal_exit, const QStringList &out,
                                const QStringList &err) = 0;

      private slots:
        void OnReadReadyStdout()
        {
            QByteArray buf = m_process.readStdout();
            m_std_out += QString::fromUtf8(buf.data(), buf.size());
        }

        void OnReadReadyStderr()
        {
            QByteArray buf = m_process.readStderr();
            m_std_error += QString::fromUtf8(buf.data(), buf.size());
        }

        void OnProcessExit()
        {
            if (!m_process.normalExit())
            {
                ShowError(QString("\"%1\" failed: Process exited abnormally")
                          .arg(m_raw_cmd));
            }

            if (m_std_error.length())
            {
                ShowError(m_std_error);
            }

            QStringList std_out = QStringList::split("\n", m_std_out);
            for (QStringList::iterator p = std_out.begin();
                 p != std_out.end(); )
            {
                QString check = (*p).stripWhiteSpace();
                if (check.at(0) == '#' || !check.length())
                {
                    p = std_out.erase(p);
                }
                else
                    ++p;
            }

            VERBOSE(VB_IMPORTANT, m_std_out);

            OnExecDone(m_process.normalExit(), std_out,
                       QStringList::split("\n", m_std_error));
        }

      private:
        void ShowError(const QString &error_msg)
        {
            VERBOSE(VB_IMPORTANT, error_msg);

            MythPopupBox::showOkPopup(gContext->GetMainWindow(),
                    QString(QObject::tr("%1 failed")).arg(m_purpose),
                    QString(QObject::tr("%1\n\nCheck VideoManager Settings"))
                    .arg(error_msg));
        }

      private:
        QString m_std_error;
        QString m_std_out;
        Q3Process m_process;
        QString m_purpose;
        QString m_raw_cmd;
    };

    // Executes the external command to do video title searches.
    class VideoTitleSearch : public ExecuteExternalCommand
    {
        Q_OBJECT

      signals:
        void SigSearchResults(bool normal_exit,
                              const SearchListHandler::item_list &items,
                              Metadata *item);

      public:
        VideoTitleSearch(QObject *oparent) : ExecuteExternalCommand(oparent),
            m_item(0)
        {
        }

        void Run(const QString &title, Metadata *item)
        {
            m_item = item;

            QString def_cmd = QDir::cleanDirPath(QString("%1/%2")
                    .arg(GetShareDir())
                    .arg("mythvideo/scripts/imdb.pl -M tv=no;video=no"));

            QString cmd = gContext->GetSetting("MovieListCommandLine", def_cmd);

            QStringList args;
            args += title;
            StartRun(cmd, args, "Video Search");
        }

      private:
        ~VideoTitleSearch() {}

        void OnExecDone(bool normal_exit, const QStringList &out,
                        const QStringList &err)
        {
            (void) err;
            typedef SearchListHandler::item_list item_list;

            item_list results;
            if (normal_exit)
            {
                for (QStringList::const_iterator p = out.begin();
                     p != out.end(); ++p)
                {
                    results.push_back(item_list::value_type(
                            (*p).section(':', 0, 0), (*p).section(':', 1)));
                }
            }

            emit SigSearchResults(normal_exit, results, m_item);
            deleteLater();
        }

      private:
        Metadata *m_item;
    };

    // Execute the command to do video searches based on their ID.
    class VideoUIDSearch : public ExecuteExternalCommand
    {
        Q_OBJECT

      signals:
        void SigSearchResults(bool normal_exit, const QStringList &results,
                              Metadata *item, const QString &video_uid);
      public:
        VideoUIDSearch(QObject *oparent) : ExecuteExternalCommand(oparent),
            m_item(0)
        {
        }

        void Run(const QString &video_uid, Metadata *item)
        {
            m_item = item;
            m_video_uid = video_uid;

            const QString def_cmd = QDir::cleanDirPath(QString("%1/%2")
                    .arg(GetShareDir())
                    .arg("mythvideo/scripts/imdb.pl -D"));
            const QString cmd = gContext->GetSetting("MovieDataCommandLine",
                                                     def_cmd);

            StartRun(cmd, QStringList(video_uid), "Video Data Query");
        }

      private:
        ~VideoUIDSearch() {}

        void OnExecDone(bool normal_exit, const QStringList &out,
                        const QStringList &err)
        {
            (void) err;
            emit SigSearchResults(normal_exit, out, m_item, m_video_uid);
            deleteLater();
        }

      private:
        Metadata *m_item;
        QString m_video_uid;
    };

    // Execute external video poster command.
    class VideoPosterSearch : public ExecuteExternalCommand
    {
        Q_OBJECT

      signals:
        void SigPosterURL(const QString &url, Metadata *item);

      public:
        VideoPosterSearch(QObject *oparent) : ExecuteExternalCommand(oparent),
            m_item(0)
        {
        }

        void Run(const QString &video_uid, Metadata *item)
        {
            m_item = item;

            const QString default_cmd =
                    QDir::cleanDirPath(QString("%1/%2")
                                       .arg(GetShareDir())
                                       .arg("mythvideo/scripts/imdb.pl -P"));
            const QString cmd = gContext->GetSetting("MoviePosterCommandLine",
                                                     default_cmd);
            StartRun(cmd, QStringList(video_uid), "Poster Query");
        }

      private:
        ~VideoPosterSearch() {}

        void OnExecDone(bool normal_exit, const QStringList &out,
                        const QStringList &err)
        {
            (void) err;
            QString url;
            if (normal_exit && out.size())
            {
                for (QStringList::const_iterator p = out.begin();
                     p != out.end(); ++p)
                {
                    if ((*p).length())
                    {
                        url = *p;
                        break;
                    }
                }
            }

            emit SigPosterURL(url, m_item);
            deleteLater();
        }

      private:
        Metadata *m_item;
    };

    // Holds the timer used for the poster download timeout notification.
    class TimeoutSignalProxy : public QObject
    {
        Q_OBJECT

      signals:
        void SigTimeout(const QString &url, Metadata *item);

      public:
        TimeoutSignalProxy() : m_item(0), m_timer(this)
        {
            connect(&m_timer, SIGNAL(timeout()), SLOT(OnTimeout()));
        }

        void start(int timeout, Metadata *item, const QString &url)
        {
            m_item = item;
            m_url = url;
            m_timer.start(timeout, true);
        }

        void stop()
        {
            if (m_timer.isActive())
                m_timer.stop();
        }

      private slots:
        void OnTimeout()
        {
            emit SigTimeout(m_url, m_item);
        }

      private:
        Metadata *m_item;
        QString m_url;
        QTimer m_timer;
    };

    // Used so we can be notified by QNetworkOperation with our item.
    class URLOperationProxy : public QObject
    {
        Q_OBJECT

      signals:
        void SigFinished(Q3NetworkOperation *op, Metadata *item);

      public:
        URLOperationProxy() : m_item(0)
        {
            connect(&m_url_op, SIGNAL(finished(Q3NetworkOperation *)),
                    SLOT(OnFinished(Q3NetworkOperation *)));
        }

        void copy(const QString &uri, const QString &dest, Metadata *item)
        {
            m_item = item;
            m_url_op.copy(uri, dest, false, false);
        }

        void stop()
        {
            m_url_op.stop();
        }

      private slots:
        void OnFinished(Q3NetworkOperation *op)
        {
            emit SigFinished(op, m_item);
        }

      private:
        Metadata *m_item;
        Q3UrlOperator m_url_op;
    };

    class VideoManagerImp : public QObject
    {
        Q_OBJECT

      private:
        struct CurrentItemGet : public InfoHandler::CurrentInfoItemGetter
        {
            CurrentItemGet() : m_list_handler(0) {}

            const Metadata *GetItem()
            {
                if (m_list_handler)
                    return m_list_handler->GetCurrentItem();
                return 0;
            }

            void connect(ListHandler *handler)
            {
                m_list_handler = handler;
            }

            void disconnect()
            {
                m_list_handler = 0;
            }

          private:
            ListHandler *m_list_handler;
        };

        typedef std::list<std::pair<QString, ParentalLevel::Level> >
                parental_level_map;

        struct rating_to_pl_less :
            public std::binary_function<parental_level_map::value_type,
                                        parental_level_map::value_type, bool>
        {
            bool operator()(const parental_level_map::value_type &lhs,
                           const parental_level_map::value_type &rhs) const
            {
                return lhs.first.length() < rhs.first.length();
            }
        };

      public:
        VideoManagerImp(VideoManager *vm, XMLParse *theme, const QRect &area,
                VideoList *video_list) :
            m_event_dispatch(this, vm), m_vm(vm), m_theme(theme), m_area(area),
            m_video_list(video_list), m_info_handler(0), m_list_handler(0),
            m_popup(0), m_wait_background(0), m_has_manual_title_search(false)
        {
            m_art_dir = gContext->GetSetting("VideoArtworkDir");

            if (gContext->
                GetNumSetting("mythvideo.ParentalLevelFromRating", 0))
            {
                for (ParentalLevel sl(ParentalLevel::plLowest);
                     sl.GetLevel() <= ParentalLevel::plHigh && sl.good(); ++sl)
                {
                    QStringList ratings = QStringList::split(':', gContext->
                            GetSetting(QString("mythvideo.AutoR2PL%1")
                                       .arg(sl.GetLevel())));

                    for (QStringList::const_iterator p = ratings.begin();
                         p != ratings.end(); ++p)
                    {
                        m_rating_to_pl.push_back(
                            parental_level_map::value_type(*p, sl.GetLevel()));
                    }
                }
                m_rating_to_pl.sort(std::not2(rating_to_pl_less()));
            }

            m_info_handler = new InfoHandler(this, m_vm, *m_theme,
                    &m_current_item_proxy, m_art_dir);
            m_list_handler = new ListHandler(this, m_vm, *m_theme, video_list);

            m_current_item_proxy.connect(m_list_handler);

            m_vm->connect(m_list_handler, SIGNAL(ListHandlerExit()),
                         SLOT(ExitWin()));

            m_event_dispatch.push(m_info_handler);
            m_event_dispatch.push(m_list_handler);

            connect(m_list_handler, SIGNAL(SigSelectionChanged()),
                    SLOT(OnListSelectionChange()));

            connect(m_list_handler, SIGNAL(SigItemEdit()),
                    SLOT(DoEditMetadata()));
            connect(m_list_handler, SIGNAL(SigItemDelete()),
                    SLOT(DoRemoveVideo()));
            connect(m_list_handler, SIGNAL(SigItemToggleBrowseable()),
                    SLOT(DoToggleBrowseable()));
            connect(m_list_handler, SIGNAL(SigItemChangeParental(int)),
                    SLOT(OnParentalChange(int)));
            connect(m_list_handler, SIGNAL(SigDoFilter()),
                    SLOT(DoFilter()));
            connect(m_list_handler, SIGNAL(SigDoMenu()),
                    SLOT(DoVideoMenu()));

            video_list->setCurrentVideoFilter(VideoFilterSettings(true,
                            "VideoManager"));

            // This bit of ugliness done only for theme compatibility.
            // Without this, themes that don't put dialogs in
            // their own context would have a messy screen.
            struct context_check
            {
                context_check(XMLParse *ltheme, const QString &name,
                              int default_context)
                {
                    LayerSet *s = ltheme->GetSet(name);
                    if (s && s->GetContext() == edcAlwaysShown)
                    {
                        s->SetContext(default_context);
                    }
                }
            };

            context_check(theme, "moviesel", edcSearchListContext);
            context_check(theme, "enterimdb", edcManualUIDSearchContext);
            context_check(theme, "entersearchtitle",
                          edcManualTitleSearchContext);
            context_check(theme, "inetwait", edcWaitContext);

            RefreshVideoList(false);

            m_has_manual_title_search = ManualSearchHandler::Exists(m_theme);
            connect(&m_url_dl_timer,
                    SIGNAL(SigTimeout(const QString &, Metadata *)),
                    SLOT(OnPosterDownloadTimeout(const QString &, Metadata *)));
            connect(&m_url_operator,
                    SIGNAL(SigFinished(Q3NetworkOperation *, Metadata *)),
                    SLOT(OnPosterCopyFinished(Q3NetworkOperation *,
                                              Metadata *)));
        }

        ~VideoManagerImp()
        {
            m_current_item_proxy.disconnect();
        }

        bool DispatchEvent(ContainerEvent &event_)
        {
            bool ret = m_event_dispatch.DispatchEvent(event_);
            if (!ret)
            {
                // unhandled event TODO
            }

            return ret;
        }

        void customEvent(QEvent *e)
        {
            if (static_cast<int>(e->type()) ==
                ContainerDoneEvent::etContainerDone)
            {
                m_event_dispatch.ProcessDone();
            }
        }

      private:
        void CancelPopup()
        {
            if (m_popup)
            {
                m_popup->deleteLater();
                m_popup = NULL;
            }
        }

        void RefreshVideoList(bool resort_only);

        static bool GetLocalVideoPoster(const QString &video_uid,
                                        const QString &filename,
                                        const QStringList &in_dirs,
                                        QString &poster)
        {
            QStringList search_dirs(in_dirs);

            QFileInfo qfi(filename);
            search_dirs += qfi.dirPath(true);

            const QString base_name = qfi.baseName(true);
            QList<QByteArray> image_types =
                    QImageReader::supportedImageFormats();

            typedef std::set<QString> image_type_list;
            image_type_list image_exts;

            for (QList<QByteArray>::const_iterator it = image_types.begin();
                    it != image_types.end(); ++it)
            {
                image_exts.insert(QString(*it).lower());
            }

            if (image_exts.find("jpeg") != image_exts.end())
            {
                image_exts.insert("jpg"); // normally only lists jpeg
            }

            const QString fntm("%1/%2.%3");

            for (QStringList::const_iterator dir = search_dirs.begin();
                    dir != search_dirs.end(); ++dir)
            {
                if (!(*dir).length()) continue;

                for (image_type_list::const_iterator ext = image_exts.begin();
                        ext != image_exts.end(); ++ext)
                {
                    QStringList sfn;
                    sfn += fntm.arg(*dir).arg(base_name).arg(*ext);
                    sfn += fntm.arg(*dir).arg(video_uid).arg(*ext);

                    for (QStringList::const_iterator i = sfn.begin();
                            i != sfn.end(); ++i)
                    {
                        if (QFile::exists(*i))
                        {
                            poster = *i;
                            return true;
                        }
                    }
                }
            }

            return false;
        }

        void ResetItem(Metadata *item)
        {
            if (item)
            {
                item->Reset();
                item->updateDatabase();

                RefreshVideoList(false);
            }
        }

        void StartWaitBackground(const QString &text)
        {
            if (!m_wait_background)
            {
                m_wait_background =
                        new WaitBackgroundHandler(this, m_vm, *m_theme);
                m_event_dispatch.push(m_wait_background);
            }

            m_wait_background->EnterMessage(text);
        }

        void StopWaitBackground()
        {
            if (m_wait_background)
            {
                if (!m_wait_background->LeaveMessage())
                {
                    m_wait_background->Close();
                    m_wait_background = 0;
                }
            }
            else
            {
                VERBOSE(VB_IMPORTANT, "Error: StopWaitBackground called with "
                                      "no active message.");
            }
        }

        void AutomaticParentalAdjustment(Metadata *item);

      // Start asynchronous functions.

      // These are the start points, separated for sanity.
      private:
        // StartVideoPosterSet() start wait background
        //   OnPosterURL()
        //     OnPosterCopyFinished()
        //       OnVideoPosterSetDone()
        //     OnPosterDownloadTimeout()
        //       OnPosterCopyFinished()
        // OnVideoPosterSetDone() stop wait background
        void StartVideoPosterSet(Metadata *item);

        // StartVideoSearchByUID() start wait background
        //   OnVideoSearchByUIDDone() stop wait background
        //     StartVideoPosterSet()
        void StartVideoSearchByUID(const QString &video_uid, Metadata *item);

        // StartVideoSearchByTitle()
        //   OnVideoSearchByTitleDone() start wait background
        //   OnVideoSearchByTitleDoneNoBackground()
        void StartVideoSearchByTitle(const QString &video_uid,
                                     const QString &title, Metadata *item);

      // any intervening stops
      private slots:
        // called during StartVideoPosterSet
        void OnPosterURL(const QString &uri, Metadata *item);
        void OnPosterCopyFinished(Q3NetworkOperation *op, Metadata *item);
        void OnPosterDownloadTimeout(const QString &url, Metadata *item);

        // called during StartVideoSearchByTitle
        void OnVideoSearchByTitleDone(bool normal_exit,
                const SearchListHandler::item_list &results, Metadata *item);

      // and now the end points
      private slots:
        // StartVideoPosterSet end
        void OnVideoPosterSetDone(Metadata *item);

        // StartVideoSearchByUID end
        void OnVideoSearchByUIDDone(bool normal_exit,
                                    const QStringList &output,
                                    Metadata *item, const QString &video_uid);

        // StartVideoSearchByTitle end
        void OnVideoSearchByTitleDoneNoBackground(bool normal_exit,
                const SearchListHandler::item_list &results, Metadata *item);

      // End asynchronous functions.

      private slots:
        // slots that bring up other windows
        void DoEditMetadata();
        void DoRemoveVideo();
        void DoFilter();
        void DoManualVideoUID();
        void DoManualVideoTitle();
        void DoVideoSearchCurrentItem()
        {
            CancelPopup();

            Metadata *item = m_list_handler->GetCurrentItem();
            if (item)
            {
                StartVideoSearchByTitle(item->InetRef(), item->Title(), item);
            }
        }

        void DoVideoMenu()
        {
            m_popup = new MythPopupBox(gContext->GetMainWindow(),
                                       "video popup");

            m_popup->addLabel(tr("Select action:"));
            m_popup->addLabel("");

            QAbstractButton *editButton = NULL;
            if (m_list_handler->GetCurrentItem())
            {
                editButton = m_popup->addButton(tr("Edit Metadata"), this,
                                              SLOT(DoEditMetadata()));
                m_popup->addButton(tr("Search"), this,
                                   SLOT(DoVideoSearchCurrentItem()));
                m_popup->addButton(tr("Manually Enter Video #"), this,
                                   SLOT(DoManualVideoUID()));
                if (m_has_manual_title_search)
                {
                    m_popup->addButton(tr("Manually Enter Video Title"), this,
                                       SLOT(DoManualVideoTitle()));
                }
                m_popup->addButton(tr("Reset Metadata"), this,
                                   SLOT(DoResetMetadata()));
                m_popup->addButton(tr("Toggle Browseable"), this,
                                   SLOT(DoToggleBrowseable()));
                m_popup->addButton(tr("Remove Video"), this,
                                   SLOT(DoRemoveVideo()));
            }

            QAbstractButton *filterButton =
                    m_popup->addButton(tr("Filter Display"), this,
                                       SLOT(DoFilter()));
            m_popup->addButton(tr("Cancel"), this, SLOT(OnVideoMenuDone()));

            m_popup->ShowPopup(this, SLOT(OnVideoMenuDone()));
            m_popup->setActiveWindow();

            if (editButton)
                editButton->setFocus();
            else
                filterButton->setFocus();
        }

        void DoToggleBrowseable()
        {
            CancelPopup();

            Metadata *item = m_list_handler->GetCurrentItem();
            if (item)
            {
                item->setBrowse(!item->Browse());
                item->updateDatabase();

                RefreshVideoList(false);
                OnSelectedItemChange();
            }
        }

        void OnParentalChange(int amount);

        // called when the list selection changed
        void OnListSelectionChange()
        {
            m_info_handler->Update();
        }

        // Called when the underlying data for an item changes
        void OnSelectedItemChange()
        {
            m_info_handler->Update();
            m_list_handler->Update();
        }

        void DoResetMetadata();

        void OnVideoMenuDone();

        void OnVideoSearchListCancel()
        {
            // I'm keeping this behavior for now, though item
            // modification on Cancel is seems anathema to me.
            Metadata *item = m_list_handler->GetCurrentItem();

            if (item && isDefaultCoverFile(item->CoverFile()))
            {
                QStringList search_dirs;
                search_dirs += m_art_dir;
                QString cover_file;

                if (GetLocalVideoPoster(item->InetRef(), item->Filename(),
                                        search_dirs, cover_file))
                {
                    item->setCoverFile(cover_file);
                    item->updateDatabase();
                    RefreshVideoList(true);
                }
            }
        }

        void OnVideoSearchListReset()
        {
            DoResetMetadata();
        }

        void OnVideoSearchListManual()
        {
            DoManualVideoUID();
        }

        void OnVideoSearchListManualTitle()
        {
            DoManualVideoTitle();
        }

        void OnVideoSearchListSelection(const QString &video_uid,
                                        const QString &video_title)
        {
            (void) video_title;
            Metadata *item = m_list_handler->GetCurrentItem();
            if (item && video_uid.length())
            {
                StartVideoSearchByUID(video_uid, item);
            }
        }

        void OnManualVideoUID(const QString &video_uid);
        void OnManualVideoTitle(const QString &title);

      private:
        ContainerDispatch<ContainerHandler, VideoManager> m_event_dispatch;
        VideoManager *m_vm;
        XMLParse *m_theme;
        QRect m_area;
        VideoList *m_video_list;
        InfoHandler *m_info_handler;
        ListHandler *m_list_handler;
        MythPopupBox *m_popup;
        WaitBackgroundHandler *m_wait_background;
        CurrentItemGet m_current_item_proxy;
        QString m_art_dir;
        bool m_has_manual_title_search;
        URLOperationProxy m_url_operator;
        TimeoutSignalProxy m_url_dl_timer;
        parental_level_map m_rating_to_pl;
    };

    void VideoManagerImp::RefreshVideoList(bool resort_only)
    {
        static bool updateML = false;
        if (updateML == true)
            return;
        updateML = true;

        unsigned int selected_id = 0;
        const Metadata *item = m_list_handler->GetCurrentItem();
        if (item)
            selected_id = item->ID();

        if (resort_only)
        {
            m_video_list->resortList(true);
        }
        else
        {
            m_video_list->refreshList(false,
                    ParentalLevel(ParentalLevel::plNone), true);
        }

        m_list_handler->OnListChanged();

        // TODO: This isn't perfect, if you delete the last item your selection
        // reverts to the first item.
        if (selected_id)
        {
            MetadataListManager::MetadataPtr sel_item =
                    m_video_list->getListCache().byID(selected_id);
            if (sel_item)
            {
                m_list_handler->SetSelectedItem(sel_item->getFlatIndex());
            }
        }

        updateML = false;
    }

    void VideoManagerImp::AutomaticParentalAdjustment(Metadata *item)
    {
        if (item && m_rating_to_pl.size())
        {
            QString rating = item->Rating();
            for (parental_level_map::const_iterator p = m_rating_to_pl.begin();
                 rating.length() && p != m_rating_to_pl.end(); ++p)
            {
                if (rating.find(p->first) != -1)
                {
                    item->setShowLevel(p->second);
                    break;
                }
            }
        }
    }

    // Copy video poster to appropriate directory and set the item's cover file.
    // This is the start of an async operation that needs to always complete
    // to OnVideoPosterSetDone.
    void VideoManagerImp::StartVideoPosterSet(Metadata *item)
    {
        StartWaitBackground(QObject::tr("Fetching poster for %1 (%2)")
                            .arg(item->InetRef())
                            .arg(item->Title()));
        QStringList search_dirs;
        search_dirs += m_art_dir;

        QString cover_file;

        if (GetLocalVideoPoster(item->InetRef(), item->Filename(), search_dirs,
                                cover_file))
        {
            item->setCoverFile(cover_file);
            OnVideoPosterSetDone(item);
            return;
        }

        // Obtain video poster
        VideoPosterSearch *vps = new VideoPosterSearch(this);
        connect(vps, SIGNAL(SigPosterURL(const QString &, Metadata *)),
                SLOT(OnPosterURL(const QString &, Metadata *)));
        vps->Run(item->InetRef(), item);
    }

    void VideoManagerImp::OnPosterURL(const QString &uri, Metadata *item)
    {
        if (item)
        {
            if (uri.length())
            {
                QString fileprefix = m_art_dir;

                QDir dir;

                // If the video artwork setting hasn't been set default to
                // using ~/.mythtv/MythVideo
                if (fileprefix.length() == 0)
                {
                    fileprefix = GetConfDir();

                    dir.setPath(fileprefix);
                    if (!dir.exists())
                        dir.mkdir(fileprefix);

                    fileprefix += "/MythVideo";
                }

                dir.setPath(fileprefix);
                if (!dir.exists())
                    dir.mkdir(fileprefix);

                Q3Url url(uri);

                QString ext = QFileInfo(url.fileName()).extension(false);
                QString dest_file = QString("%1/%2.%3").arg(fileprefix)
                        .arg(item->InetRef()).arg(ext);
                VERBOSE(VB_IMPORTANT, QString("Copying '%1' -> '%2'...")
                        .arg(uri).arg(dest_file));

                item->setCoverFile(dest_file);

                m_url_operator.copy(uri, QString("file:%1").arg(dest_file),
                                    item);
                VERBOSE(VB_IMPORTANT,
                        QString("dest_file = %1").arg(dest_file));

                const int nTimeout =
                        gContext->GetNumSetting("PosterDownloadTimeout", 30)
                        * 1000;
                m_url_dl_timer.start(nTimeout, item, url);
            }
            else
            {
                item->setCoverFile("");
                OnVideoPosterSetDone(item);
            }
        }
        else
            OnVideoPosterSetDone(item);
    }

    void VideoManagerImp::OnPosterCopyFinished(Q3NetworkOperation *op,
                                               Metadata *item)
    {
        m_url_dl_timer.stop();
        QString state, operation;
        switch(op->operation())
        {
            case Q3NetworkProtocol::OpMkDir:
                operation = "MkDir";
                break;
            case Q3NetworkProtocol::OpRemove:
                operation = "Remove";
                break;
            case Q3NetworkProtocol::OpRename:
                operation = "Rename";
                break;
            case Q3NetworkProtocol::OpGet:
                operation = "Get";
                break;
            case Q3NetworkProtocol::OpPut:
                operation = "Put";
                break;
            default:
                operation = "Uknown";
                break;
        }

        switch(op->state())
        {
            case Q3NetworkProtocol::StWaiting:
                state = "The operation is in the QNetworkProtocol's queue "
                        "waiting to be prcessed.";
                break;
            case Q3NetworkProtocol::StInProgress:
                state = "The operation is being processed.";
                break;
            case Q3NetworkProtocol::StDone:
                state = "The operation has been processed succesfully.";
                break;
            case Q3NetworkProtocol::StFailed:
                state = "The operation has been processed but an error "
                        "occurred.";
                if (item)
                    item->setCoverFile("");
                break;
            case Q3NetworkProtocol::StStopped:
                state = "The operation has been processed but has been stopped "
                        "before it finished, and is waiting to be processed.";
                break;
            default:
                state = "Unknown";
                break;
        }

        VERBOSE(VB_IMPORTANT, QString("%1: %2: %3").arg(operation).arg(state)
                .arg(op->protocolDetail()));

        OnVideoPosterSetDone(item);
    }

    void VideoManagerImp::OnPosterDownloadTimeout(const QString &url,
                                               Metadata *item)
    {
        VERBOSE(VB_IMPORTANT, QString("Copying of '%1' timed out").arg(url));

        if (item)
            item->setCoverFile("");

        m_url_operator.stop(); // results in OnPosterCopyFinished

        MythPopupBox::showOkPopup(gContext->GetMainWindow(),
                QObject::tr("Could not retrieve poster"),
                QObject::tr("A poster exists for this item but could not be "
                            "retrieved within the timeout period.\n"));
    }

    // This is the final call as part of a StartVideoPosterSet
    void VideoManagerImp::OnVideoPosterSetDone(Metadata *item)
    {
        // The item has some cover file set
        StopWaitBackground();

        item->updateDatabase();
        RefreshVideoList(true);
        OnSelectedItemChange();
    }

    void VideoManagerImp::StartVideoSearchByUID(const QString &video_uid,
                                                Metadata *item)
    {
        StartWaitBackground(video_uid);
        VideoUIDSearch *vns = new VideoUIDSearch(this);
        connect(vns, SIGNAL(SigSearchResults(bool, const QStringList &,
                                             Metadata *, const QString &)),
                SLOT(OnVideoSearchByUIDDone(bool, const QStringList &,
                                            Metadata *, const QString &)));
        vns->Run(video_uid, item);
    }

    void VideoManagerImp::OnVideoSearchByUIDDone(bool normal_exit,
                                                 const QStringList &output,
                                                 Metadata *item,
                                                 const QString &video_uid)
    {
        StopWaitBackground();

        std::map<QString, QString> data;

        if (normal_exit && output.size())
        {
            for (QStringList::const_iterator p = output.begin();
                 p != output.end(); ++p)
            {
                data[(*p).section(':', 0, 0)] = (*p).section(':', 1);
            }
            // set known values
            item->setTitle(data["Title"]);
            item->setYear(data["Year"].toInt());
            item->setDirector(data["Director"]);
            item->setPlot(data["Plot"]);
            item->setUserRating(data["UserRating"].toFloat());
            item->setRating(data["MovieRating"]);
            item->setLength(data["Runtime"].toInt());

            AutomaticParentalAdjustment(item);

            // Cast
            Metadata::cast_list cast;
            QStringList cl = QStringList::split(",", data["Cast"]);

            for (QStringList::const_iterator p = cl.begin();
                 p != cl.end(); ++p)
            {
                QString cn = (*p).stripWhiteSpace();
                if (cn.length())
                {
                    cast.push_back(Metadata::cast_list::
                                   value_type(-1, cn));
                }
            }

            item->setCast(cast);

            // Genres
            Metadata::genre_list video_genres;
            QStringList genres = QStringList::split(",", data["Genres"]);

            for (QStringList::const_iterator p = genres.begin();
                 p != genres.end(); ++p)
            {
                QString genre_name = (*p).stripWhiteSpace();
                if (genre_name.length())
                {
                    video_genres.push_back(
                            Metadata::genre_list::value_type(-1, genre_name));
                }
            }

            item->setGenres(video_genres);

            // Countries
            Metadata::country_list video_countries;
            QStringList countries = QStringList::split(",", data["Countries"]);
            for (QStringList::const_iterator p = countries.begin();
                 p != countries.end(); ++p)
            {
                QString country_name = (*p).stripWhiteSpace();
                if (country_name.length())
                {
                    video_countries.push_back(
                            Metadata::country_list::value_type(-1,
                                    country_name));
                }
            }

            item->setCountries(video_countries);

            item->setInetRef(video_uid);
            StartVideoPosterSet(item);
        }
        else
        {
            ResetItem(item);
            item->updateDatabase();
            RefreshVideoList(true);
            OnSelectedItemChange();
        }
    }

    void VideoManagerImp::StartVideoSearchByTitle(const QString &video_uid,
                                                  const QString &title,
                                                  Metadata *item)
    {
        if (video_uid == VIDEO_INETREF_DEFAULT)
        {
            StartWaitBackground(title);

            VideoTitleSearch *vts = new VideoTitleSearch(this);
            connect(vts,
                    SIGNAL(SigSearchResults(bool,
                            const SearchListHandler::item_list &, Metadata *)),
                    SLOT(OnVideoSearchByTitleDone(bool,
                            const SearchListHandler::item_list &, Metadata *)));
            vts->Run(title, item);
        }
        else
        {
            typedef SearchListHandler::item_list item_list;
            item_list videos;
            videos.push_back(item_list::value_type(video_uid, title));
            OnVideoSearchByTitleDoneNoBackground(true, videos, item);
        }
    }

    void VideoManagerImp::OnVideoSearchByTitleDone(bool normal_exit,
            const SearchListHandler::item_list &results, Metadata *item)
    {
        StopWaitBackground();
        OnVideoSearchByTitleDoneNoBackground(normal_exit, results, item);
    }

    // Called when there is no wait background to destroy.
    void VideoManagerImp::OnVideoSearchByTitleDoneNoBackground(bool normal_exit,
            const SearchListHandler::item_list &results, Metadata *item)
    {
        (void) normal_exit;
        VERBOSE(VB_IMPORTANT,
                QString("GetVideoList returned %1 possible matches")
                .arg(results.size()));

        if (results.size() == 1)
        {
            // Only one search result, fetch data.
            if (results.front().first.length() == 0)
            {
                ResetItem(item);
                OnSelectedItemChange();
                return;
            }
            StartVideoSearchByUID(results.front().first, item);
        }
        else
        {
            SearchListHandler *slh =
                    new SearchListHandler(this, m_vm, *m_theme, results,
                                          m_has_manual_title_search);
            connect(slh, SIGNAL(SigItemSelected(const QString &,
                                                const QString &)),
                    SLOT(OnVideoSearchListSelection(const QString &,
                                                    const QString &)));
            connect(slh, SIGNAL(SigCancel()), SLOT(OnVideoSearchListCancel()));
            connect(slh, SIGNAL(SigReset()), SLOT(OnVideoSearchListReset()));
            connect(slh, SIGNAL(SigManual()), SLOT(OnVideoSearchListManual()));
            connect(slh, SIGNAL(SigManualTitle()),
                    SLOT(OnVideoSearchListManualTitle()));

            m_event_dispatch.push(slh);
        }
    }

    void VideoManagerImp::OnParentalChange(int amount)
    {
        Metadata *item = m_list_handler->GetCurrentItem();
        if (item)
        {
            ParentalLevel curshowlevel = item->ShowLevel();

            curshowlevel += amount;

            if (curshowlevel.GetLevel() != item->ShowLevel())
            {
                item->setShowLevel(curshowlevel.GetLevel());
                item->updateDatabase();
                RefreshVideoList(true);
                OnSelectedItemChange();
            }
        }
    }

    void VideoManagerImp::DoManualVideoUID()
    {
        CancelPopup();
        ManualSearchUIDHandler *muidh =
                new ManualSearchUIDHandler(this, m_vm, *m_theme);
        connect(muidh, SIGNAL(SigTextChanged(const QString &)),
                SLOT(OnManualVideoUID(const QString &)));

        m_event_dispatch.push(muidh);
    }

    void VideoManagerImp::OnManualVideoUID(const QString &video_uid)
    {
        if (video_uid.length())
        {
            StartVideoSearchByUID(video_uid, m_list_handler->GetCurrentItem());
        }
    }

    void VideoManagerImp::DoManualVideoTitle()
    {
        CancelPopup();
        ManualSearchHandler *msh =
                new ManualSearchHandler(this, m_vm, *m_theme);
        connect(msh, SIGNAL(SigTextChanged(const QString &)),
                SLOT(OnManualVideoTitle(const QString &)));

        m_event_dispatch.push(msh);
    }

    void VideoManagerImp::OnManualVideoTitle(const QString &title)
    {
        Metadata *item = m_list_handler->GetCurrentItem();
        if (title.length() && item)
        {
            StartVideoSearchByTitle(VIDEO_INETREF_DEFAULT, title, item);
        }
    }

    void VideoManagerImp::DoEditMetadata()
    {
        CancelPopup();

        Metadata *item = m_list_handler->GetCurrentItem();
        if (!item) return;

        EditMetadataDialog *md_editor = new EditMetadataDialog(
                item, m_video_list->getListCache(), gContext->GetMainWindow(),
                "edit_metadata", "video-", "edit metadata dialog");

        md_editor->exec();
        delete md_editor;

        RefreshVideoList(false);

        OnSelectedItemChange();
    }

    void VideoManagerImp::DoRemoveVideo()
    {
        CancelPopup();

        Metadata *item = m_list_handler->GetCurrentItem();

        if (item)
        {
            bool okcancel;
            MythPopupBox *confirmationDialog =
                    new MythPopupBox(gContext->GetMainWindow());
            okcancel = confirmationDialog->showOkCancelPopup(
                    gContext->GetMainWindow(), "", tr("Delete this file?"),
                    false);

            if (okcancel)
            {
                if (m_video_list->Delete(item->ID()))
                    RefreshVideoList(false);
                else
                    confirmationDialog->showOkPopup(gContext->GetMainWindow(),
                            "", tr("delete failed"));
            }

            confirmationDialog->deleteLater();
        }
    }

    void VideoManagerImp::OnVideoMenuDone()
    {
        if (m_popup)
        {
            CancelPopup();
        }
    }

    void VideoManagerImp::DoResetMetadata()
    {
        CancelPopup();

        Metadata *item = m_list_handler->GetCurrentItem();
        if (item)
        {
            ResetItem(item);

            QString cover_file;
            QStringList search_dirs;
            search_dirs += m_art_dir;
            if (GetLocalVideoPoster(item->InetRef(), item->Filename(),
                            search_dirs, cover_file))
            {
                item->setCoverFile(cover_file);
                item->updateDatabase();
                RefreshVideoList(true);
            }

            OnSelectedItemChange();
        }
    }

    void VideoManagerImp::DoFilter()
    {
        CancelPopup();

        m_video_list->getFilterChangedState(); // clear any state sitting around
        BasicFilterSettingsProxy<VideoList> sp(*m_video_list);
        VideoFilterDialog *vfd =
                new VideoFilterDialog(&sp, gContext->GetMainWindow(),
                                      "filter", "video-", *m_video_list,
                                      "Video Filter Dialog");
        vfd->exec();
        delete vfd;

        unsigned int filter_state = m_video_list->getFilterChangedState();
        if (filter_state & VideoFilterSettings::FILTER_MASK)
        {
            RefreshVideoList(false);
        }
        else if (filter_state & VideoFilterSettings::kSortOrderChanged)
        {
            RefreshVideoList(true);
        }
    }
}; // mythvideo_videomanager namespace

VideoManager::VideoManager(MythMainWindow *lparent, VideoList *video_list) :
    MythThemedDialog(lparent, "manager", "video-", "video manager")
{
    m_imp.reset(new mythvideo_videomanager::VideoManagerImp(this, getTheme(),
                    QRect(0, 0, size().width(), size().height()),
                    video_list));
    buildFocusList();
    assignFirstFocus();
}

VideoManager::~VideoManager()
{
}

void VideoManager::keyPressEvent(QKeyEvent *event_)
{
    bool handled = false;

    QStringList actions;
    gContext->GetMainWindow()->TranslateKeyPress("Video", event_, actions);

    for (QStringList::const_iterator p = actions.begin();
            p != actions.end() && !handled; ++p)
    {
        mythvideo_videomanager::CEKeyPress kp(*p);
        m_imp->DispatchEvent(kp);
        handled = kp.GetHandled();
    }

    if (!handled)
        MythThemedDialog::keyPressEvent(event_);
}

void VideoManager::ExitWin()
{
        emit accept();
}

#include "videomanager.moc"
