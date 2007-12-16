#include <qapplication.h>
#include <qstringlist.h>
#include <qpixmap.h>
#include <qdir.h>
#include <qurloperator.h>
#include <qfileinfo.h>
#include <qprocess.h>
#include <qpainter.h>

#include <unistd.h>
#include <cstdlib>

#include <memory>
#include <list>
#include <stack>
#include <set>

#include <mythtv/mythcontext.h>
#include <mythtv/xmlparse.h>
#include <mythtv/compat.h>

#include "globals.h"
#include "videomanager.h"
#include "videolist.h"

#include "videofilter.h"
#include "metadata.h"
#include "editmetadata.h"
#include "videoutils.h"
#include "metadatalistmanager.h"

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

        struct lb_data
        {
            unsigned int begin;
            unsigned int end; // one past the end
            unsigned int index;
            unsigned int item_index;
            bool data_above_window; // true if items "before" window
            bool data_below_window; // true if items "past" window
        };

      public:
        ListBehaviorManager(unsigned int window_size = 0,
                               int behavior = lbNone,
                               unsigned int item_count = 0) :
            m_window_size(window_size), m_item_count(item_count), m_index(0),
            m_skip_index(SKIP_MAX)
        {
            m_scroll_center = behavior & lbScrollCenter;
            m_wrap_list = behavior & lbWrapList;
        }

        void setWindowSize(unsigned int window_size)
        {
            m_window_size = window_size;
        }

        unsigned int getWindowSize() const
        {
            return m_window_size;
        }

        void setItemCount(unsigned int item_count)
        {
            m_item_count = item_count;
        }

        unsigned int getItemCount() const
        {
            return m_item_count;
        }

        const lb_data &setIndex(unsigned int index)
        {
            m_index = index;
            return update_data();
        }

        unsigned int getIndex() const
        {
            return m_index;
        }

        void setSkipIndex(unsigned int skip = SKIP_MAX)
        {
            m_skip_index = skip;
        }

        const lb_data &move_up()
        {
            if (m_index)
                --m_index;
            else if (m_wrap_list)
                m_index = m_item_count - 1;
            return update_data(mdUp);
        }

        const lb_data &move_down()
        {
            ++m_index;
            return update_data(mdDown);
        }

        const lb_data &getData()
        {
            return update_data();
        }

        const lb_data &page_up()
        {
            if (m_index == 0)
                m_index = m_item_count - 1;
            else if (m_index < m_window_size - 1)
                m_index = 0;
            else
                m_index -= m_window_size;
            return update_data(mdUp);
        }

        const lb_data &page_down()
        {
            if (m_index == m_item_count - 1)
                m_index = 0;
            else if (m_index + m_window_size > m_item_count - 1)
                m_index = m_item_count - 1;
            else
                m_index += m_window_size;
            return update_data(mdDown);
        }

      private:
        enum movement_direction
        {
            mdNone,
            mdUp,
            mdDown
        };

        const lb_data &update_data(movement_direction direction = mdNone)
        {
            if (m_item_count <= 1)
            {
                m_index = 0;
                m_data.begin = 0;
                m_data.end = m_item_count;
                m_data.index = 0;
                m_data.item_index = 0;
                m_data.data_below_window = false;
                m_data.data_above_window = false;
            }
            else if (m_item_count)
            {
                const unsigned int last_item_index = m_item_count - 1;

                if (m_skip_index != SKIP_MAX && m_index == m_skip_index)
                {
                    if (direction == mdDown)
                        ++m_index;
                    else if (direction == mdUp)
                        --m_index;
                }

                if (m_wrap_list)
                {
                    if (m_index > last_item_index)
                    {
                        m_index = 0;
                    }
                }
                else
                {
                    if (m_index > last_item_index)
                    {
                        m_index = last_item_index;
                    }
                }

                const unsigned int max_window_size = m_window_size - 1;

                if (m_scroll_center && m_item_count > max_window_size)
                {
                    unsigned int center_buffer = m_window_size / 2;
                    if (m_index < center_buffer)
                    {
                        m_data.begin = 0;
                        m_data.end = std::min(last_item_index, max_window_size);
                    }
                    else if (m_index > m_item_count - center_buffer)
                    {
                        m_data.begin = last_item_index - max_window_size;
                        m_data.end = last_item_index;
                    }
                    else
                    {
                        m_data.begin = m_index - center_buffer;
                        m_data.end = m_index + center_buffer - 1;
                    }
                }
                else
                {
                    if (m_index <= max_window_size)
                    {
                        m_data.begin = 0;
                        m_data.end = std::min(last_item_index, max_window_size);
                    }
                    else
                    {
                        m_data.begin = m_index - max_window_size;
                        m_data.end = m_index;
                    }
                }
                m_data.index = m_index - m_data.begin;

                m_data.data_below_window = m_data.end < m_item_count - 1;
                m_data.data_above_window = m_data.begin != 0;
                m_data.item_index = m_index;
                if (m_data.end != 0) ++m_data.end;
            }

            return m_data;
        }

      private:
        unsigned int m_window_size;
        unsigned int m_item_count;
        unsigned int m_index;
        unsigned int m_skip_index;

        static const unsigned int SKIP_MAX = -1;

        bool m_scroll_center;
        bool m_wrap_list;
        lb_data m_data;
    };

    const unsigned int ListBehaviorManager::SKIP_MAX;

    void DrawContainer(QPainter &painter, LayerSet *container, int context = 0)
    {
        for (int i = 0; i < container->getLayers() + 1; ++i)
        {
            container->Draw(&painter, i, context);
        }
    }

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
        enum MyType { etContainerDone = 311976 };
        ContainerDoneEvent() : QEvent(static_cast<Type>(etContainerDone)) {}
    };

    class ContainerHandler : public QObject
    {
        Q_OBJECT

      public:
        enum HandlerFlag
        {
            ehfPaint = 0x1, // Stop paint events from processing
            ehfCanTakeFocus = 0x1 << 1 // Container can take focus
        };

        typedef QWidget ParentWindowType;

        enum ExitType
        {
            etSuccess,
            etFailure
        };

      public:
        ContainerHandler(QObject *oparent, ParentWindowType *pwt,
                XMLParse &theme, const QString &container_name,
                unsigned int flags = ehfPaint | ehfCanTakeFocus) :
            QObject(oparent),
            m_container(0), m_theme(&theme), m_pwt(pwt), m_done(false),
            m_container_name(container_name),
            m_flags(flags), m_exit_type(etFailure)
        {
            if (m_theme)
            {
                m_container = m_theme->GetSet(m_container_name);
                if (m_container)
                    m_rect = m_container->GetAreaRect();
                else
                {
                    VERBOSE(VB_IMPORTANT,
                            QString("MythVideo: VideoManager : Failed to "
                                    "get %1 object.").arg(m_container_name));
                }
            }
        }

        // returns true if the underlying container exists
        bool Exists() const
        {
            return m_container != 0;
        }

        void Success()
        {
            SetDone(true, etSuccess);
        }

        void Failure()
        {
            SetDone(true, etFailure);
        }

        unsigned int SetFlags(unsigned int flags)
        {
            m_flags = flags;
            return m_flags;
        }

        unsigned int GetFlags() const { return m_flags; }

        unsigned int SetFlag(HandlerFlag flag)
        {
            m_flags |= flag;
            return m_flags;
        }

        unsigned int ClearFlag(HandlerFlag flag)
        {
            m_flags &= ~flag;
            return m_flags;
        }

        ParentWindowType *GetParentWindow()
        {
            return m_pwt;
        }

        const QRect &GetRect() const { return m_rect; }

        // returns true if action handled
        virtual bool KeyPress(const QString &action, QKeyEvent *raw_event)
        {
            (void) raw_event;
            if (action == "ESCAPE")
            {
                SetDone(true, etFailure);
                return true;
            }
            return false;
        }

        virtual void Paint(QPainter &painter, const QRect &rel_inv_r) = 0;

        const QString &GetName() const { return m_container_name; }

        // Invalidates the rect of this container
        void Invalidate(const QRect &rect)
        {
            m_pwt->update(rect.left(), rect.top(), rect.width(), rect.height());
        }

        void Invalidate()
        {
            Invalidate(m_rect);
        }

        bool IsDone() const { return m_done; }

        // Called after the container has done Success() or Failure()
        // and before this object is destroyed.
        void DoExit()
        {
            OnExit(m_exit_type);
        }

      protected:
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
            Invalidate();
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
        enum EventType { cetNone, cetPaint, cetKeyPress };

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
        CEKeyPress(const QString &action, const QKeyEvent *raw_event) :
            ContainerEvent(ContainerEvent::cetKeyPress), m_action(action),
            m_raw_event(0)
        {
            if (raw_event)
            {
                m_raw_event = new QKeyEvent(raw_event->type(),
                                            raw_event->key(),
                                            raw_event->ascii(),
                                            raw_event->state(),
                                            raw_event->text(),
                                            raw_event->isAutoRepeat(),
                                            raw_event->count());
            }
        }

        ~CEKeyPress()
        {
            delete m_raw_event;
        }

        void Do(ContainerHandler *handler)
        {
            SetHandled(handler->KeyPress(m_action, m_raw_event));
        }

      private:
        CEKeyPress(const CEKeyPress &rhs); // no copies

      private:
        QString m_action;
        QKeyEvent *m_raw_event;
    };

    struct CEPaint : public ContainerEvent
    {
        static const bool AEW_debug_paint = false;

        CEPaint(QPainter &painter, const QRect &inv_rect) :
            ContainerEvent(ContainerEvent::cetPaint), m_painter(&painter),
            m_inv_rect(inv_rect)
        {
        }

        void Do(ContainerHandler *handler)
        {
            if (AEW_debug_paint)
            {
                QPen orig_pen = m_painter->pen();
                QBrush orig_brush = m_painter->brush();

                m_painter->setBrush(QBrush());
                m_painter->setPen(QPen(QColor(0, 255, 0), 1));
                m_painter->drawRect(m_inv_rect);

                m_painter->setPen(orig_pen);
                m_painter->setBrush(orig_brush);
            }

            handler->Paint(*m_painter, m_inv_rect);

        }

        const QRect &GetInvRect() const { return m_inv_rect; }
        QPainter &GetPainter() { return *m_painter; }

      private:
        QPainter *m_painter;
        QRect m_inv_rect;
    };

    // Dispatch container events to the top container. If there
    // is no top container it dispatches the events to the base
    // handler.
    template <typename HandlerType>
    class ContainerDispatch
    {
      public:
        ContainerDispatch(QObject *event_dest, QWidget *canvas) :
            m_event_dest(event_dest), m_canvas(canvas), m_focus(0)
        {
        }

        void push(HandlerType *handler)
        {
            m_handlers.push_back(handler);
            adjust_focus();
        }

        HandlerType *pop()
        {
            HandlerType *ret = 0;
            if (m_handlers.size())
            {
                ret = m_handlers.back();
                m_handlers.pop_back();
            }
            adjust_focus();

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
                    case ContainerEvent::cetPaint:
                    {
                        // implemented below
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

                if (event.GetType() == ContainerEvent::cetPaint)
                {
                    // Special dispatch for paint events
                    // they are sent to every container in the invalidated
                    // rect.

                    QPixmap container_image(m_canvas->geometry().size());
                    container_image.fill(m_canvas, 0, 0);
                    QPainter painter(&container_image);

                    CEPaint *paint_event = dynamic_cast<CEPaint *>(&event);

                    const QRect &invr = paint_event->GetInvRect();

                    for (typename handlers::iterator p = m_handlers.begin();
                            p != m_handlers.end(); ++p)
                    {
                        if ((*p)->GetFlags() & HandlerType::ehfPaint)
                        {
                            const QRect hrect = (*p)->GetRect();
                            const QRect isect = invr.intersect(hrect);

                            if (!isect.isEmpty())
                            {
                                bool or_clipped = painter.hasClipping();
                                QRegion or_clip_rgn = painter.clipRegion();
                                painter.setClipRect(isect);

                                // ugly
                                QRect wr(isect);
                                wr.moveTopLeft(isect.topLeft() -
                                               hrect.topLeft());
                                painter.translate((*p)->GetRect().left(),
                                                  (*p)->GetRect().top());
                                CEPaint pe(painter, wr);
                                pe.Do(*p);
                                painter.resetXForm();

                                painter.setClipRegion(or_clip_rgn);
                                painter.setClipping(or_clipped);
                            }
                        }
                    }
                    painter.end();
                    paint_event->GetPainter().
                            drawPixmap(invr.topLeft(), container_image, invr);
                }
                else if (do_dispatch)
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

        void adjust_focus()
        {
            m_focus = 0;

            if (m_handlers.size())
            {
                for (typename handlers::reverse_iterator p =
                        m_handlers.rbegin(); p != m_handlers.rend(); ++p)
                {
                    if ((*p)->GetFlags() & ContainerHandler::ehfCanTakeFocus)
                    {
                        m_focus = *p;
                        break;
                    }
                }
            }
        }

        void do_container_cleanup()
        {
            bool refocus = false;
            for (typename handlers::iterator p = m_handlers.begin();
                    p != m_handlers.end();)
            {
                // The handler could be done, if it is, remove it and
                // mark it for deletion.
                if ((*p)->IsDone())
                {
                    refocus = true;
                    (*p)->DoExit();
                    m_canvas->update((*p)->GetRect());
                    (*p)->deleteLater();

                    p = m_handlers.erase(p);
                }
                else
                    ++p;
            }

            if (refocus)
                adjust_focus();
        }

      private:
        typedef std::list<HandlerType *> handlers;

      private:
        QObject *m_event_dest;
        QWidget *m_canvas;
        handlers m_handlers;
        HandlerType *m_focus;
    };

    // handlers
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
            ContainerHandler(oparent, pwt, theme, "moviesel"),
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
                m_list = dynamic_cast<UIListType *>(m_container->
                                                    GetType("listing"));
                if (m_list)
                {
                    m_list_behave.setWindowSize(m_list->GetItems());
                    m_list_behave.setItemCount(m_item_list.size());

                    if (initial_size)
                        m_list_behave.setSkipIndex(initial_size + 1);
                }
            }
        }

        bool KeyPress(const QString &action, QKeyEvent *raw_event)
        {
            bool ret = true;
            if (action == "SELECT")
            {
                Success();
            }
            else if (action == "UP")
            {
                m_list_behave.move_up();
                Invalidate();
            }
            else if (action == "DOWN")
            {
                m_list_behave.move_down();
                Invalidate();
            }
            else if (action == "PAGEUP")
            {
                m_list_behave.page_up();
                Invalidate();
            }
            else if (action == "PAGEDOWN")
            {
                m_list_behave.page_down();
                Invalidate();
            }
            else
            {
                ret = ContainerHandler::KeyPress(action, raw_event);
            }

            return ret;
        }

        void Paint(QPainter &painter, const QRect &rel_inv_r)
        {
            (void) rel_inv_r;
            if (m_container)
            {
                if (m_list)
                {
                    m_list->ResetList();
                    m_list->SetActive(true);

                    const ListBehaviorManager::lb_data &lbd =
                            m_list_behave.getData();
                    for (unsigned int i = lbd.begin; i < lbd.end; ++i)
                    {
                        m_list->SetItemText(i, 1, m_item_list.at(i).second);
                    }

                    m_list->SetItemCurrent(lbd.index);
                    m_list->SetDownArrow(lbd.data_below_window);
                    m_list->SetUpArrow(lbd.data_above_window);
                }

                DrawContainer(painter, m_container);
            }
        }

      private:
        void OnExit(ExitType et)
        {
            if (et == etSuccess)
            {
                const item_list::value_type sel_item =
                        m_item_list.at(m_list_behave.getData().item_index);

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

    class ManualSearchUIDHandler : public ContainerHandler
    {
        Q_OBJECT

      signals:
        void SigTextChanged(const QString &uid);

      public:
        ManualSearchUIDHandler(QObject *oparent, ParentWindowType *pwt,
                          XMLParse &theme) :
            ContainerHandler(oparent, pwt, theme, "enterimdb")
        {
        }

        bool KeyPress(const QString &action, QKeyEvent *raw_event)
        {
            bool ret = true;
            bool converted = false;
            const int as_int = action.toInt(&converted);

            if (action == "SELECT")
            {
                Success();
            }
            else if (converted && as_int >= 0 && as_int <= 9)
            {
                m_number += action;
                Invalidate();
            }
            else if (action == "DELETE")
            {
                // This uncomfortable mapping made necessary
                // by only passing translated actions.
                m_number = m_number.left(m_number.length() - 1);
                Invalidate();
            }
            else
            {
                ret = ContainerHandler::KeyPress(action, raw_event);
            }

            return ret;
        }

        void Paint(QPainter &painter, const QRect &rel_inv_r)
        {
            (void) rel_inv_r;
            if (m_container)
            {
                checkedSetText(m_container, "numhold", m_number);
                DrawContainer(painter, m_container);
            }
        }

      private:
        void OnExit(ExitType et)
        {
            if (et == etSuccess)
            {
                emit SigTextChanged(m_number);
            }
        }

      private:
        QString m_number;
    };

    class ManualSearchHandler : public ContainerHandler
    {
        Q_OBJECT

      signals:
        void SigTextChanged(const QString &text);

      public:
        ManualSearchHandler(QObject *oparent, ParentWindowType *pwt,
                            XMLParse &theme) :
            ContainerHandler(oparent, pwt, theme, "entersearchtitle")
        {
        }

        bool KeyPress(const QString &action, QKeyEvent *raw_event)
        {
            // TODO: ick ick ick
            bool ret = true;
            if (action == "SELECT")
            {
                if (raw_event && raw_event->key() == Qt::Key_Space)
                {
                    m_title += raw_event->text();
                }
                else
                    Success();
            }
            else if (action == "DELETE")
            {
                m_title = m_title.left(m_title.length() - 1);
            }
            else if (!action.length() && raw_event)
            {
                ret = false;
                switch (raw_event->key())
                {
                    case Qt::Key_Return:
                    case Qt::Key_Enter:
                    case Qt::Key_Escape:
                        break;
                    case Qt::Key_Backspace:
                    case Qt::Key_Delete:
                    {
                        m_title = m_title.left(m_title.length() - 1);
                        ret = true;
                        break;
                    }
                    default:
                    {
                        m_title += raw_event->text();
                        ret = true;
                        break;
                    }
                }
            }
            else
            {
                ret = ContainerHandler::KeyPress(action, raw_event);
            }

            if (ret)
                Invalidate();

            return ret;
        }

        void Paint(QPainter &painter, const QRect &rel_inv_r)
        {
            (void) rel_inv_r;
            if (m_container)
            {
                checkedSetText(m_container, "title", m_title);
                DrawContainer(painter, m_container);
            }
        }

      private:
        void OnExit(ExitType et)
        {
            if (et == etSuccess)
            {
                emit SigTextChanged(m_title);
            }
        }

      private:
        QString m_title;
    };

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
            ContainerHandler(oparent, pwt, theme, "info", ehfPaint),
            m_art_dir(art_dir), m_item_get(item_get)
        {
        }

        void Paint(QPainter &painter, const QRect &rel_inv_r)
        {
            (void) rel_inv_r;
            const Metadata *item = m_item_get->GetItem();

            if (item)
            {
               if (m_container)
               {
                   checkedSetText(m_container, "title", item->Title());
                   checkedSetText(m_container, "filename",
                                  item->getFilenameNoPrefix());
                   checkedSetText(m_container, "video_player",
                                  Metadata::getPlayer(item));
                   checkedSetText(m_container, "director", item->Director());
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

                   DrawContainer(painter, m_container);
               }
               //allowselect = true;
            }
            else
            {
               LayerSet *norec = m_theme->GetSet("novideos_info");
               if (norec)
               {
                   DrawContainer(painter, norec);
               }

               //allowselect = false;
            }
        }

      private:
        QString m_art_dir;
        CurrentInfoItemGetter *m_item_get;
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
            ContainerHandler(oparent, pwt, theme, "selector"),
            m_list_behave(0, ListBehaviorManager::lbScrollCenter |
                          ListBehaviorManager::lbWrapList),
            m_video_list(video_list)
        {
            m_list =
                    dynamic_cast<UIListType *>(m_container->GetType("listing"));
            if (m_list)
                m_list_behave.setWindowSize(m_list->GetItems());
        }

        void SetSelectedItem(unsigned int index)
        {
            m_list_behave.setIndex(index);
        }

        // called then the underlying list may have changed
        void OnListChanged()
        {
            m_list_behave.setItemCount(m_video_list->count());
            emit SigSelectionChanged();
        }

        Metadata *GetCurrentItem()
        {
            return m_video_list->getVideoListMetadata(m_list_behave.getData().
                    item_index);
        }

        bool KeyPress(const QString &action, QKeyEvent *raw_event)
        {
            (void) raw_event;
            bool ret = true;
            const unsigned int curindex = m_list_behave.getIndex();

            if (action == "SELECT")
                emit SigItemEdit();
            else if (action == "UP")
                m_list_behave.move_up();
            else if (action == "DOWN")
                m_list_behave.move_down();
            else if (action == "PAGEUP")
                m_list_behave.page_up();
            else if (action == "PAGEDOWN")
                m_list_behave.page_down();
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
            if (curindex != m_list_behave.getIndex())
            {
                emit SigSelectionChanged();
                Invalidate();
            }

            return ret;
        }

        void Paint(QPainter &painter, const QRect &rel_inv_r)
        {
            (void) rel_inv_r;
            if (m_container)
            {
                if (m_list)
                {
                    m_list->ResetList();
                    m_list->SetActive(true);

                    const ListBehaviorManager::lb_data &lbd =
                            m_list_behave.getData();
                    for (unsigned int i = lbd.begin; i < lbd.end; ++i)
                    {
                        Metadata *meta = m_video_list->getVideoListMetadata(i);

                        QString title = meta->Title();
                        QString filename = meta->Filename();
                        if (0 == title.compare("title"))
                        {
                            title = filename.section('/', -1);
                            if (!gContext->GetNumSetting("ShowFileExtensions"))
                                title = title.section('.',0,-2);
                        }

                        m_list->SetItemText(i - lbd.begin, 1, title);
                        m_list->SetItemText(i - lbd.begin, 2, meta->Director());
                        m_list->SetItemText(i - lbd.begin, 3,
                                           getDisplayYear(meta->Year()));
                    }

                    m_list->SetItemCurrent(lbd.index);
                    m_list->SetDownArrow(lbd.data_below_window);
                    m_list->SetUpArrow(lbd.data_above_window);
                }

                DrawContainer(painter, m_container);
            }
        }

      private:
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

      signals:
//        void SigWaitCanceled();

      public:
        WaitBackgroundHandler(QObject *oparent, ParentWindowType *pwt,
                XMLParse &theme) :
            ContainerHandler(oparent, pwt, theme, "inetwait")
        {
        }

        void EnterMessage(const QString &message)
        {
            m_message.push(message);
            Invalidate();
        }

        bool LeaveMessage()
        {
            m_message.pop();
            bool more = m_message.size();
            if (more)
                Invalidate();

            return more;
        }

        void Close()
        {
            Success();
        }

        bool KeyPress(const QString &action, QKeyEvent *raw_event)
        {
            (void) action;
            (void) raw_event;
//            if (action == "ESCAPE" || action == "LEFT")
//            {
//                Failure();
//                emit SigWaitCanceled();
//            }

            // not interruptible for now
            return true;
        }

        void Paint(QPainter &painter, const QRect &rel_inv_r)
        {
            (void) rel_inv_r;
            // set the title for the wait background
            if (m_container && m_message.size())
            {
                checkedSetText(m_container, "title", m_message.top());

                DrawContainer(painter, m_container);
            }
        }

      private:
        std::stack<QString> m_message;
    };

    class BackgroundHandler : public ContainerHandler
    {
        Q_OBJECT

      public:
        BackgroundHandler(QObject *oparent, ParentWindowType *pwt,
                          XMLParse &theme) :
            ContainerHandler(oparent, pwt, theme, "background", 0)
        {
            QPixmap pix(GetParentWindow()->geometry().size());
            pix.fill(GetParentWindow(), 0, 0);
            QPainter painter(&pix);
            if (m_container)
            {
                DrawContainer(painter, m_container);
            }
            painter.end();
            GetParentWindow()->setPaletteBackgroundPixmap(pix);
        }

        bool KeyPress(const QString &action, QKeyEvent *raw_event)
        {
            (void) action;
            (void) raw_event;
            return false;
        }

        void Paint(QPainter &painter, const QRect &rel_inv_r)
        {
            (void) painter;
            (void) rel_inv_r;
        }
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
        QProcess m_process;
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

      private:
        VideoTitleSearch(QObject *oparent) : ExecuteExternalCommand(oparent),
            m_item(0)
        {
        }

        ~VideoTitleSearch() {}

      public:
        static VideoTitleSearch *Create(QObject *oparent)
        {
            return new VideoTitleSearch(oparent);
        }

        void Run(const QString &title, Metadata *item)
        {
            m_item = item;

            QString def_cmd = QDir::cleanDirPath(QString("%1/%2")
                    .arg(gContext->GetShareDir())
                    .arg("mythvideo/scripts/imdb.pl -M tv=no;video=no"));

            QString cmd = gContext->GetSetting("MovieListCommandLine", def_cmd);

            QStringList args;
            args += title;
            StartRun(cmd, args, "Video Search");
        }

      private:
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
      private:
        VideoUIDSearch(QObject *oparent) : ExecuteExternalCommand(oparent),
            m_item(0)
        {
        }

        ~VideoUIDSearch() {}

      public:
        static VideoUIDSearch *Create(QObject *oparent)
        {
            return new VideoUIDSearch(oparent);
        }

        void Run(const QString &video_uid, Metadata *item)
        {
            m_item = item;
            m_video_uid = video_uid;

            const QString def_cmd = QDir::cleanDirPath(QString("%1/%2")
                    .arg(gContext->GetShareDir())
                    .arg("mythvideo/scripts/imdb.pl -D"));
            const QString cmd = gContext->GetSetting("MovieDataCommandLine",
                                                     def_cmd);

            StartRun(cmd, video_uid, "Video Data Query");
        }

      private:
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

      private:
        VideoPosterSearch(QObject *oparent) : ExecuteExternalCommand(oparent),
            m_item(0)
        {
        }

        ~VideoPosterSearch() {}

      public:
        static VideoPosterSearch *Create(QObject *oparent)
        {
            return new VideoPosterSearch(oparent);
        }

        void Run(const QString &video_uid, Metadata *item)
        {
            m_item = item;

            const QString default_cmd =
                    QDir::cleanDirPath(QString("%1/%2")
                                       .arg(gContext->GetShareDir())
                                       .arg("mythvideo/scripts/imdb.pl -P"));
            const QString cmd = gContext->GetSetting("MoviePosterCommandLine",
                                                     default_cmd);
            StartRun(cmd, video_uid, "Poster Query");
        }

      private:
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
        void SigFinished(QNetworkOperation *op, Metadata *item);

      public:
        URLOperationProxy() : m_item(0)
        {
            connect(&m_url_op, SIGNAL(finished(QNetworkOperation *)),
                    SLOT(OnFinished(QNetworkOperation *)));
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
        void OnFinished(QNetworkOperation *op)
        {
            emit SigFinished(op, m_item);
        }

      private:
        Metadata *m_item;
        QUrlOperator m_url_op;
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

      public:
        VideoManagerImp(VideoManager *vm, double wmult, double hmult,
                        const QRect &area, VideoList *video_list) :
            m_event_dispatch(this, vm), m_vm(vm), m_area(area),
            m_video_list(video_list), m_info_handler(0), m_list_handler(0),
            m_background_handler(0), m_popup(0),
            m_wait_background(0), m_has_manual_title_search(false)
        {
            m_theme.SetWMult(wmult);
            m_theme.SetHMult(hmult);
            QDomElement xmldata;
            m_theme.LoadTheme(xmldata, "manager", "video-");
            LoadWindow(xmldata);

            m_art_dir = gContext->GetSetting("VideoArtworkDir");

            m_background_handler = new BackgroundHandler(this, m_vm, m_theme);
            m_info_handler = new InfoHandler(this, m_vm, m_theme,
                    &m_current_item_proxy, m_art_dir);
            m_list_handler = new ListHandler(this, m_vm, m_theme, video_list);

            m_current_item_proxy.connect(m_list_handler);

            m_vm->connect(m_list_handler, SIGNAL(ListHandlerExit()),
                         SLOT(ExitWin()));

            m_event_dispatch.push(m_background_handler);
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

            RefreshVideoList(false);

            m_has_manual_title_search =
                    ManualSearchHandler(0, m_vm, m_theme).Exists();
            connect(&m_url_dl_timer,
                    SIGNAL(SigTimeout(const QString &, Metadata *)),
                    SLOT(OnPosterDownloadTimeout(const QString &, Metadata *)));
            connect(&m_url_operator,
                    SIGNAL(SigFinished(QNetworkOperation *, Metadata *)),
                    SLOT(OnPosterCopyFinished(QNetworkOperation *,
                                              Metadata *)));
        }

        ~VideoManagerImp()
        {
            m_current_item_proxy.disconnect();
        }

        void Invalidate(const QRect &area)
        {
            m_vm->update(area);
        }

        void Invalidate()
        {
            Invalidate(m_area);
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

        bool event(QEvent *e)
        {
            if (e->type() == QEvent::Type(ContainerDoneEvent::etContainerDone))
            {
                m_event_dispatch.ProcessDone();
                return true;
            }

            return QObject::event(e);
        }

      private:
        void LoadWindow(QDomElement &element)
        {
            for (QDomNode chld = element.firstChild(); !chld.isNull();
                 chld = chld.nextSibling())
            {
                QDomElement e = chld.toElement();
                if (!e.isNull())
                {
                    if (e.tagName() == "font")
                    {
                        m_theme.parseFont(e);
                    }
                    else if (e.tagName() == "container")
                    {
                        QRect area;
                        QString container_name;
                        int context;
                        m_theme.parseContainer(e, container_name, context,
                                area);
                    }
                    else
                    {
                        VERBOSE(VB_IMPORTANT,
                                QString("Error: Unknown element: ").
                                arg(e.tagName()));
                        exit(0); // TODO bad
                    }
                }
            }
        }

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
            QStringList image_types = QImage::inputFormatList();

            typedef std::set<QString> image_type_list;
            image_type_list image_exts;

            for (QStringList::const_iterator it = image_types.begin();
                    it != image_types.end(); ++it)
            {
                image_exts.insert((*it).lower());
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
                        new WaitBackgroundHandler(this, m_vm, m_theme);
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
        }

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
        void OnPosterCopyFinished(QNetworkOperation *op, Metadata *item);
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

            QButton *editButton = NULL;
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

            QButton *filterButton =
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

        void OnListSelectionChange()
        {
            m_info_handler->Invalidate();
        }

        void OnSelectedItemChange()
        {
            m_info_handler->Invalidate();
            m_list_handler->Invalidate();
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

                Invalidate();
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
        ContainerDispatch<ContainerHandler> m_event_dispatch;
        VideoManager *m_vm;
        XMLParse m_theme;
        QRect m_area;
        VideoList *m_video_list;
        InfoHandler *m_info_handler;
        ListHandler *m_list_handler;
        BackgroundHandler *m_background_handler;
        MythPopupBox *m_popup;
        WaitBackgroundHandler *m_wait_background;
        CurrentItemGet m_current_item_proxy;
        QString m_art_dir;
        bool m_has_manual_title_search;
        URLOperationProxy m_url_operator;
        TimeoutSignalProxy m_url_dl_timer;
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

        if (!resort_only) // list size may have changed
            m_list_handler->OnListChanged();
        updateML = false;
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
        VideoPosterSearch *vps = VideoPosterSearch::Create(this);
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
                    fileprefix = MythContext::GetConfDir();

                    dir.setPath(fileprefix);
                    if (!dir.exists())
                        dir.mkdir(fileprefix);

                    fileprefix += "/MythVideo";
                }

                dir.setPath(fileprefix);
                if (!dir.exists())
                    dir.mkdir(fileprefix);

                QUrl url(uri);

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

    void VideoManagerImp::OnPosterCopyFinished(QNetworkOperation *op,
                                               Metadata *item)
    {
        QString state, operation;
        switch(op->operation())
        {
            case QNetworkProtocol::OpMkDir:
                operation = "MkDir";
                break;
            case QNetworkProtocol::OpRemove:
                operation = "Remove";
                break;
            case QNetworkProtocol::OpRename:
                operation = "Rename";
                break;
            case QNetworkProtocol::OpGet:
                operation = "Get";
                break;
            case QNetworkProtocol::OpPut:
                operation = "Put";
                break;
            default:
                operation = "Uknown";
                break;
        }

        switch(op->state())
        {
            case QNetworkProtocol::StWaiting:
                state = "The operation is in the QNetworkProtocol's queue "
                        "waiting to be prcessed.";
                break;
            case QNetworkProtocol::StInProgress:
                state = "The operation is being processed.";
                break;
            case QNetworkProtocol::StDone:
                state = "The operation has been processed succesfully.";
                break;
            case QNetworkProtocol::StFailed:
                state = "The operation has been processed but an error "
                        "occurred.";
                if (item)
                    item->setCoverFile("");
                break;
            case QNetworkProtocol::StStopped:
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
        VideoUIDSearch *vns = VideoUIDSearch::Create(this);
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

            // Genres
            Metadata::genre_list video_genres;
            QStringList genres = QStringList::split(",", data["Genres"]);

            for (QStringList::iterator p = genres.begin(); p != genres.end();
                    ++p)
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
            for (QStringList::iterator p = countries.begin();
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

            VideoTitleSearch *vts = VideoTitleSearch::Create(this);
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
            SearchListHandler *slh = new SearchListHandler(this, m_vm,
                    m_theme, results, m_has_manual_title_search);
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
            slh->Invalidate();
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
                new ManualSearchUIDHandler(this, m_vm, m_theme);
        connect(muidh, SIGNAL(SigTextChanged(const QString &)),
                SLOT(OnManualVideoUID(const QString &)));

        m_event_dispatch.push(muidh);
        muidh->Invalidate();
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
        ManualSearchHandler *msh = new ManualSearchHandler(this, m_vm, m_theme);
        connect(msh, SIGNAL(SigTextChanged(const QString &)),
                SLOT(OnManualVideoTitle(const QString &)));

        m_event_dispatch.push(msh);
        msh->Invalidate();
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
        Invalidate();
    }
}; // mythvideo_videomanager namespace

VideoManager::VideoManager(MythMainWindow *lparent,  const QString &lname,
                           VideoList *video_list) :
    MythDialog(lparent, lname)
{
    m_imp.reset(new mythvideo_videomanager::VideoManagerImp(this, wmult, hmult,
                    QRect(0, 0, size().width(), size().height()),
                    video_list));
    setNoErase();
}

VideoManager::~VideoManager()
{
}

void VideoManager::keyPressEvent(QKeyEvent *event_)
{
    // Get first attempt to raw events, so things like
    // space = space + SELECTED can be avoided if needed.
    mythvideo_videomanager::CEKeyPress pkp("", event_);
    m_imp->DispatchEvent(pkp);
    bool handled = pkp.GetHandled();

    if (!handled)
    {
        QStringList actions;
        gContext->GetMainWindow()->TranslateKeyPress("Video", event_, actions);

        for (QStringList::iterator p = actions.begin();
                p != actions.end() && !handled; ++p)
        {
            mythvideo_videomanager::CEKeyPress kp(*p, event_);
            m_imp->DispatchEvent(kp);
            handled = kp.GetHandled();
        }
    }

    if (!handled)
        MythDialog::keyPressEvent(event_);
}

void VideoManager::paintEvent(QPaintEvent *event_)
{
    QPainter p(this);
    mythvideo_videomanager::CEPaint pe(p, event_->rect());
    m_imp->DispatchEvent(pe);
}

void VideoManager::ExitWin()
{
        emit accept();
}

#include "videomanager.moc"
