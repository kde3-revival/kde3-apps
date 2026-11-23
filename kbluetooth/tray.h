/*
    This file is part of kbluetooth, the Bluetooth applet of the KDE3 revival

    The icon in the system tray: the state of the default adapter, the
    popup with the paired devices on a left click, the menu on a right
    click, and the notifications.

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.
*/

#ifndef KBLUETOOTH_TRAY_H
#define KBLUETOOTH_TRAY_H

#include <qguardedptr.h>
#include <qmap.h>
#include <qptrlist.h>

#include <ksystemtray.h>

#include "rfkill.h"

class KPopupMenu;
class BluezAgent;
class DevicePopup;
class ScanDialog;

class BluetoothTray : public KSystemTray
{
    Q_OBJECT
public:
    BluetoothTray();
    virtual ~BluetoothTray();

    // the adapter in use, or empty
    QString adapterPath() const;
    // why Bluetooth cannot be used, or empty when it can
    QString problem() const;
    RfKill::State rfkillState() const;

    // "Connecting...", "Disconnecting..." while a call runs, else empty
    QString busyText(const QString &devicePath) const;
    // the error of the last call on the device, if it failed
    QString lastError(const QString &devicePath) const;

    // a menu for one device (connect, rename, trust, remove)
    KPopupMenu *deviceMenu(const QString &devicePath, QWidget *parent);

public slots:
    void showPopup();
    void scanForDevices();
    void setPowered(bool on);
    void powerOn();
    void powerOff();
    void toggleConnection(const QString &devicePath);
    void connectDevice(const QString &devicePath);
    void disconnectDevice(const QString &devicePath);
    void renameDevice(const QString &devicePath);
    void removeDevice(const QString &devicePath);
    void setTrusted(const QString &devicePath, bool trusted);
    // a second start of kbluetooth: show the icon and the popup
    void activate();

signals:
    // the state shown by the popup changed
    void stateChanged();

protected:
    virtual void mousePressEvent(QMouseEvent *e);
    virtual void mouseReleaseEvent(QMouseEvent *e);
    virtual void contextMenuAboutToShow(KPopupMenu *menu);

private slots:
    void updateState();
    void serviceChanged(bool available);
    void deviceChanged(const QString &path);
    void connectionChanged(const QString &path, bool connected);
    void callFinished(const QString &path, int operation,
                      const QString &errorName, const QString &error);
    void menuActivated(int id);
    void rfkillChanged(RfKill::State state);

private:
    void readSettings();
    void writeSettings();
    void notify(const char *event, const QString &text);
    void powerOnLogin();

    enum Action {
        ActConnect, ActDisconnect, ActRename, ActTrust, ActUntrust, ActRemove,
        ActShowWithoutAdapter, ActPowerOnLogin, ActAutostart, ActAdapter, ActScan
    };
    int addAction(KPopupMenu *menu, const QString &text, const QString &icon,
                  Action action, const QString &arg);
    struct MenuEntry { Action action; QString arg; };
    QMap<int, MenuEntry> m_menuEntries;
    int m_nextId;
    QPtrList<KPopupMenu> m_subMenus;

    BluezAgent *m_agent;
    RfKill *m_rfkill;
    QGuardedPtr<DevicePopup> m_popup;
    QGuardedPtr<ScanDialog> m_scan;
    QMap<QString, QString> m_busy;
    QMap<QString, QString> m_errors;
    QStringList m_poweredOnce;  // adapters switched on at login
    int m_iconState;
    bool m_forceShown;      // started again while hidden: stay until an adapter comes

    bool m_showWithoutAdapter;
    bool m_powerOnLogin;
    bool m_autostart;
    QString m_preferredAdapter;
};

#endif
