/*
    This file is part of kbluetooth, the Bluetooth applet of the KDE3 revival

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.
*/

#include <qcheckbox.h>
#include <qlabel.h>
#include <qlayout.h>
#include <qstylesheet.h>
#include <qtooltip.h>
#include <qvbox.h>

#include <kapplication.h>
#include <kdialog.h>
#include <kglobalsettings.h>
#include <kiconloader.h>
#include <klocale.h>
#include <kpopupmenu.h>
#include <kpushbutton.h>

#include "bluez.h"
#include "icons.h"
#include "popup.h"
#include "tray.h"

DeviceRow::DeviceRow(const QString &path, QWidget *parent)
    : QFrame(parent, "devicerow"), m_path(path)
{
    QHBoxLayout *layout = new QHBoxLayout(this, 3, KDialog::spacingHint());
    m_icon = new QLabel(this);
    m_icon->setFixedSize(22, 22);
    m_icon->setAlignment(AlignCenter);
    layout->addWidget(m_icon);
    m_text = new QLabel(this);
    m_text->setTextFormat(RichText);
    layout->addWidget(m_text, 1);
}

void DeviceRow::update(BluetoothTray *tray)
{
    BluezDevice d;
    if (!Bluez::self()->device(m_path, d))
        return;
    m_icon->setPixmap(KBluetoothIcons::deviceIcon(d.icon, 22));

    QStringList status;
    QString busy = tray->busyText(m_path);
    QString error = tray->lastError(m_path);
    if (!busy.isEmpty())
        status.append(busy);
    else if (d.connected)
        status.append(i18n("Connected"));
    else if (!error.isEmpty())
        status.append(i18n("Failed: %1").arg(error));
    else
        status.append(i18n("Not connected"));
    if (d.battery >= 0)
        status.append(i18n("Battery %1%").arg(d.battery));
    m_text->setText(QString::fromLatin1("<qt><nobr><b>%1</b></nobr><br><nobr><small>%2</small></nobr></qt>")
                    .arg(QStyleSheet::escape(d.name))
                    .arg(QStyleSheet::escape(status.join(QString::fromUtf8(" \xc2\xb7 ")))));

    // the details in the tooltip, with the profiles BlueZ knows
    QStringList profiles;
    for (QStringList::ConstIterator it = d.uuids.begin(); it != d.uuids.end(); ++it) {
        QString name = Bluez::profileName(*it);
        if (!name.isEmpty() && !profiles.contains(name))
            profiles.append(name);
    }
    QString tip = QString::fromLatin1("<qt><b>%1</b><br>%2<br>%3")
                  .arg(QStyleSheet::escape(d.name))
                  .arg(QStyleSheet::escape(KBluetoothIcons::deviceType(d.icon)))
                  .arg(d.address);
    if (d.trusted)
        tip += "<br>" + i18n("Trusted");
    if (!profiles.isEmpty())
        tip += "<br>" + i18n("Profiles: %1").arg(QStyleSheet::escape(profiles.join(", ")));
    tip += "<br><i>" + (d.connected ? i18n("Click to disconnect") : i18n("Click to connect"))
           + "</i></qt>";
    QToolTip::remove(this);
    QToolTip::add(this, tip);
}

void DeviceRow::mouseReleaseEvent(QMouseEvent *e)
{
    if (!rect().contains(e->pos()))
        return;
    if (e->button() == LeftButton)
        emit clicked(m_path);
    else if (e->button() == RightButton)
        emit menuRequested(m_path, e->globalPos());
}

void DeviceRow::enterEvent(QEvent *)
{
    setHighlighted(true);
}

void DeviceRow::leaveEvent(QEvent *)
{
    setHighlighted(false);
}

void DeviceRow::setHighlighted(bool on)
{
    if (!on) {
        unsetPalette();
        return;
    }
    QPalette p = palette();
    p.setColor(QPalette::Active, QColorGroup::Background, KGlobalSettings::highlightColor());
    p.setColor(QPalette::Active, QColorGroup::Foreground, KGlobalSettings::highlightedTextColor());
    p.setColor(QPalette::Inactive, QColorGroup::Background, KGlobalSettings::highlightColor());
    p.setColor(QPalette::Inactive, QColorGroup::Foreground, KGlobalSettings::highlightedTextColor());
    setPalette(p);
}

