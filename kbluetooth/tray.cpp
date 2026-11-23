/*
    This file is part of kbluetooth, the Bluetooth applet of the KDE3 revival

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.
*/

#include <qstylesheet.h>
#include <qtooltip.h>

#include <kaction.h>
#include <kapplication.h>
#include <kconfig.h>
#include <kiconloader.h>
#include <kinputdialog.h>
#include <klocale.h>
#include <kmessagebox.h>
#include <knotifyclient.h>
#include <kpopupmenu.h>
#include <kstdaction.h>
#include <kstdguiitem.h>
#include <kwin.h>

#include "agent.h"
#include "bluez.h"
#include "icons.h"
#include "popup.h"
#include "scandialog.h"
#include "tray.h"

BluetoothTray::BluetoothTray()
    : KSystemTray(0, "kbluetooth"),
      m_nextId(1000), m_agent(0), m_rfkill(0), m_iconState(-1), m_forceShown(false),
      m_showWithoutAdapter(true), m_powerOnLogin(false), m_autostart(true)
{
    m_subMenus.setAutoDelete(true);
    readSettings();

    Bluez *bluez = Bluez::self();
    bluez->setPreferredAdapter(m_preferredAdapter);
    m_agent = new BluezAgent(this);
    m_rfkill = new RfKill(this);

    connect(bluez, SIGNAL(serviceChanged(bool)), SLOT(serviceChanged(bool)));
    connect(bluez, SIGNAL(adaptersChanged()), SLOT(updateState()));
    connect(bluez, SIGNAL(deviceAdded(const QString &)), SLOT(deviceChanged(const QString &)));
    connect(bluez, SIGNAL(deviceRemoved(const QString &)), SLOT(deviceChanged(const QString &)));
    connect(bluez, SIGNAL(deviceChanged(const QString &)), SLOT(deviceChanged(const QString &)));
    connect(bluez, SIGNAL(connectionChanged(const QString &, bool)),
            SLOT(connectionChanged(const QString &, bool)));
    connect(bluez, SIGNAL(callFinished(const QString &, int, const QString &, const QString &)),
            SLOT(callFinished(const QString &, int, const QString &, const QString &)));
    connect(m_rfkill, SIGNAL(changed(RfKill::State)), SLOT(rfkillChanged(RfKill::State)));

    updateState();
}

BluetoothTray::~BluetoothTray()
{
    delete m_popup;
    delete m_scan;
}

void BluetoothTray::readSettings()
{
    KConfig *config = kapp->config();
    KConfigGroupSaver saver(config, "General");
    m_showWithoutAdapter = config->readBoolEntry("ShowWithoutAdapter", true);
    m_powerOnLogin = config->readBoolEntry("PowerOnLogin", false);
    m_autostart = config->readBoolEntry("Autostart", true);
    m_preferredAdapter = config->readEntry("Adapter");
}

void BluetoothTray::writeSettings()
{
    KConfig *config = kapp->config();
    KConfigGroupSaver saver(config, "General");
    config->writeEntry("ShowWithoutAdapter", m_showWithoutAdapter);
    config->writeEntry("PowerOnLogin", m_powerOnLogin);
    // read by the autostart file (X-KDE-autostart-condition)
    config->writeEntry("Autostart", m_autostart);
    config->writeEntry("Adapter", m_preferredAdapter);
    config->sync();
}

QString BluetoothTray::adapterPath() const
{
    return Bluez::self()->defaultAdapter();
}

RfKill::State BluetoothTray::rfkillState() const
{
    return m_rfkill->state();
}

