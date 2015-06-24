//! \file
//! \brief File browser allowing multiple selections

#ifndef MYTHUIMULTIFILEBROWSER_H
#define MYTHUIMULTIFILEBROWSER_H

#include <QSet>

#include "mythuifilebrowser.h"


//! File browser allowing multiple selections
class MUI_PUBLIC MythUIMultiFileBrowser : public MythUIFileBrowser
{
    Q_OBJECT
public:
    MythUIMultiFileBrowser(MythScreenStack *parent, const QString &startPath);

    bool Create(void);

protected slots:
    void OKPressed(void);
    void backPressed(void);
    void homePressed(void);
    void selectPressed(void);
    void clearPressed(void);
    void PathClicked(MythUIButtonListItem *item);

protected:
    void updateFileList(void);

    QSet<QString> m_selected;
    MythUIButton  *m_selectButton;
    MythUIButton  *m_clearButton;
    MythUIText    *m_selectCount;
};

#endif // MYTHUIMULTIFILEBROWSER_H
