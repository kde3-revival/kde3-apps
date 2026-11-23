/*
    This file is part of kbluetooth, the Bluetooth applet of the KDE3 revival

    The dialog that looks for devices (discovery) and pairs, trusts and
    connects the one the user picks.

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.
*/

#ifndef KBLUETOOTH_SCANDIALOG_H
#define KBLUETOOTH_SCANDIALOG_H

#include <qmap.h>

#include <kdialogbase.h>

class QLabel;
class QListViewItem;
class KListView;
class BluetoothTray;

class ScanDialog : public KDialogBase
{
    Q_OBJECT
public:
    ScanDialog(BluetoothTray *tray);
    virtual ~ScanDialog();

protected:
    virtual void showEvent(QShowEvent *e);
    virtual void hideEvent(QHideEvent *e);

protected slots:
    // Pair (or Cancel Pairing)
    virtual void slotUser1();

private slots:
    void refresh();
    void deviceAdded(const QString &path);
    void deviceRemoved(const QString &path);
    void deviceChanged(const QString &path);
    void callFinished(const QString &path, int operation,
                      const QString &errorName, const QString &error);
    void selectionChanged();
    void executed(QListViewItem *item);

private:
    void startDiscovery();
    void stopDiscovery();
    void updateItem(const QString &path);
    QString selectedPath() const;

    BluetoothTray *m_tray;
    KListView *m_list;
    QLabel *m_status;
    QLabel *m_detail;
    QString m_adapter;          // the adapter we discover on
    bool m_discovering;         // StartDiscovery was called by us
    QMap<QString, QListViewItem *> m_items;
    // per device: "pairing", "trusting", "connecting", or the result
    QMap<QString, QString> m_progress;
    QMap<QString, QString> m_state;
};

#endif