QString BluetoothTray::problem() const
{
    Bluez *bluez = Bluez::self();
    if (!bluez->connection() && !bluez->busError().isEmpty())
        return i18n("The system message bus (D-Bus) cannot be reached: %1").arg(bluez->busError());
    if (m_rfkill->state() == RfKill::HardBlocked)
        return i18n("Bluetooth is switched off with a hardware switch or key of the computer.");
    if (!bluez->isAvailable())
        return i18n("The Bluetooth service (bluetoothd) is not running.");
    BluezAdapter adapter;
    if (!bluez->adapter(adapterPath(), adapter)) {
        if (m_rfkill->state() == RfKill::SoftBlocked)
            return i18n("Bluetooth is blocked (rfkill). Unblocking it needs administrator "
                        "rights, for example: rfkill unblock bluetooth");
        return i18n("No Bluetooth adapter was found.");
    }
    if (m_rfkill->state() == RfKill::SoftBlocked || adapter.powerState == "off-blocked")
        return i18n("Bluetooth is blocked (rfkill). Unblocking it needs administrator "
                    "rights, for example: rfkill unblock bluetooth");
    return QString::null;
}

QString BluetoothTray::busyText(const QString &devicePath) const
{
    QMap<QString, QString>::ConstIterator it = m_busy.find(devicePath);
    return it == m_busy.end() ? QString::null : it.data();
}

QString BluetoothTray::lastError(const QString &devicePath) const
{
    QMap<QString, QString>::ConstIterator it = m_errors.find(devicePath);
    return it == m_errors.end() ? QString::null : it.data();
}

void BluetoothTray::updateState()
{
    Bluez *bluez = Bluez::self();
    BluezAdapter adapter;
    bool haveAdapter = bluez->adapter(adapterPath(), adapter);
    KBluetoothIcons::State state = KBluetoothIcons::Off;
    QStringList connected;
    if (haveAdapter && adapter.powered) {
        state = KBluetoothIcons::On;
        QStringList devices = bluez->devices(adapter.path);
        for (QStringList::ConstIterator it = devices.begin(); it != devices.end(); ++it) {
            BluezDevice d;
            if (bluez->device(*it, d) && d.connected) {
                state = KBluetoothIcons::Connected;
                connected.append(d.battery >= 0 ? i18n("device name, battery", "%1 (%2%)")
                                 .arg(d.name).arg(d.battery) : d.name);
            }
        }
    }

    if (int(state) != m_iconState) {
        KConfig *config = kapp->config();
        KConfigGroupSaver saver(config, "System Tray");
        int size = config->readNumEntry("systrayIconWidth", 22);
        setPixmap(KBluetoothIcons::trayIcon(state, size));
        m_iconState = state;
    }

    QString tip;
    QString why = problem();
    if (!why.isEmpty())
        tip = why;
    else if (!adapter.powered)
        tip = i18n("Bluetooth is off");
    else if (connected.isEmpty())
        tip = i18n("Bluetooth is on, no device is connected");
    else
        tip = i18n("Connected: %1").arg(connected.join(", "));
    QToolTip::remove(this);
    QToolTip::add(this, QString::fromLatin1("<qt><b>%1</b><br>%2</qt>")
                  .arg(i18n("Bluetooth")).arg(QStyleSheet::escape(tip)));

    // without an adapter the icon may hide itself
    if (haveAdapter)
        m_forceShown = false;
    bool show = haveAdapter || m_showWithoutAdapter || m_forceShown;
    if (show && !isVisible())
        this->show();
    else if (!show && isVisible())
        hide();

    if (haveAdapter && m_powerOnLogin)
        powerOnLogin();

    emit stateChanged();
}

void BluetoothTray::powerOnLogin()
{
    // once per adapter and session: switching it off later stays off
    BluezAdapter adapter;
    if (!Bluez::self()->adapter(adapterPath(), adapter) || m_poweredOnce.contains(adapter.address))
        return;
    m_poweredOnce.append(adapter.address);
    if (!adapter.powered && m_rfkill->state() != RfKill::HardBlocked)
        Bluez::self()->setPowered(adapter.path, true);
}

void BluetoothTray::serviceChanged(bool available)
{
    if (!available) {
        m_busy.clear();
        m_errors.clear();
    }
    updateState();
}

void BluetoothTray::deviceChanged(const QString &)
{
    updateState();
}

void BluetoothTray::rfkillChanged(RfKill::State)
{
    updateState();
}

