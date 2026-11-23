/*
    This file is part of kbluetooth, the Bluetooth applet of the KDE3 revival

    The pairing agent: org.bluez.Agent1 on the connection of Bluez,
    registered with org.bluez.AgentManager1 as the default agent each time
    bluetoothd appears.  The questions go to the user in AgentDialogs; the
    D-Bus call stays open until the dialog is answered.

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.
*/

#ifndef KBLUETOOTH_AGENT_H
#define KBLUETOOTH_AGENT_H

#include <qguardedptr.h>
#include <qobject.h>

class AgentDialog;

class BluezAgent : public QObject
{
    Q_OBJECT
public:
    BluezAgent(QObject *parent = 0);
    virtual ~BluezAgent();

    // registered with bluetoothd as its default agent
    bool isRegistered() const { return m_registered; }

    // the D-Bus side, in agent.cpp
    friend struct BluezAgentPrivate;

private slots:
    void connectionReady();
    void connectionLost();
    void serviceChanged(bool available);
    void deviceChanged(const QString &path);
    void deviceRemoved(const QString &path);
    void callFinished(const QString &path, int operation,
                      const QString &errorName, const QString &error);
    void answered(bool accepted, const QString &text, bool always);
    void dialogDestroyed();

private:
    void request(void *invocation, const QString &method, void *parameters);
    // ends the open request with org.bluez.Error.Canceled
    void cancelRequest();
    void closeDisplay();

    unsigned m_object;
    bool m_registered;
    // the open request and its dialog
    void *m_invocation;     // GDBusMethodInvocation *
    QString m_method;
    QGuardedPtr<AgentDialog> m_dialog;
    // DisplayPinCode or DisplayPasskey: no answer, only shown
    QGuardedPtr<AgentDialog> m_display;
    QString m_displayValue;
};

#endif
