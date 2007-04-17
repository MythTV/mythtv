#ifndef MYTHSETTINGS_H
#define MYTHSETTINGS_H

#include "mythtv/settings.h"
#include "mythtv/mythcontext.h"

class MusicGeneralSettings : public ConfigurationWizard
{
  public:
    MusicGeneralSettings();
};

class MusicPlayerSettings : public QObject, public ConfigurationWizard
{
  Q_OBJECT

  public:
    MusicPlayerSettings();
    ~MusicPlayerSettings() {};

  public slots:
    void showVisEditor(void);

  private:
    HostLineEdit *visModesEdit;
};

class MusicRipperSettings : public ConfigurationWizard
{
  public:
    MusicRipperSettings();
};


class VisualizationsEditor : public MythDialog
{
  Q_OBJECT
  public:

    VisualizationsEditor(const QString &currentSelection, MythMainWindow *parent,
                         const char *name = 0);
    ~VisualizationsEditor(void);

    QString getSelectedModes(void);

  protected slots:
    void okClicked(void);
    void cancelClicked(void);
    void upClicked(void);
    void downClicked(void);
    void availableChanged(QListViewItem *item);
    void selectedChanged(QListViewItem *item);
    void availableOnSelect(QListViewItem *item);
    void selectedOnSelect(QListViewItem *item);

  protected:
    bool eventFilter(QObject *obj, QEvent *ev);
    bool handleKeyPress(QKeyEvent *e);

    private:
    void fillWidgets(const QString &currentSelection);

    MythListView   *availableList;
    MythListView   *selectedList;
};

#endif
