/*
 * vim:ts=4:sw=4:expandtab
 *
 * RaumZeitLabor status widget
 *
 * © 2010 Michael Stapelberg
 * © 2010 Nokia Corporation
 *
 * See LICENSE for licensing information
 *
 */
#ifndef RZLWIDGET_H
#define RZLWIDGET_H

#include <QtGui/qlabel.h>
#include <QtGui/qinputdialog.h>
#include <QtGui/qpainter.h>
#include <QTimer>
#include <QIcon>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>

/* for conic (connection status) we need glib */
#include <glib-object.h>
#include <conic/conic.h>
#include <dbus/dbus-glib-lowlevel.h>

class RZLWidget : public QWidget
{
    Q_OBJECT

private:
    QNetworkAccessManager *network;
    ConIcConnection *connection;
    QTimer *timer;
    QTimer *periodic_bearer;
    QIcon *icon_unklar;
    QIcon *icon_auf;
    QIcon *icon_zu;
    QIcon *icon;
    QString lastBearer;
    QString text;
    QString lastUpdated;
    int interval;

public:

    RZLWidget(QWidget *parent = 0);

    QSize minimumSizeHint() const {
        return QSize(90, 90);
    }

    QSize sizeHint() const {
        return QSize(90, 90);
    }

    void setConnection(QString bearer);
    void update();

public slots:
    void trigger_update();
    void trigger_periodic();
    void req_finished();
    void req_error(QNetworkReply::NetworkError err);

protected:
    void paintEvent(QPaintEvent *event);
    void mouseReleaseEvent(QMouseEvent *event);
};

#endif
