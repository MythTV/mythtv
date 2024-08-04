//! \file
//! \brief The info/details overlay

#ifndef PROG_INFO_LIST_H
#define PROG_INFO_LIST_H

#include "libmythui/mythuibuttonlist.h"

class MythScreenType;

//! The info/details buttonlist overlay that displays key:data info
class ProgInfoList : QObject
{
    Q_OBJECT

  public:
    enum VisibleLevel : std::uint8_t { kNone = 0, kLevel1 = 1, kLevel2 = 2 };

    // name, data, level
    using DataItem = std::tuple<QString, QString, int>;
    using DataList = QList<DataItem>;

  public:
    explicit ProgInfoList(MythScreenType &screen)
        : m_screen(screen) {}

    bool             Create(bool focusable);
    void             Toggle(void);
    bool             Hide(void);
    void             Display(const DataList& data);
    VisibleLevel     GetLevel(void) const   { return m_infoVisible; }

    void PageDown(void) { m_btnList->MoveDown(MythUIButtonList::MovePage); }
    void PageUp(void)   { m_btnList->MoveUp(MythUIButtonList::MovePage); }

  private slots:
    void Clear()   { m_btnList->Reset(); }

  private:
    void CreateButton(const QString &name, const QString &value);

    MythScreenType   &m_screen;                //!< Parent screen
    MythUIButtonList *m_btnList     {nullptr}; //!< Overlay buttonlist
    VisibleLevel      m_infoVisible {kLevel1}; //!< Info list state
};

#endif // PROG_INFO_LIST_H