DevicePopup::DevicePopup(BluetoothTray *tray)
    : QFrame(0, "kbluetooth_popup", WType_Popup), m_tray(tray), m_anchor(0)
{
    setFrameStyle(QFrame::PopupPanel | QFrame::Raised);
    setLineWidth(1);
    m_rows.setAutoDelete(true);

    QVBoxLayout *layout = new QVBoxLayout(this, KDialog::marginHint(), KDialog::spacingHint());
    m_title = new QLabel(this);
    m_title->setTextFormat(RichText);
    layout->addWidget(m_title);

    m_power = new QCheckBox(i18n("Bluetooth &on"), this);
    layout->addWidget(m_power);
    connect(m_power, SIGNAL(toggled(bool)), SLOT(powerToggled(bool)));

    m_message = new QLabel(this);
    m_message->setTextFormat(RichText);
    m_message->setAlignment(WordBreak | AlignLeft | AlignTop);
    m_message->setMinimumWidth(240);
    setMinimumWidth(260);
    layout->addWidget(m_message);

    QFrame *line = new QFrame(this);
    line->setFrameStyle(QFrame::HLine | QFrame::Sunken);
    layout->addWidget(line);

    m_devicesLabel = new QLabel(i18n("<b>Paired devices</b>"), this);
    layout->addWidget(m_devicesLabel);
    m_devices = new QVBox(this);
    layout->addWidget(m_devices);
    m_none = new QLabel(i18n("<i>No paired devices</i>"), this);
    layout->addWidget(m_none);

    QFrame *line2 = new QFrame(this);
    line2->setFrameStyle(QFrame::HLine | QFrame::Sunken);
    layout->addWidget(line2);

    m_scan = new KPushButton(SmallIconSet("find"), i18n("&Scan for Devices..."), this);
    m_scan->setFlat(true);
    layout->addWidget(m_scan);
    connect(m_scan, SIGNAL(clicked()), SLOT(scan()));

    connect(tray, SIGNAL(stateChanged()), SLOT(refresh()));
    refresh();
}

void DevicePopup::popupAt(QWidget *icon)
{
    m_anchor = icon;
    refresh();
    place();
    show();
}

void DevicePopup::place()
{
    adjustSize();
    QSize size = sizeHint().expandedTo(minimumSizeHint());
    resize(size);
    if (!m_anchor)
        return;
    QRect desk = KGlobalSettings::desktopGeometry(m_anchor);
    QPoint at = m_anchor->mapToGlobal(QPoint(0, 0));
    int x = at.x() + m_anchor->width() / 2 - size.width() / 2;
    int y = at.y() + m_anchor->height();
    // above the icon when the panel is at the bottom
    if (y + size.height() > desk.bottom())
        y = at.y() - size.height();
    x = QMAX(desk.left(), QMIN(x, desk.right() - size.width()));
    y = QMAX(desk.top(), y);
    move(x, y);
}

void DevicePopup::refresh()
{
    // popupAt() refreshes before it shows the popup
    if (!isVisible() && sender())
        return;
    Bluez *bluez = Bluez::self();
    BluezAdapter adapter;
    bool haveAdapter = bluez->adapter(m_tray->adapterPath(), adapter);
    QString why = m_tray->problem();

    QString title = QString::fromLatin1("<b>%1</b>").arg(i18n("Bluetooth"));
    if (haveAdapter)
        title += QString::fromLatin1(" &mdash; %1").arg(QStyleSheet::escape(adapter.name));
    m_title->setText("<nobr>" + title + "</nobr>");

    m_power->blockSignals(true);
    m_power->setChecked(haveAdapter && adapter.powered);
    m_power->setEnabled(haveAdapter && why.isEmpty());
    m_power->blockSignals(false);

    if (!why.isEmpty()) {
        m_message->setText(QStyleSheet::escape(why));
        m_message->show();
    } else if (haveAdapter && adapter.powerState == "off-enabling") {
        m_message->setText(i18n("Switching on..."));
        m_message->show();
    } else {
        m_message->hide();
    }

    // the paired devices of the adapter
    QStringList paths;
    if (haveAdapter) {
        QStringList devices = bluez->devices(adapter.path);
        for (QStringList::ConstIterator it = devices.begin(); it != devices.end(); ++it) {
            BluezDevice d;
            if (bluez->device(*it, d) && d.paired)
                paths.append(*it);
        }
    }
    QStringList shown;
    for (DeviceRow *row = m_rows.first(); row; row = m_rows.next())
        shown.append(row->path());
    if (shown != paths) {
        m_rows.clear();
        for (QStringList::ConstIterator it = paths.begin(); it != paths.end(); ++it) {
            DeviceRow *row = new DeviceRow(*it, m_devices);
            connect(row, SIGNAL(clicked(const QString &)), SLOT(rowClicked(const QString &)));
            connect(row, SIGNAL(menuRequested(const QString &, const QPoint &)),
                    SLOT(rowMenu(const QString &, const QPoint &)));
            m_rows.append(row);
            row->show();
        }
    }
    for (DeviceRow *row = m_rows.first(); row; row = m_rows.next()) {
        row->update(m_tray);
        row->setEnabled(adapter.powered);
    }
    m_devicesLabel->setShown(haveAdapter);
    m_devices->setShown(haveAdapter && !paths.isEmpty());
    m_none->setShown(haveAdapter && paths.isEmpty());
    m_scan->setEnabled(haveAdapter && adapter.powered);

    if (isVisible())
        place();
}

void DevicePopup::keyPressEvent(QKeyEvent *e)
{
    // a popup frame does not close on Escape by itself
    if (e->key() == Key_Escape) {
        hide();
        return;
    }
    QFrame::keyPressEvent(e);
}

void DevicePopup::powerToggled(bool on)
{
    m_tray->setPowered(on);
}

void DevicePopup::rowClicked(const QString &path)
{
    m_tray->toggleConnection(path);
}

void DevicePopup::rowMenu(const QString &path, const QPoint &pos)
{
    KPopupMenu *menu = m_tray->deviceMenu(path, 0);
    // the popup closes when the menu opens; the dialogs come after
    hide();
    menu->exec(pos);
    delete menu;
}

void DevicePopup::scan()
{
    hide();
    m_tray->scanForDevices();
}

#include "popup.moc"
