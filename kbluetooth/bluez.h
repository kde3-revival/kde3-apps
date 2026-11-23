/*
    This file is part of kbluetooth, the Bluetooth applet of the KDE3 revival

    The client side of BlueZ 5 (org.bluez on the system D-Bus): the
    adapters, devices and batteries BlueZ exports through its
    ObjectManager, kept up to date from InterfacesAdded, InterfacesRemoved
    and PropertiesChanged, and the asynchronous calls on them.

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.
*/

#ifndef KBLUETOOTH_BLUEZ_H
#define KBLUETOOTH_BLUEZ_H

#include <qmap.h>
#include <qobject.h>
#include <qstringlist.h>
#include <qvariant.h>

// One adapter (org.bluez.Adapter1)
struct BluezAdapter
{
    QString path;
    QString address;
    QString name;           // Alias
    bool powered;
    bool discovering;
    bool discoverable;
    QString powerState;     // PowerState of BlueZ 5.64+, empty before
};

// One device (org.bluez.Device1, with org.bluez.Battery1 when present)
struct BluezDevice
{
    QString path;
    QString adapter;
    QString address;
    QString name;           // Alias, which falls back to Name or the address
    QString icon;           // freedesktop icon name BlueZ derives from the class
    bool paired;
    bool trusted;
    bool blocked;
    bool connected;
    bool hasRssi;
    int rssi;
    int battery;            // percent, -1 without Battery1
    QStringList uuids;
};

typedef QMap<QString, QVariant> BluezProperties;

class Bluez : public QObject
{
    Q_OBJECT
public:
    // The asynchronous operations; callFinished() names them
    enum Operation {
        SetPowered, StartDiscovery, StopDiscovery, Connect, Disconnect,
        Pair, CancelPairing, SetTrusted, SetAlias, Remove
    };

    static Bluez *self();
    virtual ~Bluez();

    // bluetoothd owns org.bluez on the bus and its objects are known
    bool isAvailable() const { return m_available; }
    // the unique bus name of bluetoothd, to check who calls the agent
    QString serviceOwner() const { return m_owner; }
    // the system bus could not be reached
    QString busError() const { return m_busError; }

    QStringList adapters() const;
    bool adapter(const QString &path, BluezAdapter &result) const;
    // The adapter to use: the one whose address was set with
    // setPreferredAdapter(), else the first one (hci0 before hci1)
    QString defaultAdapter() const;
    void setPreferredAdapter(const QString &address);

    // The devices of an adapter, sorted by name
    QStringList devices(const QString &adapterPath) const;
    bool device(const QString &path, BluezDevice &result) const;

    void setPowered(const QString &adapterPath, bool on);
    void startDiscovery(const QString &adapterPath);
    void stopDiscovery(const QString &adapterPath);
    void removeDevice(const QString &devicePath);

    void connectDevice(const QString &devicePath);
    void disconnectDevice(const QString &devicePath);
    void pair(const QString &devicePath);
    void cancelPairing(const QString &devicePath);
    void setTrusted(const QString &devicePath, bool trusted);
    void setAlias(const QString &devicePath, const QString &alias);

    // The connection, a GDBusConnection *, for the agent; 0 before it is up
    void *connection() const { return m_connection; }

    // A readable text for an error of BlueZ
    static QString errorText(const QString &errorName, const QString &message);

    // Names for the profile UUIDs a device announces
    static QString profileName(const QString &uuid);

signals:
    // the connection to the system bus is up, or went away
    void connectionReady();
    void connectionLost();
    // bluetoothd appeared (after its objects are read) or vanished
    void serviceChanged(bool available);
    // an adapter was added or removed, or a property of one changed
    void adaptersChanged();
    void deviceAdded(const QString &path);
    void deviceRemoved(const QString &path);
    void deviceChanged(const QString &path);
    // Connected of a device changed after the initial state was read
    void connectionChanged(const QString &path, bool connected);
    // An operation started with one of the calls above ended; error is
    // empty on success, errorName is the D-Bus error name
    void callFinished(const QString &path, int operation,
                      const QString &errorName, const QString &error);

private slots:
    void connectBus();

private:
    Bluez();

    void setConnection(void *connection);
    void dropConnection();

    void call(const QString &path, const char *interface, const char *method,
              void *parameters, int operation, int timeout);
    void setProperty(const QString &path, const char *interface,
                     const char *name, void *value, int operation);

    // The callbacks of GDBus, in bluez.cpp
    friend struct BluezPrivate;

    void clear();
    void updateInterface(const QString &path, const QString &interface,
                         const BluezProperties &props, const QStringList &invalidated,
                         bool notify);
    void removeInterface(const QString &path, const QString &interface);
    QVariant property(const QString &path, const char *interface, const char *name) const;
    bool hasInterface(const QString &path, const char *interface) const;

    typedef QMap<QString, BluezProperties> Interfaces;
    QMap<QString, Interfaces> m_objects;

    void *m_connection;     // GDBusConnection *
    void *m_cancellable;    // GCancellable *
    unsigned m_watch;
    unsigned m_subscriptions[3];
    unsigned m_generation;  // bumped when bluetoothd vanishes
    bool m_available;
    QString m_busError;
    QString m_preferred;
    QString m_owner;
};

#endif
