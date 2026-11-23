/*
    This file is part of kbluetooth, the Bluetooth applet of the KDE3 revival

    The client side of BlueZ 5, on GIO's GDBus.  kde3-qt runs the GLib
    main loop (QT_GLIB_EVENTLOOP), so the replies and signals of GDBus
    arrive in the Qt event loop; no call here waits for bluetoothd.

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.
*/

// GLib's headers must come before any Qt header: Qt 3 defines "signals"
// as a macro, which breaks GLib's declarations that use the word.
#include <gio/gio.h>

#include <string.h>

#include <qtimer.h>
#include <kdebug.h>
#include <klocale.h>

#include "bluez.h"

#ifndef QT_GLIB_EVENTLOOP
#error "kbluetooth needs Qt built with the GLib event loop (QT_GLIB_EVENTLOOP)"
#endif

static const char service[] = "org.bluez";
static const char adapterInterface[] = "org.bluez.Adapter1";
static const char deviceInterface[] = "org.bluez.Device1";
static const char batteryInterface[] = "org.bluez.Battery1";

static Bluez *s_self = 0;

// The context of one asynchronous call
struct BluezCall
{
    QString path;
    int operation;
    unsigned generation;
};

static QVariant toVariant(GVariant *value)
{
    if (g_variant_is_of_type(value, G_VARIANT_TYPE_BOOLEAN))
        return QVariant(bool(g_variant_get_boolean(value)), 0);
    if (g_variant_is_of_type(value, G_VARIANT_TYPE_BYTE))
        return QVariant(int(g_variant_get_byte(value)));
    if (g_variant_is_of_type(value, G_VARIANT_TYPE_INT16))
        return QVariant(int(g_variant_get_int16(value)));
    if (g_variant_is_of_type(value, G_VARIANT_TYPE_UINT16))
        return QVariant(uint(g_variant_get_uint16(value)));
    if (g_variant_is_of_type(value, G_VARIANT_TYPE_INT32))
        return QVariant(int(g_variant_get_int32(value)));
    if (g_variant_is_of_type(value, G_VARIANT_TYPE_UINT32))
        return QVariant(uint(g_variant_get_uint32(value)));
    if (g_variant_is_of_type(value, G_VARIANT_TYPE_STRING)
        || g_variant_is_of_type(value, G_VARIANT_TYPE_OBJECT_PATH))
        return QVariant(QString::fromUtf8(g_variant_get_string(value, 0)));
    if (g_variant_is_of_type(value, G_VARIANT_TYPE_STRING_ARRAY)) {
        QStringList list;
        gsize n = g_variant_n_children(value);
        for (gsize i = 0; i < n; ++i) {
            GVariant *child = g_variant_get_child_value(value, i);
            list.append(QString::fromUtf8(g_variant_get_string(child, 0)));
            g_variant_unref(child);
        }
        return QVariant(list);
    }
    // the other properties (ManufacturerData, ServiceData, ...) are not used
    return QVariant();
}

// The properties in an a{sv}
static BluezProperties toProperties(GVariant *dict)
{
    BluezProperties props;
    GVariantIter iter;
    const gchar *name;
    GVariant *value;
    g_variant_iter_init(&iter, dict);
    while (g_variant_iter_next(&iter, "{&sv}", &name, &value)) {
        QVariant v = toVariant(value);
        if (v.isValid())
            props.insert(QString::fromLatin1(name), v);
        g_variant_unref(value);
    }
    return props;
}

static QStringList toStringList(GVariant *array)
{
    QStringList list;
    GVariantIter iter;
    const gchar *s;
    g_variant_iter_init(&iter, array);
    while (g_variant_iter_next(&iter, "&s", &s))
        list.append(QString::fromUtf8(s));
    return list;
}

// The callbacks of GDBus; a friend of Bluez
struct BluezPrivate
{
    static void busReady(GObject *, GAsyncResult *result, gpointer)
    {
        GError *error = 0;
        GDBusConnection *connection = g_bus_get_finish(result, &error);
        if (!s_self) {
            if (connection)
                g_object_unref(connection);
            if (error)
                g_error_free(error);
            return;
        }
        if (!connection) {
            if (!g_error_matches(error, G_IO_ERROR, G_IO_ERROR_CANCELLED)) {
                s_self->m_busError = QString::fromUtf8(error->message);
                kdWarning() << "no system bus: " << s_self->m_busError << endl;
                emit s_self->serviceChanged(false);
                // try again later: the system bus may be restarting
                QTimer::singleShot(10000, s_self, SLOT(connectBus()));
            }
            g_error_free(error);
            return;
        }
        s_self->m_busError = QString::null;
        s_self->setConnection(connection);
    }

