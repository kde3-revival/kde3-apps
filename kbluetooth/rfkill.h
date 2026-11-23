/*
    This file is part of kbluetooth, the Bluetooth applet of the KDE3 revival

    The rfkill state of the Bluetooth radios, from /sys/class/rfkill,
    read again on each event of /dev/rfkill (or every few seconds when
    /dev/rfkill cannot be opened).  Unblocking needs privileges; the
    applet only tells the user.

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.
*/

#ifndef KBLUETOOTH_RFKILL_H
#define KBLUETOOTH_RFKILL_H

#include <qmap.h>
#include <qobject.h>

class QSocketNotifier;
class QTimer;

class RfKill : public QObject
{
    Q_OBJECT
public:
    enum State { NoRadio, Unblocked, SoftBlocked, HardBlocked };

    RfKill(QObject *parent = 0);
    virtual ~RfKill();

    // for the case without an adapter: the least blocked of all Bluetooth
    // radios (one unblocked radio makes it Unblocked)
    State state() const { return m_state; }
    // the state of one adapter ("hci0"): its own radio, blocked also by a
    // switch of the platform (thinkpad_acpi, ...), which blocks them all
    State state(const QString &adapter) const;

signals:
    void changed(RfKill::State state);

private slots:
    void readEvents();
    void update();

private:
    QString m_dir;
    int m_fd;
    QSocketNotifier *m_notifier;
    QTimer *m_timer;
    State m_state;
    QMap<QString, State> m_radios;  // name (hci0, tpacpi_bluetooth_sw) -> state
};

#endif
