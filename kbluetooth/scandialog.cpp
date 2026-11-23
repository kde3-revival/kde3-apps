/*
    This file is part of kbluetooth, the Bluetooth applet of the KDE3 revival

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.
*/

#include <string.h>

#include <qlabel.h>
#include <qlayout.h>
#include <qpushbutton.h>

#include <kiconloader.h>
#include <klistview.h>
#include <klocale.h>

#include "bluez.h"
#include "icons.h"
#include "scandialog.h"
#include "tray.h"

// The steps of "Pair": pair, trust, connect
static const char statePairing[] = "pairing";
static const char stateTrusting[] = "trusting";
static const char stateConnecting[] = "connecting";
static const char stateDone[] = "done";
static const char stateFailed[] = "failed";

// a text for the errors of Pair the user can do something about
static QString pairError(const QString &name, const QString &error)
{
    if (name == "org.bluez.Error.AuthenticationCanceled" || name == "org.bluez.Error.Canceled")
        return i18n("Canceled");
    if (name == "org.bluez.Error.AuthenticationRejected" || name == "org.bluez.Error.Rejected")
        return i18n("Rejected");
    if (name == "org.bluez.Error.AuthenticationFailed")
        return i18n("Wrong PIN or passkey");
    if (name == "org.bluez.Error.AuthenticationTimeout" || name == "org.freedesktop.DBus.Error.NoReply")
        return i18n("No answer from the device");
    if (name == "org.bluez.Error.ConnectionAttemptFailed")
        return i18n("The device cannot be reached");
    return error;
}

ScanDialog::ScanDialog(BluetoothTray *tray)
    : KDialogBase(0, "scandialog", false, i18n("Bluetooth Devices"),
                  User1 | Close, User1, true, KGuiItem(i18n("&Pair"), "connect_creating")),
      m_tray(tray), m_discovering(false)
{
    QWidget *page = new QWidget(this);
    setMainWidget(page);
    QVBoxLayout *layout = new QVBoxLayout(page, 0, spacingHint());

    QLabel *hint = new QLabel(i18n("Switch the device to pairing mode (it must be visible), "
                                   "then pick it and press Pair. A paired device is trusted "
                                   "and connected."), page);
    hint->setAlignment(WordBreak | AlignLeft | AlignTop);
    layout->addWidget(hint);

    m_list = new KListView(page);
    m_list->addColumn(i18n("Device"));
    m_list->addColumn(i18n("Type"));
    m_list->addColumn(i18n("Signal"));
    m_list->addColumn(i18n("Status"));
    m_list->setAllColumnsShowFocus(true);
    m_list->setSorting(0);
    m_list->setMinimumSize(460, 220);
    layout->addWidget(m_list, 1);

    m_status = new QLabel(page);
    layout->addWidget(m_status);

    Bluez *bluez = Bluez::self();
    connect(bluez, SIGNAL(adaptersChanged()), SLOT(refresh()));
    connect(bluez, SIGNAL(serviceChanged(bool)), SLOT(refresh()));
    connect(bluez, SIGNAL(deviceAdded(const QString &)), SLOT(deviceAdded(const QString &)));
    connect(bluez, SIGNAL(deviceRemoved(const QString &)), SLOT(deviceRemoved(const QString &)));
    connect(bluez, SIGNAL(deviceChanged(const QString &)), SLOT(deviceChanged(const QString &)));
    connect(bluez, SIGNAL(callFinished(const QString &, int, const QString &, const QString &)),
            SLOT(callFinished(const QString &, int, const QString &, const QString &)));
    connect(m_list, SIGNAL(selectionChanged()), SLOT(selectionChanged()));
    connect(m_list, SIGNAL(executed(QListViewItem *)), SLOT(executed(QListViewItem *)));

    selectionChanged();
}

ScanDialog::~ScanDialog()
{
    stopDiscovery();
}

void ScanDialog::showEvent(QShowEvent *e)
{
    KDialogBase::showEvent(e);
    if (e->spontaneous())
        return;
    m_items.clear();
    m_list->clear();
    m_state.clear();
    refresh();
}

void ScanDialog::hideEvent(QHideEvent *e)
{
    KDialogBase::hideEvent(e);
    if (!e->spontaneous())
        stopDiscovery();
}

void ScanDialog::startDiscovery()
{
    if (m_discovering || m_adapter.isEmpty())
        return;
    m_discovering = true;
    Bluez::self()->startDiscovery(m_adapter);
}

void ScanDialog::stopDiscovery()
{
    if (!m_discovering)
        return;
    m_discovering = false;
    // bluetoothd ends the discovery of a client by itself when it leaves
    BluezAdapter a;
    if (Bluez::self()->adapter(m_adapter, a))
        Bluez::self()->stopDiscovery(m_adapter);
}

void ScanDialog::refresh()
{
    if (!isVisible())
        return;
    Bluez *bluez = Bluez::self();
    QString path = m_tray->adapterPath();
    if (path != m_adapter) {
        stopDiscovery();
        m_adapter = path;
        m_items.clear();
        m_list->clear();
    }
    BluezAdapter adapter;
    if (!bluez->adapter(m_adapter, adapter) || !adapter.powered) {
        m_discovering = false;
        QString why = m_tray->problem();
        m_status->setText(why.isEmpty() ? i18n("Bluetooth is off.") : why);
        selectionChanged();
        return;
    }
    startDiscovery();
    m_status->setText(adapter.discovering ? i18n("Looking for devices...")
                                          : i18n("Not looking for devices."));

    QStringList devices = bluez->devices(m_adapter);
    for (QStringList::ConstIterator it = devices.begin(); it != devices.end(); ++it)
        updateItem(*it);
    selectionChanged();
}

