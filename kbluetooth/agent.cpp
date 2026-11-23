/*
    This file is part of kbluetooth, the Bluetooth applet of the KDE3 revival

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.
*/

// GLib's headers must come before any Qt header (Qt 3's "signals" macro)
#include <gio/gio.h>

#include <string.h>

#include <kdebug.h>
#include <klocale.h>

#include "agent.h"
#include "agentdialog.h"
#include "bluez.h"

static const char agentPath[] = "/org/kde3/kbluetooth/agent";
static const char rejected[] = "org.bluez.Error.Rejected";
static const char canceled[] = "org.bluez.Error.Canceled";

static const char introspection[] =
    "<node>"
    " <interface name='org.bluez.Agent1'>"
    "  <method name='Release'/>"
    "  <method name='RequestPinCode'>"
    "   <arg type='o' name='device' direction='in'/>"
    "   <arg type='s' name='pincode' direction='out'/>"
    "  </method>"
    "  <method name='DisplayPinCode'>"
    "   <arg type='o' name='device' direction='in'/>"
    "   <arg type='s' name='pincode' direction='in'/>"
    "  </method>"
    "  <method name='RequestPasskey'>"
    "   <arg type='o' name='device' direction='in'/>"
    "   <arg type='u' name='passkey' direction='out'/>"
    "  </method>"
    "  <method name='DisplayPasskey'>"
    "   <arg type='o' name='device' direction='in'/>"
    "   <arg type='u' name='passkey' direction='in'/>"
    "   <arg type='q' name='entered' direction='in'/>"
    "  </method>"
    "  <method name='RequestConfirmation'>"
    "   <arg type='o' name='device' direction='in'/>"
    "   <arg type='u' name='passkey' direction='in'/>"
    "  </method>"
    "  <method name='RequestAuthorization'>"
    "   <arg type='o' name='device' direction='in'/>"
    "  </method>"
    "  <method name='AuthorizeService'>"
    "   <arg type='o' name='device' direction='in'/>"
    "   <arg type='s' name='uuid' direction='in'/>"
    "  </method>"
    "  <method name='Cancel'/>"
    " </interface>"
    "</node>";

static BluezAgent *s_agent = 0;

static QString deviceName(const QString &path)
{
    BluezDevice d;
    if (Bluez::self()->device(path, d))
        return d.name;
    // the last part of the path has the address: dev_AA_BB_...
    QString name = path.section('/', -1);
    if (name.startsWith("dev_"))
        name = name.mid(4).replace('_', ':');
    return name;
}

static QString passkeyText(guint32 passkey)
{
    return QString().sprintf("%06u", passkey);
}

struct BluezAgentPrivate
{
    static void methodCall(GDBusConnection *, const gchar *sender, const gchar *,
                           const gchar *, const gchar *method, GVariant *parameters,
                           GDBusMethodInvocation *invocation, gpointer)
    {
        if (!s_agent) {
            g_dbus_method_invocation_return_dbus_error(invocation, rejected, "The agent is gone");
            return;
        }
        // only bluetoothd may ask
        QString owner = Bluez::self()->serviceOwner();
        if (owner.isEmpty() || owner != QString::fromLatin1(sender)) {
            g_dbus_method_invocation_return_dbus_error(invocation, rejected,
                                                       "Only bluetoothd may call the agent");
            return;
        }
        s_agent->request(invocation, QString::fromLatin1(method), parameters);
    }

    // the reply belongs to the bluetoothd and connection of now: not to one
    // that went away while the call ran
    static bool current(GObject *source, gpointer generation)
    {
        return s_agent && Bluez::self()->connection() == (void *)source
               && Bluez::self()->generation() == GPOINTER_TO_UINT(generation);
    }

    static void registered(GObject *source, GAsyncResult *result, gpointer generation)
    {
        GError *error = 0;
        GVariant *reply = g_dbus_connection_call_finish(G_DBUS_CONNECTION(source), result, &error);
        if (reply)
            g_variant_unref(reply);
        bool already = false;
        if (error) {
            gchar *remote = g_dbus_error_get_remote_error(error);
            already = remote && !strcmp(remote, "org.bluez.Error.AlreadyExists");
            if (!already)
                kdWarning() << "RegisterAgent: " << error->message << endl;
            g_free(remote);
        }
        if ((!error || already) && current(source, generation)) {
            g_dbus_connection_call(G_DBUS_CONNECTION(Bluez::self()->connection()),
                                   "org.bluez", "/org/bluez", "org.bluez.AgentManager1",
                                   "RequestDefaultAgent", g_variant_new("(o)", agentPath),
                                   0, G_DBUS_CALL_FLAGS_NONE, -1, 0, defaultAgent, generation);
        }
        if (error)
            g_error_free(error);
    }

