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

RZLWidget::RZLWidget(QWidget *parent) : QWidget(parent) {
    setAttribute(Qt::WA_TranslucentBackground);

    icon_unklar = new QIcon("/usr/share/raumzeitlabor-status-widget/unklar.png");
    icon_auf = new QIcon("/usr/share/raumzeitlabor-status-widget/auf.png");
    icon_zu = new QIcon("/usr/share/raumzeitlabor-status-widget/zu.png");
    icon = icon_unklar;

    lastUpdated = "?";

    network = new QNetworkAccessManager(this);

    /* Timer will be triggered in setConnection() as soon as the connection
     * status is known */
    timer = new QTimer(this);
    connect(timer, SIGNAL(timeout()), this, SLOT(update()));

    /* Setup stuff for the conic library */
    DBusConnection *conn;
    DBusError err;

    dbus_error_init(&err);
    conn = dbus_bus_get(DBUS_BUS_SYSTEM, &err);
    if (!conn)
        return;

    dbus_connection_setup_with_g_main(conn, NULL);

    /* We want to get called on connection events */
    ConIcConnection *connection = con_ic_connection_new();
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
    if (bearer == "offline") {
        timer->stop();
        icon = icon_unklar;
        repaint();
        return;
    }

    /* on wireless, update every 15 minutes */
    if (bearer == "WLAN_INFRA") {
        timer->stop();
        timer->start(15 * 60 * 1000);
    }
    else {
        /* on data connection, update every 30 minutes */
        timer->stop();
        timer->start(30 * 60 * 1000);
    }

    /* also trigger an immediate update */
    update();
}

void RZLWidget::paintEvent(QPaintEvent *event) {
    Q_UNUSED(event);

    QPainter p(this);
    p.setBrush(QColor(0, 0, 0, 150));
    p.setPen(Qt::NoPen);
    p.drawRoundedRect(rect(), 15, 15);

    icon->paint(&p, rect());

    p.setPen(QPen(Qt::white));
    p.drawText(rect(), Qt::AlignHCenter, "rzl");
    QRect r = rect();
    QRect lu_rect = QRect(r.x(), 75, r.width(), 30);

    p.drawText(lu_rect, Qt::AlignHCenter, lastUpdated);
}

/*
 * On click (when the mouse is released), trigger an update
 *
 */
void RZLWidget::mouseReleaseEvent(QMouseEvent *event) {
    Q_UNUSED(event);

    /* if the timer is not active, we don't have an internet connection, so
     * cancel this handler */
    if (!timer->isActive())
        return;

    update();
}

void RZLWidget::update() {
    /* Send a new HTTP request to get the status */
    QNetworkRequest request;
    request.setUrl(QUrl("http://scytale.name/files/tmp/rzlstatus.txt"));
    QNetworkReply *reply = network->get(request);
    connect(reply, SIGNAL(finished()), this, SLOT(req_finished()));
    connect(reply, SIGNAL(error(QNetworkReply::NetworkError)),
            this, SLOT(req_error(QNetworkReply::NetworkError)));
}

void RZLWidget::req_finished() {
    QNetworkReply *reply = (QNetworkReply*)sender();
    QByteArray answer = reply->readAll();

    icon = (QString(answer).trimmed() == "1" ? icon_auf : icon_zu);
    lastUpdated = QDateTime::currentDateTime().toString("hh:mm");
    repaint();

    reply->deleteLater();
}

void RZLWidget::req_error(QNetworkReply::NetworkError err) {
    Q_UNUSED(err);

    QNetworkReply *reply = (QNetworkReply*)sender();
    /* disconnect all signals to prevent finished() from getting called with an
     * empty reply */
    reply->disconnect();
    icon = icon_unklar;
    lastUpdated = QDateTime::currentDateTime().toString("hh:mm");
    repaint();

    reply->deleteLater();
}