void BluetoothTray::connectionChanged(const QString &path, bool connected)
{
    BluezDevice d;
    if (!Bluez::self()->device(path, d))
        return;
    if (connected)
        m_errors.remove(path);
    // switching the adapter off disconnects all: no notification for each
    BluezAdapter adapter;
    if (!connected && (!Bluez::self()->adapter(d.adapter, adapter) || !adapter.powered
                       || adapter.powerState == "on-disabling"))
        return;
    if (connected)
        notify("connected", i18n("%1 is connected.").arg(d.name));
    else
        notify("disconnected", i18n("%1 is disconnected.").arg(d.name));
}

void BluetoothTray::notify(const char *event, const QString &text)
{
    KNotifyClient::event(winId(), QString::fromLatin1(event), text);
}

void BluetoothTray::callFinished(const QString &path, int operation,
                                 const QString &errorName, const QString &error)
{
    if (operation == Bluez::Connect || operation == Bluez::Disconnect)
        m_busy.remove(path);
    if (!error.isEmpty() && operation != Bluez::CancelPairing
        && operation != Bluez::StartDiscovery && operation != Bluez::StopDiscovery) {
        QString text = Bluez::errorText(errorName, error);
        m_errors[path] = text;
        BluezDevice d;
        BluezAdapter a;
        QString name = Bluez::self()->device(path, d) ? d.name
                       : Bluez::self()->adapter(path, a) ? a.name : path;
        // the scan dialog shows the errors of its pairing steps itself
        bool scanning = m_scan && m_scan->isVisible()
                        && (operation == Bluez::Pair || operation == Bluez::SetTrusted
                            || operation == Bluez::Connect);
        if (!scanning)
            notify("error", i18n("%1: %2").arg(name).arg(text));
    } else if (error.isEmpty()) {
        m_errors.remove(path);
    }
    updateState();
}

void BluetoothTray::mousePressEvent(QMouseEvent *e)
{
    if (e->button() == LeftButton && rect().contains(e->pos())) {
        if (m_popup && m_popup->isVisible())
            m_popup->hide();
        else
            showPopup();
        return;
    }
    KSystemTray::mousePressEvent(e);
}

void BluetoothTray::mouseReleaseEvent(QMouseEvent *e)
{
    KSystemTray::mouseReleaseEvent(e);
}

void BluetoothTray::showPopup()
{
    if (!m_popup) {
        m_popup = new DevicePopup(this);
    }
    m_popup->popupAt(this);
}

void BluetoothTray::activate()
{
    m_forceShown = true;
    if (!isVisible())
        show();
    showPopup();
}

void BluetoothTray::scanForDevices()
{
    if (m_popup)
        m_popup->hide();
    if (!m_scan)
        m_scan = new ScanDialog(this);
    m_scan->show();
    m_scan->raise();
    KWin::activateWindow(m_scan->winId());
}

void BluetoothTray::setPowered(bool on)
{
    QString path = adapterPath();
    if (!path.isEmpty())
        Bluez::self()->setPowered(path, on);
}

void BluetoothTray::toggleConnection(const QString &path)
{
    BluezDevice d;
    if (!Bluez::self()->device(path, d) || !busyText(path).isEmpty())
        return;
    if (d.connected)
        disconnectDevice(path);
    else
        connectDevice(path);
}

void BluetoothTray::connectDevice(const QString &path)
{
    m_errors.remove(path);
    m_busy[path] = i18n("Connecting...");
    Bluez::self()->connectDevice(path);
    emit stateChanged();
}

void BluetoothTray::disconnectDevice(const QString &path)
{
    m_errors.remove(path);
    m_busy[path] = i18n("Disconnecting...");
    Bluez::self()->disconnectDevice(path);
    emit stateChanged();
}

void BluetoothTray::renameDevice(const QString &path)
{
    BluezDevice d;
    if (!Bluez::self()->device(path, d))
        return;
    bool ok = false;
    QString name = KInputDialog::getText(i18n("Rename Device"),
                                         i18n("New name of %1 (%2):").arg(d.name).arg(d.address),
                                         d.name, &ok, 0);
    // an empty alias gives back the name of the device itself
    if (ok && Bluez::self()->device(path, d))
        Bluez::self()->setAlias(path, name.stripWhiteSpace());
}

