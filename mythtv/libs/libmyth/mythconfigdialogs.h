// -*- Mode: c++ -*-

#ifndef MYTH_CONFIG_DIALOGS_H
#define MYTH_CONFIG_DIALOGS_H

// C++ headers
#include <vector>

// Qt headers
#include <QObject>
#include <QString>

// MythTV headers
#include "mythexp.h"
#include "mythwidgets.h"
#include "mythdialogs.h"
#include "mythdbcon.h"
#include "mythstorage.h"
#include "mythconfiggroups.h"
#include "mythterminal.h"

class MPUBLIC ConfigurationDialogWidget : public MythDialog
{
    Q_OBJECT

        public:
    ConfigurationDialogWidget(MythMainWindow *parent,
                              const char     *widgetName) :
        MythDialog(parent, widgetName) { }

    virtual void keyPressEvent(QKeyEvent *e);

  signals:
    void editButtonPressed(void);
    void deleteButtonPressed(void);
};

/** \class ConfigurationDialog
 *  \brief A ConfigurationDialog that uses a ConfigurationGroup
 *         all children on one page in a vertical layout.
 */
class MPUBLIC ConfigurationDialog : public Storage
{
  public:
    ConfigurationDialog() : dialog(NULL), cfgGrp(new ConfigurationGroup()) { }
    virtual ~ConfigurationDialog();

    // Make a modal dialog containing configWidget
    virtual MythDialog *dialogWidget(MythMainWindow *parent,
                                     const char     *widgetName);

    // Show a dialogWidget, and save if accepted
    virtual DialogCode exec(bool saveOnExec = true, bool doLoad = true);

    virtual void addChild(Configurable *child);

    virtual Setting *byName(const QString &settingName)
        { return cfgGrp->byName(settingName); }

    void setLabel(const QString &label);

    // Storage
    virtual void Load(void) { cfgGrp->Load(); }
    virtual void Save(void) { cfgGrp->Save(); }
    virtual void Save(QString destination) { cfgGrp->Save(destination); }

  protected:
    typedef std::vector<Configurable*> ChildList;

    ChildList           cfgChildren;
    std::vector<QWidget*>    childwidget;
    MythDialog         *dialog;
    ConfigurationGroup *cfgGrp;
};

/** \class ConfigurationWizard
 *  \brief A ConfigurationDialog that uses a ConfigurationGroup
 *         with one child per page.
 */
class MPUBLIC ConfigurationWizard : public ConfigurationDialog
{
  public:
    ConfigurationWizard() : ConfigurationDialog() {}

    virtual MythDialog *dialogWidget(MythMainWindow *parent,
                                     const char *widgetName);
};

class MythTerminal;
class MPUBLIC TerminalWizard : public ConfigurationWizard
{
  public:
    TerminalWizard(QString program, QStringList args);

    virtual DialogCode exec(bool saveOnExec = false, bool doLoad = false);

  private:
    MythTerminal *terminal;
};

#endif // MYTH_CONFIG_DIALOGS_H