    static void defaultAgent(GObject *source, GAsyncResult *result, gpointer generation)
    {
        GError *error = 0;
        GVariant *reply = g_dbus_connection_call_finish(G_DBUS_CONNECTION(source), result, &error);
        if (reply)
            g_variant_unref(reply);
        if (error) {
            kdWarning() << "RequestDefaultAgent: " << error->message << endl;
            g_error_free(error);
        } else if (current(source, generation)) {
            s_agent->m_registered = true;
            kdDebug() << "the default agent of bluetoothd" << endl;
        }
    }
};

static const GDBusInterfaceVTable vtable = { BluezAgentPrivate::methodCall, 0, 0, { 0 } };

BluezAgent::BluezAgent(QObject *parent)
    : QObject(parent, "agent"), m_object(0), m_registered(false), m_invocation(0)
{
    s_agent = this;
    Bluez *bluez = Bluez::self();
    connect(bluez, SIGNAL(connectionReady()), SLOT(connectionReady()));
    connect(bluez, SIGNAL(connectionLost()), SLOT(connectionLost()));
    connect(bluez, SIGNAL(serviceChanged(bool)), SLOT(serviceChanged(bool)));
    connect(bluez, SIGNAL(deviceChanged(const QString &)), SLOT(deviceChanged(const QString &)));
    connect(bluez, SIGNAL(deviceRemoved(const QString &)), SLOT(deviceRemoved(const QString &)));
    connect(bluez, SIGNAL(callFinished(const QString &, int, const QString &, const QString &)),
            SLOT(callFinished(const QString &, int, const QString &, const QString &)));
    if (bluez->connection())
        connectionReady();
    if (bluez->isAvailable())
        serviceChanged(true);
}

BluezAgent::~BluezAgent()
{
    cancelRequest();
    closeDisplay();
    connectionLost();
    s_agent = 0;
}

void BluezAgent::connectionReady()
{
    if (m_object)
        return;
    GError *error = 0;
    GDBusNodeInfo *info = g_dbus_node_info_new_for_xml(introspection, &error);
    if (!info) {
        kdWarning() << "agent introspection: " << error->message << endl;
        g_error_free(error);
        return;
    }
    m_object = g_dbus_connection_register_object(G_DBUS_CONNECTION(Bluez::self()->connection()),
                                                 agentPath, info->interfaces[0], &vtable,
                                                 0, 0, &error);
    g_dbus_node_info_unref(info);
    if (!m_object) {
        kdWarning() << "agent object: " << error->message << endl;
        g_error_free(error);
    }
}

void BluezAgent::connectionLost()
{
    cancelRequest();
    closeDisplay();
    m_registered = false;
    if (m_object && Bluez::self()->connection())
        g_dbus_connection_unregister_object(G_DBUS_CONNECTION(Bluez::self()->connection()), m_object);
    m_object = 0;
}

void BluezAgent::serviceChanged(bool available)
{
    if (!available) {
        // bluetoothd is gone: nothing can be answered any more
        cancelRequest();
        closeDisplay();
        m_registered = false;
        return;
    }
    if (!m_object || !Bluez::self()->connection())
        return;
    // KeyboardDisplay: the computer can show a passkey and type one
    g_dbus_connection_call(G_DBUS_CONNECTION(Bluez::self()->connection()),
                           "org.bluez", "/org/bluez", "org.bluez.AgentManager1",
                           "RegisterAgent", g_variant_new("(os)", agentPath, "KeyboardDisplay"),
                           0, G_DBUS_CALL_FLAGS_NONE, -1, 0, BluezAgentPrivate::registered,
                           GUINT_TO_POINTER(Bluez::self()->generation()));
}

