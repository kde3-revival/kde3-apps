/*
    This file is part of kbluetooth, the Bluetooth applet of the KDE3 revival

    The popup of a left click on the tray icon: the power switch of the
    adapter, the paired devices (a click connects or disconnects one) and
    the way to the scan dialog.

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.
*/

#ifndef KBLUETOOTH_POPUP_H
#define KBLUETOOTH_POPUP_H

#include <qframe.h>
#include <qptrlist.h>

class QCheckBox;
class QLabel;
class QVBox;
class QVBoxLayout;
class KPushButton;
class BluetoothTray;

// One paired device in the popup
class DeviceRow : public QFrame
{
    Q_OBJECT
public:
    DeviceRow(const QString &path, QWidget *parent);
    QString path() const { return m_path; }
    void update(BluetoothTray *tray);

signals:
    void clicked(const QString &path);
    void menuRequested(const QString &path, const QPoint &pos);

protected:
    virtual void mouseReleaseEvent(QMouseEvent *e);
    virtual void enterEvent(QEvent *e);
    virtual void leaveEvent(QEvent *e);

private:
    void setHighlighted(bool on);

    QString m_path;
    QLabel *m_icon;
    QLabel *m_text;
};

class DevicePopup : public QFrame
{
    Q_OBJECT
public:
    DevicePopup(BluetoothTray *tray);

    // shows the popup next to the tray icon
    void popupAt(QWidget *icon);

private slots:
    void refresh();
    void powerToggled(bool on);
    void rowClicked(const QString &path);
    void rowMenu(const QString &path, const QPoint &pos);
    void scan();

private:
    void place();

    BluetoothTray *m_tray;
    QWidget *m_anchor;
    QLabel *m_title;
    QCheckBox *m_power;
    QLabel *m_message;
    QLabel *m_devicesLabel;
    QVBox *m_devices;
    QLabel *m_none;
    KPushButton *m_scan;
    QPtrList<DeviceRow> m_rows;
};

#endif
