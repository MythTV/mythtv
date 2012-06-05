#ifndef MYTHUI_BUTTONTREE_H_
#define MYTHUI_BUTTONTREE_H_

#include "mythuitype.h"
#include "mythgenerictree.h"
#include "mythuibuttonlist.h"

class MythUIButtonListItem;

/** \class MythUIButtonTree
 *
 * \brief A tree widget for displaying and navigating a MythGenericTree()
 *
 * \ingroup MythUI_Widgets
 */
class MUI_PUBLIC MythUIButtonTree : public MythUIType
{
    Q_OBJECT
  public:
    MythUIButtonTree(MythUIType *parent, const QString &name);
   ~MythUIButtonTree();

    virtual bool keyPressEvent(QKeyEvent *);
    virtual bool gestureEvent(MythGestureEvent *event);

    bool AssignTree(MythGenericTree *tree);
    void Reset(void);
    bool SetNodeByString(QStringList route);
    bool SetNodeById(QList<int> route);
    bool SetCurrentNode(MythGenericTree *node);
    void ShowSearchDialog(void);
    MythGenericTree* GetCurrentNode(void) const { return m_currentNode; }

    void SetActive(bool active);

    MythUIButtonListItem* GetItemCurrent(void) const;
    void RemoveItem(MythUIButtonListItem *item, bool deleteNode = false);
    void RemoveCurrentItem(bool deleteNode = false);

  public slots:
    void handleSelect(MythUIButtonListItem* item);
    void handleClick(MythUIButtonListItem* item);
    void handleVisible(MythUIButtonListItem* item);
    void Select();
    void Deselect();

  signals:
    void itemSelected(MythUIButtonListItem* item);
    void itemClicked(MythUIButtonListItem* item);
    void itemVisible(MythUIButtonListItem* item);
    void nodeChanged(MythGenericTree* node);
    void rootChanged(MythGenericTree* node);

  protected:
    virtual bool ParseElement(
        const QString &filename, QDomElement &element, bool showWarnings);
    virtual void CopyFrom(MythUIType *base);
    virtual void CreateCopy(MythUIType *parent);

  private:
    void Init(void);
    void SetTreeState(bool refreshAll = false);
    bool UpdateList(MythUIButtonList *list, MythGenericTree *node);
    bool DoSetCurrentNode(MythGenericTree *node);

    void SwitchList(bool right);

    bool m_active;
    bool m_initialized;
    uint m_numLists;
    uint m_visibleLists;
    uint m_currentDepth;
    int  m_depthOffset;
    uint m_oldDepth;
    QList<MythUIButtonList*> m_buttonlists;
    MythUIButtonList *m_listTemplate;
    MythUIButtonList *m_activeList;
    uint m_activeListID;
    MythGenericTree *m_rootNode;
    MythGenericTree *m_currentNode;
    uint m_listSpacing;
};

#endif
