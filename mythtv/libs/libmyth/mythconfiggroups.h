// -*- Mode: c++ -*-

#ifndef MYTH_CONFIG_GROUPS_H
#define MYTH_CONFIG_GROUPS_H

#include <QVBoxLayout>

// MythTV headers
#include "mythexp.h"
#include "mythstorage.h"

// C++ headers
#include <vector>

#define MYTHCONFIG
#include "settings.h"
#undef MYTHCONFIG

class QStackedWidget;
class MythGroupBox;

class MPUBLIC ConfigurationGroup : public Setting, public Storage
{
    Q_OBJECT

  public:
    ConfigurationGroup(bool luselabel   = true,  bool luseframe  = true,
                       bool lzeroMargin = false, bool lzeroSpace = false);

    virtual void deleteLater(void);

    void addChild(Configurable *child)
    {
        children.push_back(child);
    };

    Setting *byName(const QString &name) override; // Configurable

    void setUseLabel(bool useit) { uselabel = useit; }
    void setUseFrame(bool useit) { useframe = useit; }

    void setOptions(bool luselabel   = true,  bool luseframe  = true,
                    bool lzeroMargin = false, bool lzeroSpace = false)
    {
        uselabel = luselabel; useframe = luseframe;
        zeroMargin = lzeroMargin; zeroSpace = lzeroSpace;
    }

    // Storage
    void Load(void) override; // Storage
    void Save(void) override; // Storage
    void Save(QString destination) override; // Storage
    void SetSaveRequired(void) override; // Storage

  signals:
    void changeHelpText(QString);

  protected:
    virtual ~ConfigurationGroup();

  protected:
    typedef std::vector<Configurable*> childList;
    childList children;
    bool uselabel;
    bool useframe;
    bool zeroMargin;
    bool zeroSpace;
    int margin;
    int space;
};

class MPUBLIC VerticalConfigurationGroup : public ConfigurationGroup
{
  public:
    VerticalConfigurationGroup(
        bool luselabel   = true,  bool luseframe  = true,
        bool lzeroMargin = false, bool lzeroSpace = false) :
        ConfigurationGroup(luselabel, luseframe, lzeroMargin, lzeroSpace),
        widget(nullptr), confgrp(nullptr), layout(nullptr)
    {
    }

    void deleteLater(void) override; // ConfigurationGroup

    QWidget *configWidget(ConfigurationGroup *cg,
                          QWidget            *parent,
                          const char         *widgetName) override; // Configurable
    void widgetInvalid(QObject *obj) override; // Configurable

    bool replaceChild(Configurable *old_child, Configurable *new_child);
    void repaint(void);

  protected:
    /// You need to call deleteLater to delete QObject
    virtual ~VerticalConfigurationGroup() = default;

  private:
    std::vector<QWidget*>    childwidget;
    QWidget            *widget;
    ConfigurationGroup *confgrp;
    QVBoxLayout        *layout;
};

class MPUBLIC HorizontalConfigurationGroup : public ConfigurationGroup
{
  public:
    HorizontalConfigurationGroup(
        bool luselabel   = true,  bool luseframe  = true,
        bool lzeroMargin = false, bool lzeroSpace = false) :
        ConfigurationGroup(luselabel, luseframe, lzeroMargin, lzeroSpace)
    {
    }

    QWidget *configWidget(ConfigurationGroup *cg,
                          QWidget            *parent,
                          const char         *widgetName) override; // Configurable

  protected:
    /// You need to call deleteLater to delete QObject
    virtual ~HorizontalConfigurationGroup() = default;
};

class MPUBLIC GridConfigurationGroup : public ConfigurationGroup
{
  public:
    GridConfigurationGroup(uint col,
                           bool uselabel   = true,  bool useframe  = true,
                           bool zeroMargin = false, bool zeroSpace = false) :
        ConfigurationGroup(uselabel, useframe, zeroMargin, zeroSpace),
        columns(col)
    {
    }

    QWidget *configWidget(ConfigurationGroup *cg,
                          QWidget            *parent,
                          const char         *widgetName) override; // Configurable

