#ifndef MYTHUI_STATETYPE_H_
#define MYTHUI_STATETYPE_H_

#include <QString>
#include <QMap>

#include "mythuicomposite.h"

class MythImage;

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
class MUI_PUBLIC MythUIStateType : public MythUIComposite
{
  public:
    // Can be used for tri-state checks, two state toggles, etc.
    enum StateType : std::uint8_t
        { None = 0, Off, Half, Full };

    MythUIStateType(MythUIType *parent, const QString &name);
   ~MythUIStateType() override = default;

    void SetShowEmpty(bool showempty) { m_showEmpty = showempty; }

    bool AddImage(const QString &name, MythImage *image);
    bool AddImage(StateType type, MythImage *image);

    bool AddObject(const QString &name, MythUIType *object);
    bool AddObject(StateType type, MythUIType *object);

    bool DisplayState(const QString &name);
    bool DisplayState(StateType type);

    MythUIType* GetCurrentState() { return m_currentState; }
    MythUIType* GetState(const QString &name);
    MythUIType* GetState(StateType state);

    void Reset(void) override; // MythUIType
    void Clear(void);

    void EnsureStateLoaded(const QString &name);
    void EnsureStateLoaded(StateType type);

    void LoadNow(void) override; // MythUIType
    void RecalculateArea(bool recurse = true) override; // MythUIType

    void SetTextFromMap(const InfoMap &infoMap) override; // MythUIComposite

  protected:
    bool ParseElement(const QString &filename, QDomElement &element,
                      bool showWarnings) override; // MythUIType
    void CopyFrom(MythUIType *base) override; // MythUIType
    void CreateCopy(MythUIType *parent) override; // MythUIType
    void Finalize(void) override; // MythUIType
    virtual void AdjustDependence(void);

    QMap<QString, MythUIType *> m_objectsByName;
    QMap<int, MythUIType *> m_objectsByState;

    MythUIType *m_currentState {nullptr};
    MythRect    m_parentArea;

    bool        m_showEmpty    {true};

  friend class MythUIButtonList;
};

#endif
