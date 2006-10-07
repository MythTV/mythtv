/****************************************************************************
** 
**
** Definition of the MythWizard class.
**
** Created : 990101
**
** Copyright (C) 1999 by Trolltech AS.  All rights reserved.
**
** This file is part of the dialogs module of the Qt GUI Toolkit.
**
** This file may be distributed under the terms of the Q Public License
** as defined by Trolltech AS of Norway and appearing in the file
** LICENSE.QPL included in the packaging of this file.
**
** This file may be distributed and/or modified under the terms of the
** GNU General Public License version 2 as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL included in the
** packaging of this file.
**
** Licensees holding valid Qt Enterprise Edition or Qt Professional Edition
** licenses may use this file in accordance with the Qt Commercial License
** Agreement provided with the Software.
**
** This file is provided AS IS with NO WARRANTY OF ANY KIND, INCLUDING THE
** WARRANTY OF DESIGN, MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
**
** See http://www.trolltech.com/pricing.html or email sales@trolltech.com for
**   information about Qt Commercial License Agreements.
** See http://www.trolltech.com/qpl/ for QPL licensing information.
** See http://www.trolltech.com/gpl/ for GPL licensing information.
**
** Contact info@trolltech.com if any conditions of this licensing are
** not clear to you.
**
**********************************************************************/

#ifndef MYTHWIZARDDIALOG_H
#define MYTHWIZARDDIALOG_H

#include "mythwidgets.h"
#include "mythdialogs.h"

class QHBoxLayout;
class MythWizardPrivate;

class MPUBLIC MythWizard : public MythDialog
{
    Q_OBJECT
    Q_PROPERTY(QFont titleFont READ titleFont WRITE setTitleFont)

public:
    MythWizard(MythMainWindow* parent, const char* name = 0);
    virtual ~MythWizard();

    void Show();

    void setFont(const QFont & font);

    virtual void addPage(QWidget *, const QString &);
    virtual void insertPage(QWidget *, const QString &, int);
    virtual void removePage(QWidget *);

    QString title(QWidget *) const;
    void setTitle(QWidget *, const QString &);
    QFont titleFont() const;
    void setTitleFont(const QFont &);

    virtual void showPage(QWidget *);

    QWidget * currentPage() const;

    QWidget* page(int) const;
    int pageCount() const;
    int indexOf(QWidget*) const;

    virtual bool appropriate(QWidget *) const;
    virtual void setAppropriate(QWidget *, bool);

    MythPushButton * backButton() const;
    MythPushButton * nextButton() const;
    MythPushButton * finishButton() const;
    MythPushButton * cancelButton() const;

    bool eventFilter(QObject *, QEvent *);

    virtual void keyPressEvent(QKeyEvent *);

public slots:
    virtual void setBackEnabled(QWidget *, bool);
    virtual void setNextEnabled(QWidget *, bool);
    virtual void setFinishEnabled(QWidget *, bool);
    
    virtual void setHelpText(QString helptext);

protected slots:
    virtual void back();
    virtual void next();

signals:
    void selected(const QString&);

protected:
    virtual void layOutButtonRow(QHBoxLayout *);
    virtual void layOutTitleRow(QHBoxLayout *, const QString &);

    void setBackEnabled(bool);
    void setNextEnabled(bool);

    void setNextPage(QWidget *);

    void updateButtons();

    void layOut();

    MythWizardPrivate *d;
};

class MPUBLIC MythJumpWizard : public MythWizard
{
    Q_OBJECT

  public:
    MythJumpWizard(MythMainWindow *parent, const char *name = NULL);
    virtual ~MythJumpWizard();

};

#endif
