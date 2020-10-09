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
   ~MythUIButtonTree() override = default;

    bool keyPressEvent(QKeyEvent *event) override; // MythUIType
    bool gestureEvent(MythGestureEvent *event) override; // MythUIType

    bool AssignTree(MythGenericTree *tree);
    void Reset(void) override; // MythUIType
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
    bool ParseElement(const QString &filename, QDomElement &element,
                      bool showWarnings) override; // MythUIType
    void CopyFrom(MythUIType *base) override; // MythUIType
    void CreateCopy(MythUIType *parent) override; // MythUIType

  private:
    void Init(void);
    void SetTreeState(bool refreshAll = false);
    bool UpdateList(MythUIButtonList *list, MythGenericTree *node) const;
    bool DoSetCurrentNode(MythGenericTree *node);

    void SwitchList(bool right);

    bool m_active                    {true};
    bool m_initialized               {false};
    uint m_numLists                  {1};
    uint m_visibleLists              {0};
    uint m_currentDepth              {0};
    int  m_depthOffset               {0};
    uint m_oldDepth                  {0};
    QList<MythUIButtonList*> m_buttonlists;
    MythUIButtonList *m_listTemplate {nullptr};
    MythUIButtonList *m_activeList   {nullptr};
    uint              m_activeListID {0};
    MythGenericTree  *m_rootNode     {nullptr};
    MythGenericTree  *m_currentNode  {nullptr};
    uint              m_listSpacing  {0};
};

#endif
