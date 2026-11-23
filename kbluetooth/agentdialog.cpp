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
#include <qregexp.h>
#include <qstylesheet.h>
#include <qvalidator.h>

#include <kapplication.h>
#include <kglobalsettings.h>
#include <kiconloader.h>
#include <klineedit.h>
#include <klocale.h>
#include <kstdguiitem.h>
#include <kwin.h>

#include "agentdialog.h"
#include "bluez.h"

static int buttonsFor(AgentDialog::Kind kind)
{
    switch (kind) {
    case AgentDialog::Confirm:
    case AgentDialog::Authorize:
    case AgentDialog::AuthorizeService:
        return KDialogBase::Yes | KDialogBase::No;
    case AgentDialog::PinCode:
    case AgentDialog::Passkey:
        return KDialogBase::Ok | KDialogBase::Cancel;
    case AgentDialog::DisplayPinCode:
    case AgentDialog::DisplayPasskey:
        break;
    }
    return KDialogBase::Cancel;
}

static KDialogBase::ButtonCode defaultFor(AgentDialog::Kind kind)
{
    int buttons = buttonsFor(kind);
    if (buttons & KDialogBase::Yes)
        return KDialogBase::Yes;
    if (buttons & KDialogBase::Ok)
        return KDialogBase::Ok;
    return KDialogBase::Cancel;
}

AgentDialog::AgentDialog(Kind kind, const QString &devicePath, const QString &deviceName,
                         const QString &value)
    : KDialogBase(0, "agentdialog", false, i18n("Bluetooth Pairing"),
                  buttonsFor(kind), defaultFor(kind), true),
      m_kind(kind), m_devicePath(devicePath), m_value(0), m_edit(0), m_always(0),
      m_answered(false)
{
    if (kind == AuthorizeService)
        setCaption(i18n("Bluetooth Service"));

    QWidget *page = new QWidget(this);
    setMainWidget(page);
    QHBoxLayout *top = new QHBoxLayout(page, 0, spacingHint());
    QLabel *icon = new QLabel(page);
    icon->setPixmap(DesktopIcon("kbluetooth", KIcon::SizeLarge));
    icon->setAlignment(AlignTop);
    top->addWidget(icon);
    QVBoxLayout *box = new QVBoxLayout(top, spacingHint());

    QString name = QString::fromLatin1("<b>%1</b>").arg(QStyleSheet::escape(deviceName));
    QString text;
    switch (kind) {
    case Confirm:
        text = i18n("%1 wants to pair with this computer.<br>"
                    "Does the device show the same passkey?").arg(name);
        break;
    case Authorize:
        text = i18n("%1 wants to pair with this computer.<br>"
                    "Allow the pairing?").arg(name);
        break;
    case AuthorizeService: {
        QString service = Bluez::profileName(value);
        if (service.isEmpty())
            service = value;
        text = i18n("%1 wants to use the service <b>%2</b>.<br>"
                    "Allow it?").arg(name).arg(QStyleSheet::escape(service));
        break;
    }
    case PinCode:
        text = i18n("Type the PIN code of %1.<br>"
                    "Many headsets and car kits use 0000 or 1234.").arg(name);
        break;
    case Passkey:
        text = i18n("Type the six-digit passkey %1 shows.").arg(name);
        break;
    case DisplayPinCode:
        text = i18n("Type this PIN code on %1 and press Enter there:").arg(name);
        break;
    case DisplayPasskey:
        text = i18n("Type this passkey on %1 and press Enter there:").arg(name);
        break;
    }
    QLabel *label = new QLabel(text, page);
    label->setTextFormat(RichText);
    box->addWidget(label);

    if (kind == Confirm || kind == DisplayPinCode || kind == DisplayPasskey) {
        m_value = new QLabel(page);
        QFont f = KGlobalSettings::fixedFont();
        f.setPointSize(f.pointSize() * 2);
        f.setBold(true);
        m_value->setFont(f);
        m_value->setAlignment(AlignCenter);
        m_value->setText(value);
        box->addWidget(m_value);
    }
    if (kind == PinCode || kind == Passkey) {
        m_edit = new KLineEdit(page);
        if (kind == Passkey) {
            m_edit->setValidator(new QRegExpValidator(QRegExp("[0-9]{1,6}"), m_edit));
            m_edit->setMaxLength(6);
        } else {
            // BlueZ takes 1 to 16 characters
            m_edit->setMaxLength(16);
        }
        box->addWidget(m_edit);
        connect(m_edit, SIGNAL(textChanged(const QString &)), SLOT(checkInput()));
        m_edit->setFocus();
        checkInput();
    }
    if (kind == AuthorizeService) {
        m_always = new QCheckBox(i18n("&Always allow this device (trust it)"), page);
        box->addWidget(m_always);
    }
    box->addStretch();

    if (buttonsFor(kind) & Yes) {
        if (kind == Confirm) {
            setButtonGuiItem(Yes, KGuiItem(i18n("&Pair"), "button_ok"));
            setButtonGuiItem(No, KGuiItem(i18n("&Reject"), "button_cancel"));
        } else {
            setButtonGuiItem(Yes, KGuiItem(i18n("&Allow"), "button_ok"));
            setButtonGuiItem(No, KGuiItem(i18n("&Deny"), "button_cancel"));
        }
    }
    if (kind == DisplayPinCode || kind == DisplayPasskey)
        setButtonGuiItem(Cancel, KGuiItem(i18n("&Cancel Pairing"), "button_cancel"));

    // the request comes from outside: in front, like a message box
    KWin::setState(winId(), NET::KeepAbove);
}

void AgentDialog::setEntered(const QString &passkey, int entered)
{
    if (!m_value)
        return;
    // the digits typed so far, then the rest
    QString done = passkey.left(entered), rest = passkey.mid(entered);
    m_value->setText(QString::fromLatin1("<u>%1</u>%2").arg(done).arg(rest));
}

void AgentDialog::checkInput()
{
    if (!m_edit)
        return;
    enableButtonOK(!m_edit->text().isEmpty());
}

void AgentDialog::answer(bool accepted)
{
    if (m_answered)
        return;
    m_answered = true;
    emit answered(accepted, m_edit ? m_edit->text() : QString::null,
                  m_always && m_always->isChecked());
}

void AgentDialog::slotYes()
{
    answer(true);
    KDialogBase::slotYes();
}

void AgentDialog::slotNo()
{
    answer(false);
    KDialogBase::slotNo();
}

void AgentDialog::slotOk()
{
    if (m_edit && m_edit->text().isEmpty())
        return;
    answer(true);
    KDialogBase::slotOk();
}

void AgentDialog::slotCancel()
{
    answer(false);
    KDialogBase::slotCancel();
}

void AgentDialog::slotClose()
{
    answer(false);
    KDialogBase::slotClose();
}

// every way out hides the dialog: an answer that was not given is a no
void AgentDialog::hideEvent(QHideEvent *e)
{
    KDialogBase::hideEvent(e);
    if (e->spontaneous())
        return;  // minimised, or on another desktop
    answer(false);
    delayedDestruct();
}

#include "agentdialog.moc"