void BluetoothTray::removeDevice(const QString &path)
{
    BluezDevice d;
    if (!Bluez::self()->device(path, d))
        return;
    int answer = KMessageBox::warningContinueCancel(0,
        i18n("<qt>Remove <b>%1</b>?<br>The pairing is deleted; to use the device "
             "again it must be paired again.</qt>").arg(QStyleSheet::escape(d.name)),
        i18n("Remove Device"), KGuiItem(i18n("&Remove"), "editdelete"));
    if (answer == KMessageBox::Continue && Bluez::self()->device(path, d))
        Bluez::self()->removeDevice(path);
}

void BluetoothTray::setTrusted(const QString &path, bool trusted)
{
    Bluez::self()->setTrusted(path, trusted);
}

int BluetoothTray::addAction(KPopupMenu *menu, const QString &text, const QString &icon,
                             Action action, const QString &arg)
{
    int id = m_nextId++;
    if (icon.isEmpty())
        menu->insertItem(text, id);
    else
        menu->insertItem(SmallIconSet(icon), text, id);
    MenuEntry entry;
    entry.action = action;
    entry.arg = arg;
    m_menuEntries.insert(id, entry);
    return id;
}

KPopupMenu *BluetoothTray::deviceMenu(const QString &path, QWidget *parent)
{
    BluezDevice d;
    KPopupMenu *menu = new KPopupMenu(parent);
    if (!Bluez::self()->device(path, d))
        return menu;
    menu->insertTitle(KBluetoothIcons::deviceIcon(d.icon, 16), d.name);
    if (d.connected)
        addAction(menu, i18n("&Disconnect"), "connect_no", ActDisconnect, path);
    else
        addAction(menu, i18n("&Connect"), "connect_established", ActConnect, path);
    menu->setItemEnabled(m_nextId - 1, busyText(path).isEmpty());
    addAction(menu, i18n("Re&name..."), "edit", ActRename, path);
    int trust = addAction(menu, i18n("&Trusted"), QString::null, d.trusted ? ActUntrust : ActTrust, path);
    menu->setItemChecked(trust, d.trusted);
    menu->insertSeparator();
    addAction(menu, i18n("&Remove..."), "editdelete", ActRemove, path);
    connect(menu, SIGNAL(activated(int)), SLOT(menuActivated(int)));
    return menu;
}