  protected:
    /// You need to call deleteLater to delete QObject
    virtual ~GridConfigurationGroup() = default;

  private:
    uint columns;
};

class MPUBLIC StackedConfigurationGroup : public ConfigurationGroup
{
    Q_OBJECT

  public:
    StackedConfigurationGroup(
        bool uselabel   = true,  bool useframe  = true,
        bool zeroMargin = false, bool zeroSpace = false) :
        ConfigurationGroup(uselabel, useframe, zeroMargin, zeroSpace),
        widget(nullptr), confgrp(nullptr), top(0), saveAll(true)
    {
    }

    void deleteLater(void) override; // ConfigurationGroup

    QWidget *configWidget(ConfigurationGroup *cg, QWidget *parent,
                          const char *widgetName = nullptr) override; // Configurable

    void raise(Configurable *child);
    void Save(void) override; // ConfigurationGroup
    void Save(QString destination) override; // ConfigurationGroup

    // save all children, or only the top?
    void setSaveAll(bool b) { saveAll = b; };

    void addChild(Configurable*);
    void removeChild(Configurable*);

  public slots:
    void widgetInvalid(QObject *obj) override; // Configurable

  protected:
    /// You need to call deleteLater to delete QObject
    virtual ~StackedConfigurationGroup();

  protected:
    std::vector<QWidget*>    childwidget;
    QStackedWidget     *widget;
    ConfigurationGroup *confgrp;
    uint                top;
    bool                saveAll;
};

class MPUBLIC TriggeredConfigurationGroup : public ConfigurationGroup
{
    Q_OBJECT

  public:
    TriggeredConfigurationGroup(
        bool uselabel         = true,  bool useframe        = true,
        bool zeroMargin       = false, bool zeroSpace       = false,
        bool stack_uselabel   = true,  bool stack_useframe  = true,
        bool stack_zeroMargin = false, bool stack_zeroSpace = false) :
        ConfigurationGroup(uselabel, useframe, zeroMargin, zeroSpace),
        stackUseLabel(stack_uselabel),     stackUseFrame(stack_useframe),
        stackZeroMargin(stack_zeroMargin), stackZeroSpace(stack_zeroSpace),
        isVertical(true),                  isSaveAll(true),
        configLayout(nullptr),             configStack(nullptr),
        trigger(nullptr),                  widget(nullptr)
    {
    }

    // Commands

    virtual void addChild(Configurable *child);

    void addTarget(QString triggerValue, Configurable *target);
    void removeTarget(QString triggerValue);

    QWidget *configWidget(ConfigurationGroup *cg,
                          QWidget            *parent,
                          const char         *widgetName) override; // Configurable
    void widgetInvalid(QObject *obj) override; // Configurable

    Setting *byName(const QString &settingName) override; // ConfigurationGroup

    void Load(void) override; // ConfigurationGroup
    void Save(void) override; // ConfigurationGroup
    void Save(QString destination) override; // ConfigurationGroup

    void repaint(void);

    // Sets

    void SetVertical(bool vert);

    virtual void setSaveAll(bool b)
    {
        if (configStack)
            configStack->setSaveAll(b);
        isSaveAll = b;
    }

    void setTrigger(Configurable *_trigger);

  protected slots:
    virtual void triggerChanged(const QString &value);

  protected:
    /// You need to call deleteLater to delete QObject
    virtual ~TriggeredConfigurationGroup() = default;
    void VerifyLayout(void);

  protected:
    bool stackUseLabel;
    bool stackUseFrame;
    bool stackZeroMargin;
    bool stackZeroSpace;
    bool isVertical;
    bool isSaveAll;
    ConfigurationGroup          *configLayout;
    StackedConfigurationGroup   *configStack;
    Configurable                *trigger;
    QMap<QString,Configurable*>  triggerMap;
    QWidget                     *widget;
};

class MPUBLIC JumpPane : public VerticalConfigurationGroup
{
    Q_OBJECT

  public:
    JumpPane(const QStringList &labels, const QStringList &helptext);

  signals:
    void pressed(QString);
};

#endif // MYTH_CONFIG_GROUPS_H