void BluezAgent::request(void *inv, const QString &method, void *params)
{
    GDBusMethodInvocation *invocation = static_cast<GDBusMethodInvocation *>(inv);
    GVariant *parameters = static_cast<GVariant *>(params);

    if (method == "Release") {
        // bluetoothd unregistered the agent
        m_registered = false;
        cancelRequest();
        closeDisplay();
        g_dbus_method_invocation_return_value(invocation, 0);
        return;
    }
    if (method == "Cancel") {
        cancelRequest();
        closeDisplay();
        g_dbus_method_invocation_return_value(invocation, 0);
        return;
    }

    const gchar *device = 0;
    g_variant_get_child(parameters, 0, "&o", &device);
    QString path = QString::fromUtf8(device);
    QString name = deviceName(path);

    if (method == "DisplayPinCode" || method == "DisplayPasskey") {
        QString value;
        int entered = -1;
        AgentDialog::Kind kind;
        if (method == "DisplayPinCode") {
            const gchar *pin = 0;
            g_variant_get_child(parameters, 1, "&s", &pin);
            value = QString::fromUtf8(pin);
            kind = AgentDialog::DisplayPinCode;
        } else {
            guint32 passkey = 0;
            guint16 typed = 0;
            g_variant_get(parameters, "(&ouq)", &device, &passkey, &typed);
            value = passkeyText(passkey);
            entered = typed;
            kind = AgentDialog::DisplayPasskey;
        }
        // DisplayPasskey comes again for each digit typed on the device
        if (!m_display || m_display->devicePath() != path || m_display->kind() != kind
            || m_displayValue != value) {
            closeDisplay();
            m_display = new AgentDialog(kind, path, name, value);
            m_displayValue = value;
            connect(m_display, SIGNAL(answered(bool, const QString &, bool)),
                    SLOT(answered(bool, const QString &, bool)));
            m_display->show();
        }
        if (entered >= 0)
            m_display->setEntered(value, entered);
        g_dbus_method_invocation_return_value(invocation, 0);
        return;
    }

    AgentDialog::Kind kind;
    QString value;
    if (method == "RequestPinCode") {
        kind = AgentDialog::PinCode;
    } else if (method == "RequestPasskey") {
        kind = AgentDialog::Passkey;
    } else if (method == "RequestConfirmation") {
        guint32 passkey = 0;
        g_variant_get(parameters, "(&ou)", &device, &passkey);
        kind = AgentDialog::Confirm;
        value = passkeyText(passkey);
    } else if (method == "RequestAuthorization") {
        kind = AgentDialog::Authorize;
    } else if (method == "AuthorizeService") {
        const gchar *uuid = 0;
        g_variant_get_child(parameters, 1, "&s", &uuid);
        kind = AgentDialog::AuthorizeService;
        value = QString::fromLatin1(uuid);
    } else {
        g_dbus_method_invocation_return_dbus_error(invocation, "org.freedesktop.DBus.Error.UnknownMethod",
                                                   "Unknown method");
        return;
    }

    // one question at a time: a new one ends the old one
    cancelRequest();
    m_invocation = invocation;
    m_method = method;
    m_dialog = new AgentDialog(kind, path, name, value);
    connect(m_dialog, SIGNAL(answered(bool, const QString &, bool)),
            SLOT(answered(bool, const QString &, bool)));
    connect(m_dialog, SIGNAL(destroyed()), SLOT(dialogDestroyed()));
    m_dialog->show();
}

void BluezAgent::answered(bool accepted, const QString &text, bool always)
{
    const QObject *dialog = sender();
    if (dialog && dialog == (AgentDialog *)m_display) {
        // "Cancel Pairing" while the PIN or passkey is shown
        QString path = m_display->devicePath();
        m_display = 0;
        if (!accepted)
            Bluez::self()->cancelPairing(path);
        return;
    }
    if (!m_invocation || !dialog || dialog != (AgentDialog *)m_dialog)
        return;
    GDBusMethodInvocation *invocation = static_cast<GDBusMethodInvocation *>(m_invocation);
    QString path = m_dialog->devicePath();
    m_invocation = 0;
    m_dialog = 0;
    if (!accepted) {
        g_dbus_method_invocation_return_dbus_error(invocation, rejected, "Rejected by the user");
        return;
    }
    if (m_method == "RequestPinCode") {
        g_dbus_method_invocation_return_value(invocation, g_variant_new("(s)", text.utf8().data()));
    } else if (m_method == "RequestPasskey") {
        g_dbus_method_invocation_return_value(invocation, g_variant_new("(u)", text.toUInt()));
    } else {
        if (m_method == "AuthorizeService" && always)
            Bluez::self()->setTrusted(path, true);
        g_dbus_method_invocation_return_value(invocation, 0);
    }
}

void BluezAgent::dialogDestroyed()
{
    // the dialog went away without an answer
    if (m_invocation && !m_dialog) {
        g_dbus_method_invocation_return_dbus_error(static_cast<GDBusMethodInvocation *>(m_invocation),
                                                   rejected, "Rejected by the user");
        m_invocation = 0;
    }
}

void BluezAgent::cancelRequest()
{
    if (m_dialog) {
        AgentDialog *dialog = m_dialog;
        m_dialog = 0;
        dialog->disconnect(this);
        dialog->close();
    }
    if (m_invocation) {
        g_dbus_method_invocation_return_dbus_error(static_cast<GDBusMethodInvocation *>(m_invocation),
                                                   canceled, "Canceled");
        m_invocation = 0;
    }
}

void BluezAgent::closeDisplay()
{
    if (!m_display)
        return;
    AgentDialog *dialog = m_display;
    m_display = 0;
    dialog->disconnect(this);
    dialog->close();
}

void BluezAgent::deviceChanged(const QString &path)
{
    // the PIN or passkey was typed on the device: paired
    if (!m_display || m_display->devicePath() != path)
        return;
    BluezDevice d;
    if (Bluez::self()->device(path, d) && d.paired)
        closeDisplay();
}

void BluezAgent::deviceRemoved(const QString &path)
{
    // the device went away (removed, or out of range): bluetoothd does not
    // always cancel what the agent shows or asks for it
    if (m_display && m_display->devicePath() == path)
        closeDisplay();
    if (m_invocation && m_dialog && m_dialog->devicePath() == path)
        cancelRequest();
}

void BluezAgent::callFinished(const QString &path, int operation, const QString &, const QString &)
{
    if (operation == Bluez::Pair && m_display && m_display->devicePath() == path)
        closeDisplay();
}

#include "agent.moc"