void ScanDialog::updateItem(const QString &path)
{
    BluezDevice d;
    if (!Bluez::self()->device(path, d) || d.adapter != m_adapter)
        return;
    // the devices paired before are in the popup; the ones paired here stay
    if (d.paired && !m_state.contains(path)) {
        if (m_items.contains(path)) {
            delete m_items[path];
            m_items.remove(path);
        }
        return;
    }
    QListViewItem *item = m_items.contains(path) ? m_items[path] : 0;
    if (!item) {
        item = new QListViewItem(m_list);
        m_items.insert(path, item);
    }
    item->setPixmap(0, KBluetoothIcons::deviceIcon(d.icon, 16));
    item->setText(0, d.name == d.address ? d.address : i18n("device name, address", "%1 (%2)")
                  .arg(d.name).arg(d.address));
    item->setText(1, KBluetoothIcons::deviceType(d.icon));
    item->setText(2, d.hasRssi ? i18n("%1 dBm").arg(d.rssi) : QString::fromLatin1("-"));

    QString status;
    QString state = m_state.contains(path) ? m_state[path] : QString::null;
    if (state == statePairing)
        status = i18n("Pairing...");
    else if (state == stateTrusting)
        status = i18n("Paired, trusting...");
    else if (state == stateConnecting)
        status = i18n("Paired, connecting...");
    else if (state == stateDone)
        status = d.connected ? i18n("Paired and connected") : i18n("Paired");
    else if (state.startsWith(stateFailed))
        status = state.mid(strlen(stateFailed) + 1);
    else
        status = d.connected ? i18n("Connected") : QString::null;
    item->setText(3, status);
}

QString ScanDialog::selectedPath() const
{
    QListViewItem *item = m_list->selectedItem();
    for (QMap<QString, QListViewItem *>::ConstIterator it = m_items.begin(); it != m_items.end(); ++it)
        if (it.data() == item)
            return it.key();
    return QString::null;
}

void ScanDialog::selectionChanged()
{
    QString path = selectedPath();
    QString state = m_state.contains(path) ? m_state[path] : QString::null;
    if (state == statePairing) {
        setButtonGuiItem(User1, KGuiItem(i18n("&Cancel Pairing"), "button_cancel"));
        enableButton(User1, true);
        return;
    }
    setButtonGuiItem(User1, KGuiItem(i18n("&Pair"), "connect_creating"));
    BluezDevice d;
    bool ok = !path.isEmpty() && Bluez::self()->device(path, d) && !d.paired
              && (state.isEmpty() || state.startsWith(stateFailed));
    enableButton(User1, ok);
}

void ScanDialog::executed(QListViewItem *)
{
    if (actionButton(User1)->isEnabled() && m_state[selectedPath()] != statePairing)
        slotUser1();
}

void ScanDialog::slotUser1()
{
    QString path = selectedPath();
    if (path.isEmpty())
        return;
    if (m_state.contains(path) && m_state[path] == statePairing) {
        Bluez::self()->cancelPairing(path);
        return;
    }
    m_state[path] = statePairing;
    Bluez::self()->pair(path);
    updateItem(path);
    selectionChanged();
}

void ScanDialog::deviceAdded(const QString &path)
{
    if (isVisible())
        updateItem(path);
}

void ScanDialog::deviceRemoved(const QString &path)
{
    if (m_items.contains(path)) {
        delete m_items[path];
        m_items.remove(path);
    }
    m_state.remove(path);
    selectionChanged();
}

void ScanDialog::deviceChanged(const QString &path)
{
    if (!isVisible())
        return;
    updateItem(path);
    selectionChanged();
}

void ScanDialog::callFinished(const QString &path, int operation,
                              const QString &errorName, const QString &error)
{
    if (operation == Bluez::StartDiscovery && path == m_adapter && !error.isEmpty()) {
        m_discovering = false;
        // InProgress: another program discovers already, which is fine
        if (errorName != "org.bluez.Error.InProgress")
            m_status->setText(i18n("Looking for devices failed: %1").arg(error));
        return;
    }
    if (!m_state.contains(path))
        return;
    QString state = m_state[path];
    if (operation == Bluez::Pair && state == statePairing) {
        if (error.isEmpty() || errorName == "org.bluez.Error.AlreadyExists") {
            m_state[path] = stateTrusting;
            Bluez::self()->setTrusted(path, true);
        } else {
            m_state[path] = QString::fromLatin1("%1:%2").arg(stateFailed)
                            .arg(i18n("Pairing failed: %1").arg(pairError(errorName, error)));
        }
    } else if (operation == Bluez::SetTrusted && state == stateTrusting) {
        // connect even when trusting failed: the pairing is there
        m_state[path] = stateConnecting;
        m_tray->connectDevice(path);
    } else if (operation == Bluez::Connect && state == stateConnecting) {
        if (error.isEmpty())
            m_state[path] = stateDone;
        else
            m_state[path] = QString::fromLatin1("%1:%2").arg(stateFailed)
                            .arg(i18n("Paired; connecting failed: %1").arg(error));
    } else {
        return;
    }
    updateItem(path);
    selectionChanged();
}

#include "scandialog.moc"
