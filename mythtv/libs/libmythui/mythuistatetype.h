#ifndef MYTHUI_STATETYPE_H_
#define MYTHUI_STATETYPE_H_

#include <QString>
#include <QMap>

#include "mythuitype.h"
#include "mythimage.h"

/** \class MythUIStateType
 *
 * \brief This widget is used for grouping other widgets for display when a
 *        particular named state is called. A statetype can contain any number
 *        of state groups which can themselves contain any number of widgets.
 *
 * States are mutally exclusive, when one state is displayed the others are
 * hidden.
 *
 * \ingroup MythUI_Widgets
 */
class MPUBLIC MythUIStateType : public MythUIType
{
  public:
    enum StateType { None = 0, Off, Half, Full }; // Can be used for tri-state checks, two state toggles, etc.

    MythUIStateType(MythUIType *parent, const QString &name);
   ~MythUIStateType();

    void SetShowEmpty(bool showempty) { m_ShowEmpty = showempty; }

    bool AddImage(const QString &name, MythImage *image);
    bool AddImage(StateType type, MythImage *image);

    bool AddObject(const QString &name, MythUIType *object);
    bool AddObject(StateType type, MythUIType *object);

    bool DisplayState(const QString &name);
    bool DisplayState(StateType type);

    MythUIType* GetCurrentState() { return m_CurrentState; }
    MythUIType* GetState(const QString &name);
    MythUIType* GetState(StateType state);

    void Reset(void);
    void Clear(void);

    void EnsureStateLoaded(const QString &name);
    void EnsureStateLoaded(StateType type);

    virtual void LoadNow(void);

  protected:
    virtual bool ParseElement(
        const QString &filename, QDomElement &element, bool showWarnings);
    virtual void CopyFrom(MythUIType *base);
    virtual void CreateCopy(MythUIType *parent);
    virtual void Finalize(void);

    QMap<QString, MythUIType *> m_ObjectsByName;
    QMap<int, MythUIType *> m_ObjectsByState;

    MythUIType *m_CurrentState;

    bool m_ShowEmpty;

  friend class MythUIButtonList;
};

#endif