void BluetoothTray::contextMenuAboutToShow(KPopupMenu *menu)
{
    menu->clear();
    m_subMenus.clear();
    m_menuEntries.clear();
    menu->insertTitle(*pixmap(), i18n("Bluetooth"));

    Bluez *bluez = Bluez::self();
    BluezAdapter adapter;
    bool haveAdapter = bluez->adapter(adapterPath(), adapter);
    QString why = problem();

    bool powered = haveAdapter && adapter.powered;
    int power = menu->insertItem(i18n("Bluetooth &On"), this,
                                 powered ? SLOT(powerOff()) : SLOT(powerOn()));
    menu->setItemChecked(power, powered);
    menu->setItemEnabled(power, haveAdapter && why.isEmpty());

    if (haveAdapter) {
        QStringList devices = bluez->devices(adapter.path);
        bool any = false;
        for (QStringList::ConstIterator it = devices.begin(); it != devices.end(); ++it) {
            BluezDevice d;
            if (!bluez->device(*it, d) || !d.paired)
                continue;
            if (!any)
                menu->insertSeparator();
            any = true;
            KPopupMenu *sub = deviceMenu(*it, menu);
            m_subMenus.append(sub);
            QString text = d.name;
            if (d.connected)
                text = i18n("device name, connected", "%1 (connected)").arg(d.name);
            menu->insertItem(QIconSet(KBluetoothIcons::deviceIcon(d.icon, 16)), text, sub);
        }
        menu->insertSeparator();
        int scan = addAction(menu, i18n("&Scan for Devices..."), "find", ActScan, QString::null);
        menu->setItemEnabled(scan, adapter.powered);
    }

    QStringList adapters = bluez->adapters();
    if (adapters.count() > 1) {
        KPopupMenu *sub = new KPopupMenu(menu);
        m_subMenus.append(sub);
        for (QStringList::ConstIterator it = adapters.begin(); it != adapters.end(); ++it) {
            BluezAdapter a;
            if (!bluez->adapter(*it, a))
                continue;
            int id = addAction(sub, i18n("adapter name, address", "%1 (%2)").arg(a.name).arg(a.address),
                               QString::null, ActAdapter, a.address);
            sub->setItemChecked(id, *it == adapter.path);
        }
        connect(sub, SIGNAL(activated(int)), SLOT(menuActivated(int)));
        menu->insertItem(i18n("&Adapter"), sub);
    }

    KPopupMenu *settings = new KPopupMenu(menu);
    m_subMenus.append(settings);
    int id = addAction(settings, i18n("Show the Icon Without an &Adapter"), QString::null,
                       ActShowWithoutAdapter, QString::null);
    settings->setItemChecked(id, m_showWithoutAdapter);
    id = addAction(settings, i18n("Switch Bluetooth On at &Login"), QString::null,
                   ActPowerOnLogin, QString::null);
    settings->setItemChecked(id, m_powerOnLogin);
    id = addAction(settings, i18n("&Start kbluetooth at Login"), QString::null,
                   ActAutostart, QString::null);
    settings->setItemChecked(id, m_autostart);
    connect(settings, SIGNAL(activated(int)), SLOT(menuActivated(int)));
    menu->insertSeparator();
    menu->insertItem(SmallIconSet("configure"), i18n("S&ettings"), settings);
    connect(menu, SIGNAL(activated(int)), SLOT(menuActivated(int)));

    menu->insertSeparator();
    KAction *quit = actionCollection()->action(KStdAction::name(KStdAction::Quit));
    if (quit)
        quit->plug(menu);
}

void BluetoothTray::menuActivated(int id)
{
    QMap<int, MenuEntry>::ConstIterator it = m_menuEntries.find(id);
    if (it == m_menuEntries.end())
        return;
    MenuEntry entry = it.data();
    // a parent menu may report the item of a submenu again
    m_menuEntries.remove(id);
    switch (entry.action) {
    case ActConnect:
        connectDevice(entry.arg);
        break;
    case ActDisconnect:
        disconnectDevice(entry.arg);
        break;
    case ActRename:
        renameDevice(entry.arg);
        break;
    case ActTrust:
        setTrusted(entry.arg, true);
        break;
    case ActUntrust:
        setTrusted(entry.arg, false);
        break;
    case ActRemove:
        removeDevice(entry.arg);
        break;
    case ActScan:
        scanForDevices();
        break;
    case ActAdapter:
        m_preferredAdapter = entry.arg;
        writeSettings();
        Bluez::self()->setPreferredAdapter(entry.arg);
        break;
    case ActShowWithoutAdapter:
        m_showWithoutAdapter = !m_showWithoutAdapter;
        m_forceShown = false;
        writeSettings();
        if (!m_showWithoutAdapter && adapterPath().isEmpty())
            KMessageBox::information(0, i18n("There is no Bluetooth adapter now, so the icon "
                                              "hides itself. It comes back when an adapter is "
                                              "plugged in, or when kbluetooth is started again."),
                                     i18n("Bluetooth"), "kbluetooth_hidden");
        updateState();
        break;
    case ActPowerOnLogin:
        m_powerOnLogin = !m_powerOnLogin;
        writeSettings();
        break;
    case ActAutostart:
        m_autostart = !m_autostart;
        writeSettings();
        break;
    }
}

void BluetoothTray::powerOn()
{
    setPowered(true);
}

void BluetoothTray::powerOff()
{
    setPowered(false);
}

#include "tray.moc"
