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

#include "qmaemo5homescreenadaptor.h"

#include <QtCore/qsocketnotifier.h>
#include <QtCore/qpointer.h>

#include <QtGui/qapplication.h>
#include <QtGui/qx11info_x11.h>
#include <QtGui/qwidget.h>
#include <QtGui/qevent.h>

#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Xutil.h>

static QCoreApplication::EventFilter oldEventFilter;
static QList<QMaemo5HomescreenAdaptor *> allDesktopItems;

static Atom atomByName(const char *name)
{
    Atom atom = XInternAtom(QX11Info::display(), name, False);
    if (!atom)
        qWarning("Unable to obtain %s atom. This class requires a running Hildon session.", name);

    return atom;
}

enum HomescreenAtoms
{
    HildonAppletId               = 0,
    NetWmWindowType              = 1,
    Utf8String                   = 2,
    HildonTypeHomeApplet         = 3,
    HildonAppletSettings         = 4,
    HildonAppletShowSettings     = 5,
    HildonAppletOnCurrentDesktop = 6,
    EnumCount                    = 7
};

static Atom hsAtoms[EnumCount] = { 0, 0, 0, 0, 0, 0, 0 };

static void initAtoms()
{
    hsAtoms[HildonAppletId] = atomByName("_HILDON_APPLET_ID");
    hsAtoms[NetWmWindowType] = atomByName("_NET_WM_WINDOW_TYPE");
    hsAtoms[Utf8String] = atomByName("UTF8_STRING");
    hsAtoms[HildonTypeHomeApplet] = atomByName("_HILDON_WM_WINDOW_TYPE_HOME_APPLET");
    hsAtoms[HildonAppletSettings] = atomByName("_HILDON_APPLET_SETTINGS");
    hsAtoms[HildonAppletShowSettings] = atomByName("_HILDON_APPLET_SHOW_SETTINGS");
    hsAtoms[HildonAppletOnCurrentDesktop] = atomByName("_HILDON_APPLET_ON_CURRENT_DESKTOP");
}

/*! \class QMaemo5HomescreenAdaptor

    \brief The QMaemo5HomescreenAdaptor flags a top-level QWidget as homescreen widget

    QMaemo5HomescreenAdaptor is used in conjunction with the Qt for Maemo homescreen
    loader. It evaluates the two command line arguments "-plugin-id" and "-write-pipe"
    to set up a Qt top-level widget as Maemo 5 homescreen widget.

    Note: By default, the widget will have a black background. In order to make the
    widget transparent, set the Qt::WA_TranslucentBackground widget attribute.

    Example:

    \code
    QLabel *label = new QLabel("Hello Homescreen");
    new QMaemo5HomescreenAdaptor(label);
    label->show();
    \endcode

    Maemo 5 supports homescreen widgets with settings dialogs. To use it, set
    the settingsAvailable() property and show a settings dialog when the
    settingsRequested() signal is emitted.

    Maemo 5 supports more than one homescreen. In order to determine whether
    the homescreen widget is on the currently visible homescreen, connect to
    the homescreenChanged() signal.
*/

/*! \property QMaemo5HomescreenAdaptor::settingsAvailable

    Set this property to true if the widget can make use of a settings dialog,
    otherwise to false. When this property is set to true, the Maemo 5 homescreen
    renders a small settings icon on top of the homescreen widget when the
    user enters the desktop menu. When the user clicks that settings icon, the
    settingsRequested() signal is emitted.

    The default is false.

    \sa settingsRequested()
 */

/*! \fn void settingsRequested()

    This signal is emitted every time the homescreen widget's settings icon is
    invoked by the user. Note that this icon is only visible when the settingsAvailable()
    property is set.

    \sa settingsAvailable()
 */

/*! \fn void homescreenChanged(bool isOnCurrentHomescreen)

    This is signal is emitted when current homescreen changes and the homescreen
    widget becomes visible or invisible. The \a isOnCurrentHomescreen argument
    indicates whether the homescreen widget is on the current homescreen or not.

    This signal can be used to start/stop background processing in order to save
    battery life.
 */

/*!
    Constructs a new QMaemo5HomescreenAdaptor for the given \a widget.

    Note: The widget must be a top-level widget, and must not be reparented
    during the lifetime of this adaptor.

    Note: \a widget is also the parent of this class, if the widget is destroyed,
    so is this adaptor.
 */