    static void closed(GDBusConnection *, gboolean, GError *, gpointer)
    {
        if (!s_self)
            return;
        kdWarning() << "the system bus closed the connection" << endl;
        s_self->dropConnection();
        QTimer::singleShot(5000, s_self, SLOT(connectBus()));
    }

    static void appeared(GDBusConnection *, const gchar *, const gchar *owner, gpointer)
    {
        if (!s_self)
            return;
        s_self->m_owner = QString::fromLatin1(owner);
        BluezCall *c = new BluezCall;
        c->operation = -1;
        c->generation = s_self->m_generation;
        g_dbus_connection_call(G_DBUS_CONNECTION(s_self->m_connection), service, "/",
                               "org.freedesktop.DBus.ObjectManager", "GetManagedObjects",
                               0, G_VARIANT_TYPE("(a{oa{sa{sv}}})"),
                               G_DBUS_CALL_FLAGS_NONE, -1,
                               G_CANCELLABLE(s_self->m_cancellable), managedObjects, c);
    }

    static void vanished(GDBusConnection *, const gchar *, gpointer)
    {
        if (!s_self)
            return;
        ++s_self->m_generation;
        s_self->m_owner = QString::null;
        bool was = s_self->m_available || !s_self->m_objects.isEmpty();
        s_self->clear();
        if (was) {
            emit s_self->adaptersChanged();
            emit s_self->serviceChanged(false);
        }
    }

    static void addObject(GVariant *interfaces, const QString &path, bool notify)
    {
        GVariantIter iter;
        const gchar *interface;
        GVariant *props;
        g_variant_iter_init(&iter, interfaces);
        while (g_variant_iter_next(&iter, "{&s@a{sv}}", &interface, &props)) {
            s_self->updateInterface(path, QString::fromLatin1(interface),
                                    toProperties(props), QStringList(), notify);
            g_variant_unref(props);
        }
    }

