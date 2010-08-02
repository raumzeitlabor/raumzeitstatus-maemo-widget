/****************************************************************************
**
** Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies).
** All rights reserved.
** Contact: Nokia Corporation (qt-info@nokia.com)
**
** This file is part of the examples of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** No Commercial Usage
** This file contains pre-release code and may not be distributed.
** You may use this file in accordance with the terms and conditions
** contained in the Technology Preview License Agreement accompanying
** this package.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU Lesser General Public License version 2.1 requirements
** will be met: http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Nokia gives you certain additional
** rights.  These rights are described in the Nokia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** If you have questions regarding the use of this file, please contact
** Nokia at qt-info@nokia.com.
**
**
**
**
**
**
**
**
** $QT_END_LICENSE$
**
****************************************************************************/

#ifndef QMAEMO5HOMESCREENADAPTOR_H
#define QMAEMO5HOMESCREENADAPTOR_H

#include <QtCore/qobject.h>
#include <QtCore/qpointer.h>
#include <QtGui/qwidget.h>

class QWidget;
class QSocketNotifier;

class QMaemo5HomescreenAdaptor : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool settingsAvailable READ settingsAvailable WRITE setSettingsAvailable)

public:
    QMaemo5HomescreenAdaptor(QWidget *widget);
    ~QMaemo5HomescreenAdaptor();

    inline void setSettingsAvailable(bool available)
    {
        hasSettings = available;
        updateStatus();
    }

    inline bool settingsAvailable() const
    {
        return hasSettings;
    }

Q_SIGNALS:
    void settingsRequested();
    void homescreenChanged(bool isOnCurrentHomescreen);

private Q_SLOTS:
    void socketException();

private:
    inline QWidget *appletWidget() const { return static_cast<QWidget *>(parent()); }

    void updateStatus();

    static bool applicationEventFilter(void *message, long *result);

    bool hasSettings;
    QString appletId;
    QSocketNotifier *socketNotifier;
};

#endif