QMaemo5HomescreenAdaptor::QMaemo5HomescreenAdaptor(QWidget *widget)
    : QObject(widget),
      hasSettings(false)
{
    Q_ASSERT(widget->isWindow());

    if (!hsAtoms[0])
        initAtoms();

    Display *display = QX11Info::display();

    const QStringList args = QApplication::arguments();

    // parse the command line arguments.
    int idx;
    if ((idx = args.indexOf(QLatin1String("-plugin-id"))) != -1) {
        appletId = args.value(idx + 1);
        const QByteArray pluginId = appletId.toUtf8();
        if (!pluginId.isEmpty()) {
            XChangeProperty(display,
                    widget->winId(),
                    hsAtoms[HildonAppletId],
                    hsAtoms[Utf8String], 8, PropModeReplace,
                    reinterpret_cast<const unsigned char *>(pluginId.constData()),
                    pluginId.length());
        }
    }
    if ((idx = args.indexOf(QLatin1String("-write-pipe"))) != -1) {
        bool ok;
        int sockId = args.value(idx + 1).toInt(&ok);
        if (ok) {
            socketNotifier = new QSocketNotifier(sockId, QSocketNotifier::Exception, this);
            connect(socketNotifier, SIGNAL(activated(int)), this, SLOT(socketException()));
        }
    }

    // set the X11 atoms to flag our widget as homescreen widget
    if (!appletId.isEmpty()) {
        XChangeProperty(display,
                widget->winId(),
                hsAtoms[NetWmWindowType],
                XA_ATOM, 32, PropModeReplace,
                reinterpret_cast<const unsigned char *>(&hsAtoms[HildonTypeHomeApplet]),
                1);

        updateStatus();

        // --- make this window a child of root
        XSetTransientForHint(display, widget->winId(),
                             RootWindow(display, widget->x11Info().screen()));

        // --- add an x11 event filter
        if (!oldEventFilter)
            oldEventFilter = QCoreApplication::instance()->setEventFilter(applicationEventFilter);

        allDesktopItems.append(this);

        // --- set WM input hints indicating that we don't want focus events
        XWMHints *h = XGetWMHints(display, widget->winId());
        XWMHints wm_hints;
        if (!h) {
            memset(&wm_hints, 0, sizeof(wm_hints)); // make valgrind happy
            h = &wm_hints;
        }
        h->flags |= InputHint;
        h->input = False;

        XSetWMHints(display, widget->winId(), h);
        if (h != &wm_hints)
            XFree(h);

        widget->setMouseTracking(true);
    }
}

QMaemo5HomescreenAdaptor::~QMaemo5HomescreenAdaptor()
{
    allDesktopItems.removeOne(this);
}

/*! \internal */
void QMaemo5HomescreenAdaptor::updateStatus()
{
    if (appletId.isEmpty())
        return;

    Display *display = QX11Info::display();

    // Set or remove settings property
    if (hasSettings)
        XChangeProperty(display,
                appletWidget()->winId(),
                hsAtoms[HildonAppletSettings],
                XA_CARDINAL, 32, PropModeReplace,
                (const unsigned char *) &(hasSettings), 1);
    else
        XDeleteProperty(display,
                appletWidget()->winId(),
                hsAtoms[HildonAppletSettings]);
}

/*! \internal */
void QMaemo5HomescreenAdaptor::socketException()
{
    socketNotifier->setEnabled(false);
    appletWidget()->close();
}

bool qt_sendSpontaneousEvent(QObject *receiver, QEvent *event)
{
    return QCoreApplication::sendSpontaneousEvent(receiver, event);
}



/*! \internal */
bool QMaemo5HomescreenAdaptor::applicationEventFilter(void *message, long *result)
{
    static QPointer<QWidget> lastMouseWidget;
    bool retval = false;

    if (oldEventFilter)
        retval = oldEventFilter(message, result);

    if (allDesktopItems.isEmpty())
        return retval;

    XEvent *ev = reinterpret_cast<XEvent *>(message);

    // Generate a mouse release for a leave Notify (as we don't get the mouse release from X11)
    if (ev->type == ButtonPress) {
        QPoint globalPos( ev->xbutton.x_root, ev->xbutton.y_root);
        QWidget *widget = QWidget::find((WId)ev->xany.window);
        if (widget) {
            lastMouseWidget = widget->childAt(widget->mapFromGlobal(globalPos));
            if (!lastMouseWidget)
                lastMouseWidget = widget;
        }

    } else if (ev->type == ButtonRelease) {
        lastMouseWidget = 0;

    } else if (ev->type == LeaveNotify) {
        if (lastMouseWidget) {
            // create a mouse up event that lies in Nirvana.
            QPoint pos(-1000, -1000);
            QMouseEvent e(QEvent::MouseButtonRelease, pos, pos, Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
            qt_sendSpontaneousEvent(lastMouseWidget, &e);
            lastMouseWidget = 0;
       }

    } else if (ev->type == ClientMessage) {
        XClientMessageEvent *cm = (XClientMessageEvent *)message;
        if (cm->message_type == hsAtoms[HildonAppletShowSettings]) {
            for (int i = 0; i < allDesktopItems.count(); ++i) {
                if (allDesktopItems.at(i)->appletWidget()->winId() == ev->xproperty.window) {
                    emit allDesktopItems.at(i)->settingsRequested();
                    retval = true;
                }
            }
        }
    } else if (ev->type == PropertyNotify) {
        if (ev->xproperty.atom == hsAtoms[HildonAppletOnCurrentDesktop]) {
            for (int i = 0; i < allDesktopItems.count(); ++i) {
                if (allDesktopItems.at(i)->appletWidget()->winId() == ev->xproperty.window) {
                    emit allDesktopItems.at(i)->homescreenChanged(ev->xproperty.window == 0);
                    retval = true;
                }
            }
        }
    }

    return retval;
}