    static void managedObjects(GObject *source, GAsyncResult *result, gpointer data)
    {
        BluezCall *c = static_cast<BluezCall *>(data);
        GError *error = 0;
        GVariant *reply = g_dbus_connection_call_finish(G_DBUS_CONNECTION(source), result, &error);
        if (!s_self || c->generation != s_self->m_generation) {
            // bluetoothd went away in the meantime, or the applet quit
            if (reply)
                g_variant_unref(reply);
        } else if (!reply) {
            if (!g_error_matches(error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
                kdWarning() << "GetManagedObjects: " << error->message << endl;
        } else {
            GVariant *objects = g_variant_get_child_value(reply, 0);
            GVariantIter iter;
            const gchar *path;
            GVariant *interfaces;
            g_variant_iter_init(&iter, objects);
            while (g_variant_iter_next(&iter, "{&o@a{sa{sv}}}", &path, &interfaces)) {
                addObject(interfaces, QString::fromUtf8(path), false);
                g_variant_unref(interfaces);
            }
            g_variant_unref(objects);
            g_variant_unref(reply);
            s_self->m_available = true;
            emit s_self->adaptersChanged();
            emit s_self->serviceChanged(true);
        }
        if (error)
            g_error_free(error);
        delete c;
    }

    static void signal(GDBusConnection *, const gchar *, const gchar *objectPath,
                       const gchar *, const gchar *signalName,
                       GVariant *parameters, gpointer)
    {
        if (!s_self || !s_self->m_available)
            return;  // the reply of GetManagedObjects brings the state
        if (!strcmp(signalName, "PropertiesChanged")) {
            const gchar *interface;
            GVariant *changed, *invalidated;
            g_variant_get(parameters, "(&s@a{sv}@as)", &interface, &changed, &invalidated);
            QString path = QString::fromUtf8(objectPath);
            QString iface = QString::fromLatin1(interface);
            // only the interfaces known from the object manager
            if (s_self->m_objects.contains(path) && s_self->m_objects[path].contains(iface))
                s_self->updateInterface(path, iface, toProperties(changed),
                                        toStringList(invalidated), true);
            g_variant_unref(changed);
            g_variant_unref(invalidated);
        } else if (!strcmp(signalName, "InterfacesAdded")) {
            const gchar *path;
            GVariant *interfaces;
            g_variant_get(parameters, "(&o@a{sa{sv}})", &path, &interfaces);
            addObject(interfaces, QString::fromUtf8(path), true);
            g_variant_unref(interfaces);
        } else if (!strcmp(signalName, "InterfacesRemoved")) {
            const gchar *path;
            GVariant *interfaces;
            g_variant_get(parameters, "(&o@as)", &path, &interfaces);
            QStringList list = toStringList(interfaces);
            for (QStringList::ConstIterator it = list.begin(); it != list.end(); ++it)
                s_self->removeInterface(QString::fromUtf8(path), *it);
            g_variant_unref(interfaces);
        }
    }

    static void callDone(GObject *source, GAsyncResult *result, gpointer data)
    {
        BluezCall *c = static_cast<BluezCall *>(data);
        GError *error = 0;
        GVariant *reply = g_dbus_connection_call_finish(G_DBUS_CONNECTION(source), result, &error);
        if (reply)
            g_variant_unref(reply);
        if (s_self && !(error && g_error_matches(error, G_IO_ERROR, G_IO_ERROR_CANCELLED))) {
            QString name, message;
            if (error) {
                gchar *remote = g_dbus_error_get_remote_error(error);
                name = remote ? QString::fromLatin1(remote) : QString::fromLatin1("org.freedesktop.DBus.Error.Failed");
                g_free(remote);
                g_dbus_error_strip_remote_error(error);
                message = QString::fromUtf8(error->message);
                if (message.isEmpty())
                    message = name;
            }
            emit s_self->callFinished(c->path, c->operation, name, message);
        }
        if (error)
            g_error_free(error);
        delete c;
    }
};

Bluez *Bluez::self()
{
    if (!s_self)
        new Bluez;
    return s_self;
}

Bluez::Bluez()
    : QObject(0, "bluez"),
      m_connection(0), m_cancellable(0), m_watch(0), m_generation(0),
      m_available(false)
{
    s_self = this;
    m_subscriptions[0] = m_subscriptions[1] = m_subscriptions[2] = 0;
    m_cancellable = g_cancellable_new();
    connectBus();
}

Bluez::~Bluez()
{
    g_cancellable_cancel(G_CANCELLABLE(m_cancellable));
    dropConnection();
    g_object_unref(m_cancellable);
    s_self = 0;
}

void Bluez::connectBus()
{
    if (m_connection)
        return;
    // DBUS_SYSTEM_BUS_ADDRESS, when set, names the bus; the tests use it
    // to point kbluetooth at a mock BlueZ on a private bus
    g_bus_get(G_BUS_TYPE_SYSTEM, G_CANCELLABLE(m_cancellable), BluezPrivate::busReady, 0);
}

void Bluez::setConnection(void *conn)
{
    GDBusConnection *connection = G_DBUS_CONNECTION(conn);
    m_connection = connection;
    // a restart of the system bus must not end the applet
    g_dbus_connection_set_exit_on_close(connection, FALSE);
    g_signal_connect(connection, "closed", G_CALLBACK(BluezPrivate::closed), 0);

    // subscribe first, so that nothing between GetManagedObjects and its
    // reply is lost
    m_subscriptions[0] = g_dbus_connection_signal_subscribe(
        connection, service, "org.freedesktop.DBus.Properties", "PropertiesChanged",
        0, 0, G_DBUS_SIGNAL_FLAGS_NONE, BluezPrivate::signal, 0, 0);
    m_subscriptions[1] = g_dbus_connection_signal_subscribe(
        connection, service, "org.freedesktop.DBus.ObjectManager", "InterfacesAdded",
        0, 0, G_DBUS_SIGNAL_FLAGS_NONE, BluezPrivate::signal, 0, 0);
    m_subscriptions[2] = g_dbus_connection_signal_subscribe(
        connection, service, "org.freedesktop.DBus.ObjectManager", "InterfacesRemoved",
        0, 0, G_DBUS_SIGNAL_FLAGS_NONE, BluezPrivate::signal, 0, 0);
    m_watch = g_bus_watch_name_on_connection(connection, service, G_BUS_NAME_WATCHER_FLAGS_NONE,
                                             BluezPrivate::appeared, BluezPrivate::vanished, 0, 0);
    emit connectionReady();
}

void Bluez::dropConnection()
{
    if (!m_connection)
        return;
    GDBusConnection *connection = G_DBUS_CONNECTION(m_connection);
    if (m_watch)
        g_bus_unwatch_name(m_watch);
    m_watch = 0;
    for (int i = 0; i < 3; ++i) {
        if (m_subscriptions[i])
            g_dbus_connection_signal_unsubscribe(connection, m_subscriptions[i]);
        m_subscriptions[i] = 0;
    }
    g_signal_handlers_disconnect_by_func(connection, (gpointer)BluezPrivate::closed, 0);
    emit connectionLost();
    m_connection = 0;
    g_object_unref(connection);
    ++m_generation;
    m_owner = QString::null;
    bool was = m_available || !m_objects.isEmpty();
    clear();
    if (was && s_self) {
        emit adaptersChanged();
        emit serviceChanged(false);
    }
}

void Bluez::clear()
{
    m_available = false;
    QStringList devs;
    for (QMap<QString, Interfaces>::ConstIterator it = m_objects.begin(); it != m_objects.end(); ++it)
        if (it.data().contains(deviceInterface))
            devs.append(it.key());
    m_objects.clear();
    for (QStringList::ConstIterator it = devs.begin(); it != devs.end(); ++it)
        emit deviceRemoved(*it);
}

void Bluez::updateInterface(const QString &path, const QString &interface,
                            const BluezProperties &props, const QStringList &invalidated,
                            bool notify)
{
    if (interface != adapterInterface && interface != deviceInterface
        && interface != batteryInterface)
        return;
    bool isNew = !m_objects.contains(path) || !m_objects[path].contains(interface);
    BluezProperties &stored = m_objects[path][interface];
    bool wasConnected = stored.contains("Connected") && stored["Connected"].toBool();
    for (BluezProperties::ConstIterator it = props.begin(); it != props.end(); ++it)
        stored[it.key()] = it.data();
    for (QStringList::ConstIterator it = invalidated.begin(); it != invalidated.end(); ++it)
        stored.remove(*it);

    if (!notify)
        return;
    if (interface == adapterInterface) {
        emit adaptersChanged();
    } else if (interface == deviceInterface) {
        if (isNew)
            emit deviceAdded(path);
        else
            emit deviceChanged(path);
        bool connected = stored.contains("Connected") && stored["Connected"].toBool();
        if (!isNew && connected != wasConnected)
            emit connectionChanged(path, connected);
    } else if (hasInterface(path, deviceInterface)) {
        emit deviceChanged(path);  // the battery
    }
}

void Bluez::removeInterface(const QString &path, const QString &interface)
{
    if (!m_objects.contains(path) || !m_objects[path].contains(interface))
        return;
    m_objects[path].remove(interface);
    if (m_objects[path].isEmpty())
        m_objects.remove(path);
    if (interface == adapterInterface)
        emit adaptersChanged();
    else if (interface == deviceInterface)
        emit deviceRemoved(path);
    else if (interface == batteryInterface && hasInterface(path, deviceInterface))
        emit deviceChanged(path);
}

bool Bluez::hasInterface(const QString &path, const char *interface) const
{
    QMap<QString, Interfaces>::ConstIterator it = m_objects.find(path);
    return it != m_objects.end() && it.data().contains(interface);
}

QVariant Bluez::property(const QString &path, const char *interface, const char *name) const
{
    QMap<QString, Interfaces>::ConstIterator it = m_objects.find(path);
    if (it == m_objects.end())
        return QVariant();
    Interfaces::ConstIterator i = it.data().find(interface);
    if (i == it.data().end())
        return QVariant();
    BluezProperties::ConstIterator p = i.data().find(name);
    return p == i.data().end() ? QVariant() : p.data();
}

QStringList Bluez::adapters() const
{
    QStringList list;
    for (QMap<QString, Interfaces>::ConstIterator it = m_objects.begin(); it != m_objects.end(); ++it)
        if (it.data().contains(adapterInterface))
            list.append(it.key());
    list.sort();
    return list;
}

bool Bluez::adapter(const QString &path, BluezAdapter &result) const
{
    if (!hasInterface(path, adapterInterface))
        return false;
    result.path = path;
    result.address = property(path, adapterInterface, "Address").toString();
    result.name = property(path, adapterInterface, "Alias").toString();
    if (result.name.isEmpty())
        result.name = property(path, adapterInterface, "Name").toString();
    if (result.name.isEmpty())
        result.name = result.address;
    result.powered = property(path, adapterInterface, "Powered").toBool();
    result.discovering = property(path, adapterInterface, "Discovering").toBool();
    result.discoverable = property(path, adapterInterface, "Discoverable").toBool();
    result.powerState = property(path, adapterInterface, "PowerState").toString();
    return true;
}

QString Bluez::defaultAdapter() const
{
    QStringList list = adapters();
    if (!m_preferred.isEmpty()) {
        for (QStringList::ConstIterator it = list.begin(); it != list.end(); ++it)
            if (property(*it, adapterInterface, "Address").toString().upper() == m_preferred.upper())
                return *it;
    }
    return list.isEmpty() ? QString::null : list.first();
}

void Bluez::setPreferredAdapter(const QString &address)
{
    if (m_preferred == address)
        return;
    m_preferred = address;
    emit adaptersChanged();
}

QStringList Bluez::devices(const QString &adapterPath) const
{
    // sorted by name, then by path
    QMap<QString, QString> sorted;
    for (QMap<QString, Interfaces>::ConstIterator it = m_objects.begin(); it != m_objects.end(); ++it) {
        if (!it.data().contains(deviceInterface))
            continue;
        if (property(it.key(), deviceInterface, "Adapter").toString() != adapterPath)
            continue;
        BluezDevice d;
        device(it.key(), d);
        sorted.insert(d.name.lower() + QChar(0) + it.key(), it.key());
    }
    return sorted.values();
}

bool Bluez::device(const QString &path, BluezDevice &d) const
{
    if (!hasInterface(path, deviceInterface))
        return false;
    d.path = path;
    d.adapter = property(path, deviceInterface, "Adapter").toString();
    d.address = property(path, deviceInterface, "Address").toString();
    d.named = property(path, deviceInterface, "Name").isValid();
    d.name = property(path, deviceInterface, "Alias").toString();
    if (d.name.isEmpty())
        d.name = property(path, deviceInterface, "Name").toString();
    if (d.name.isEmpty())
        d.name = d.address;
    d.icon = property(path, deviceInterface, "Icon").toString();
    d.paired = property(path, deviceInterface, "Paired").toBool();
    d.trusted = property(path, deviceInterface, "Trusted").toBool();
    d.blocked = property(path, deviceInterface, "Blocked").toBool();
    d.connected = property(path, deviceInterface, "Connected").toBool();
    QVariant rssi = property(path, deviceInterface, "RSSI");
    d.hasRssi = rssi.isValid();
    d.rssi = rssi.toInt();
    QVariant battery = property(path, batteryInterface, "Percentage");
    d.battery = battery.isValid() ? battery.toInt() : -1;
    d.uuids = property(path, deviceInterface, "UUIDs").toStringList();
    return true;
}

void Bluez::call(const QString &path, const char *interface, const char *method,
                 void *parameters, int operation, int timeout)
{
    GVariant *params = static_cast<GVariant *>(parameters);
    if (!m_connection) {
        if (params)
            g_variant_unref(g_variant_ref_sink(params));
        emit callFinished(path, operation, QString::fromLatin1("org.freedesktop.DBus.Error.NoServer"),
                          i18n("The system bus is not available."));
        return;
    }
    BluezCall *c = new BluezCall;
    c->path = path;
    c->operation = operation;
    c->generation = m_generation;
    g_dbus_connection_call(G_DBUS_CONNECTION(m_connection), service, path.utf8(),
                           interface, method, params, 0, G_DBUS_CALL_FLAGS_NONE,
                           timeout, G_CANCELLABLE(m_cancellable), BluezPrivate::callDone, c);
}

void Bluez::setProperty(const QString &path, const char *interface, const char *name,
                        void *value, int operation)
{
    call(path, "org.freedesktop.DBus.Properties", "Set",
         g_variant_new("(ssv)", interface, name, static_cast<GVariant *>(value)),
         operation, -1);
}

void Bluez::setPowered(const QString &adapterPath, bool on)
{
    setProperty(adapterPath, adapterInterface, "Powered", g_variant_new_boolean(on), SetPowered);
}

void Bluez::startDiscovery(const QString &adapterPath)
{
    call(adapterPath, adapterInterface, "StartDiscovery", 0, StartDiscovery, -1);
}

void Bluez::stopDiscovery(const QString &adapterPath)
{
    call(adapterPath, adapterInterface, "StopDiscovery", 0, StopDiscovery, -1);
}

void Bluez::removeDevice(const QString &devicePath)
{
    QString adapterPath = property(devicePath, deviceInterface, "Adapter").toString();
    if (adapterPath.isEmpty())
        return;
    call(adapterPath, adapterInterface, "RemoveDevice",
         g_variant_new("(o)", devicePath.utf8().data()), Remove, -1);
}

// Connecting and pairing take seconds, pairing waits for the user too
static const int connectTimeout = 60 * 1000;
static const int pairTimeout = 120 * 1000;

void Bluez::connectDevice(const QString &devicePath)
{
    call(devicePath, deviceInterface, "Connect", 0, Connect, connectTimeout);
}

void Bluez::disconnectDevice(const QString &devicePath)
{
    call(devicePath, deviceInterface, "Disconnect", 0, Disconnect, connectTimeout);
}

void Bluez::pair(const QString &devicePath)
{
    call(devicePath, deviceInterface, "Pair", 0, Pair, pairTimeout);
}

void Bluez::cancelPairing(const QString &devicePath)
{
    call(devicePath, deviceInterface, "CancelPairing", 0, CancelPairing, -1);
}

void Bluez::setTrusted(const QString &devicePath, bool trusted)
{
    setProperty(devicePath, deviceInterface, "Trusted", g_variant_new_boolean(trusted), SetTrusted);
}

void Bluez::setAlias(const QString &devicePath, const QString &alias)
{
    setProperty(devicePath, deviceInterface, "Alias", g_variant_new_string(alias.utf8()), SetAlias);
}

QString Bluez::errorText(const QString &name, const QString &message)
{
    // the reasons of Connect in BlueZ 5.5x (src/error.h), then the names
    if (message == "br-connection-profile-unavailable" || message == "le-connection-profile-unavailable")
        return i18n("the device offers no service this computer can use");
    if (message.startsWith("br-connection-page-timeout") || message == "Page Timeout"
        || message == "le-connection-abort-by-local")
        return i18n("the device does not answer (out of range or switched off?)");
    if (message.startsWith("br-connection-canceled") || message.startsWith("le-connection-canceled"))
        return i18n("canceled");
    if (message.startsWith("br-connection-busy") || message == "br-connection-already-connected")
        return i18n("the device is busy");
    if (message.startsWith("br-connection-key-missing"))
        return i18n("the device forgot the pairing; remove it and pair it again");
    if (message.contains("rfkill") || name == "org.bluez.Error.Blocked")
        return i18n("Bluetooth is blocked (rfkill)");
    if (name == "org.bluez.Error.NotReady")
        return i18n("Bluetooth is off");
    if (name == "org.bluez.Error.AuthenticationCanceled" || name == "org.bluez.Error.Canceled")
        return i18n("canceled");
    if (name == "org.bluez.Error.AuthenticationRejected" || name == "org.bluez.Error.Rejected")
        return i18n("rejected");
    if (name == "org.bluez.Error.AuthenticationFailed")
        return i18n("wrong PIN or passkey");
    if (name == "org.bluez.Error.AuthenticationTimeout" || name == "org.freedesktop.DBus.Error.NoReply")
        return i18n("no answer from the device");
    if (name == "org.bluez.Error.ConnectionAttemptFailed")
        return i18n("the device cannot be reached");
    return message;
}

QString Bluez::profileName(const QString &uuid)
{
    // the 16 bit UUIDs on the Bluetooth base UUID
    QString u = uuid.lower();
    if (!u.endsWith("-0000-1000-8000-00805f9b34fb") || !u.startsWith("0000"))
        return QString::null;
    uint id = u.mid(4, 4).toUInt(0, 16);
    switch (id) {
    case 0x1101: return i18n("Serial port");
    case 0x1103: return i18n("Dial-up networking");
    case 0x1105: return i18n("Object push");
    case 0x1106: return i18n("File transfer");
    case 0x1108: case 0x1112: return i18n("Headset (HSP)");
    case 0x110a: return i18n("Audio source (A2DP)");
    case 0x110b: return i18n("Audio sink (A2DP)");
    case 0x110c: case 0x110e: case 0x110f: return i18n("Remote control (AVRCP)");
    case 0x1115: case 0x1116: return i18n("Personal area network");
    case 0x111e: case 0x111f: return i18n("Hands-free (HFP)");
    case 0x1124: return i18n("Input device (HID)");
    case 0x112f: return i18n("Phone book access");
    case 0x1132: case 0x1133: case 0x1134: return i18n("Message access");
    case 0x180f: return i18n("Battery");
    case 0x1812: return i18n("Input device (HID over GATT)");
    case 0x184e: case 0x184f: case 0x1850: case 0x1853: return i18n("LE Audio");
    default: return QString::null;
    }
}

#include "bluez.moc"
