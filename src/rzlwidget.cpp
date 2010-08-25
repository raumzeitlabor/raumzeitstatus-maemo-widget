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
#define DEBUG 1
#define OSSOLOG_SYSLOG 1
#include <osso-log.h>

#include <QDateTime>
#include "rzlwidget.h"

static void connection_change(ConIcConnection *connection, ConIcConnectionEvent *event, gpointer user_data) {
    Q_UNUSED(connection);
    RZLWidget *w = (RZLWidget*)user_data;
 
    ConIcConnectionStatus status = con_ic_connection_event_get_status(event);
    switch(status) {
        case CON_IC_STATUS_CONNECTED:
            w->setConnection(QString(con_ic_event_get_bearer_type(CON_IC_EVENT(event))));
            break;
        case CON_IC_STATUS_DISCONNECTING:
            w->setConnection("offline");
            break;
        case CON_IC_STATUS_DISCONNECTED:
            w->setConnection("offline");
            break;
        default:
            qDebug("Unknown connection status received");
    }
}

static size_t recv_status(void *buffer, size_t size, size_t nmemb, void *userp) {
    RZLWidget *widget = (RZLWidget*)userp;
    const char *buf = (const char*)buffer;

    if ((size * nmemb) > 0)
        widget->receive_status(QString(buf[0]));
    else widget->req_error();

    return size * nmemb;
}

RZLWidget::RZLWidget(QWidget *parent) : QWidget(parent) {
    setAttribute(Qt::WA_TranslucentBackground);

    icon_unklar = new QIcon("/usr/share/raumzeitlabor-status-widget/unklar.png");
    icon_auf = new QIcon("/usr/share/raumzeitlabor-status-widget/auf.png");
    icon_zu = new QIcon("/usr/share/raumzeitlabor-status-widget/zu.png");
    icon = icon_unklar;

    lastUpdated = "?";

    hdl = curl_easy_init();

    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers, "Pragma: no-cache");
    headers = curl_slist_append(headers, "Cache-Control: no-cache");

    curl_easy_setopt(hdl, CURLOPT_URL, "http://status.raumzeitlabor.de/api/simple");
    curl_easy_setopt(hdl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(hdl, CURLOPT_WRITEFUNCTION, recv_status);
    curl_easy_setopt(hdl, CURLOPT_WRITEDATA, this);
    curl_easy_setopt(hdl, CURLOPT_ERRORBUFFER, errbuf);

    /* Timer will be triggered in setConnection() as soon as the connection
     * status is known */
    timer = new QTimer(this);
    connect(timer, SIGNAL(timeout()), this, SLOT(trigger_update()));

    periodic_bearer = new QTimer(this);
    periodic_bearer->start(60 * 1000);
    connect(periodic_bearer, SIGNAL(timeout()), this, SLOT(trigger_periodic()));

    /* Setup stuff for the conic library */
    DBusConnection *conn;
    DBusError err;

    dbus_error_init(&err);
    conn = dbus_bus_get(DBUS_BUS_SYSTEM, &err);
    if (!conn)
        return;

    dbus_connection_setup_with_g_main(conn, NULL);

    /* We want to get called on connection events */
    connection = con_ic_connection_new();
    g_signal_connect(G_OBJECT(connection), "connection-event", G_CALLBACK(connection_change), this);
    g_object_set(G_OBJECT(connection), "automatic-connection-events", TRUE, NULL);

    con_ic_connection_connect(connection, CON_IC_CONNECT_FLAG_NONE);
}

/*
 * Called by connection_change() with the bearer (WLAN_INFRA for wireless,
 * offline when not connected, anything else for some other kind of
 * connection like GPRS, UMTS, Bluetooth, …)
 *
 */
void RZLWidget::setConnection(QString bearer) {
    if (lastBearer == bearer)
        return;
    ULOG_INFO_L("new bearer: %s", (char*)bearer.toAscii().data());
    lastBearer = bearer;

    if (bearer == "offline") {
        timer->stop();
        if (lastUpdated == "...") {
            lastUpdated = QString("(%1)").arg(QDateTime::currentDateTime().toString("hh:mm"));
            repaint();
        }
        return;
    }

    /* on wireless, update every 15 minutes */
    if (bearer == "WLAN_INFRA") {
        timer->stop();
        interval = 15 * 60 * 1000;

        QTime next = QTime::currentTime();
        int min;
        int hr = next.hour();
        if (next.minute() < 45 && next.minute() >= 30)
            min = 45;
        else if (next.minute() < 30 && next.minute() >= 15)
            min = 30;
        else if (next.minute() < 15 && next.minute() >= 0)
            min = 15;
        else {
            min = 0;
            hr = (hr < 23 ? hr + 1 : 0);
        }
        next.setHMS(hr, min, 0);
        timer->start(QTime::currentTime().msecsTo(next));
    }
    else {
        /* on data connection, update every 30 minutes */
        timer->stop();
        interval = 30 * 60 * 1000;

        QTime next = QTime::currentTime();
        int min;
        int hr = next.hour();
        if (next.minute() >= 30) {
            min = 0;
            hr = (hr < 23 ? hr + 1 : 0);
        } else min = 30;
        next.setHMS(hr, (next.minute() >= 30 ? 0 : 30), 0);
        timer->start(QTime::currentTime().msecsTo(next));
    }

    /* also trigger an immediate update */
    update();
}

void RZLWidget::paintEvent(QPaintEvent *event) {
    Q_UNUSED(event);

    QRect r = rect();
    QPainter p(this);
    p.setBrush(QColor(0, 0, 0, 150));
    p.setPen(Qt::NoPen);
    p.drawRoundedRect(rect(), 15, 15);
    QRect iconrect = QRect(r.x(), 10, r.width(), 50);

    icon->paint(&p, iconrect);

    p.setPen(QPen(Qt::white));
    QRect lu_rect = QRect(r.x(), 55, r.width(), 30);

    p.drawText(lu_rect, Qt::AlignHCenter, lastUpdated);
}

/*
 * On click (when the mouse is released), trigger an update
 *
 */
void RZLWidget::mouseReleaseEvent(QMouseEvent *event) {
    Q_UNUSED(event);

    update();
}

/*
 * We periodically get the bearer because icd sometimes fails to provide us
 * with bearer changes (or mixes up the order). Meh.
 *
 */
void RZLWidget::trigger_periodic() {
    con_ic_connection_connect(connection, CON_IC_CONNECT_FLAG_NONE);
}

void RZLWidget::trigger_update() {
    if (interval > 0) {
        timer->stop();
        timer->start(interval);
        interval = 0;
    }

    update();
}

void RZLWidget::update() {
    lastUpdated = "...";
    repaint();

    /* Send a new HTTP request to get the status */
    CURLcode success = curl_easy_perform(hdl);
    if (success == 0)
        return;

    ULOG_ERR_L("Error updating status: %s", errbuf);
    req_error();
}

void RZLWidget::receive_status(QString status) {
    if (status == "1")
        icon = icon_auf;
    else if (status == "0")
        icon = icon_zu;
    else icon = icon_unklar;
    lastUpdated = QDateTime::currentDateTime().toString("hh:mm");
    repaint();
}

void RZLWidget::req_error() {
    icon = icon_unklar;
    lastUpdated = QDateTime::currentDateTime().toString("hh:mm");
    repaint();
}
