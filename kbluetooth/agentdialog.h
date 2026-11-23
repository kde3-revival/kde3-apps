/*
    This file is part of kbluetooth, the Bluetooth applet of the KDE3 revival

    The dialogs of the pairing agent: they never block, the agent answers
    BlueZ when the user closes them.

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.
*/

#ifndef KBLUETOOTH_AGENTDIALOG_H
#define KBLUETOOTH_AGENTDIALOG_H

#include <kdialogbase.h>

class QCheckBox;
class QLabel;
class KLineEdit;

class AgentDialog : public KDialogBase
{
    Q_OBJECT
public:
    enum Kind {
        Confirm,        // RequestConfirmation: compare the passkey
        Authorize,      // RequestAuthorization: pairing without a passkey
        AuthorizeService,
        PinCode,        // RequestPinCode: the user types a PIN
        Passkey,        // RequestPasskey: the user types six digits
        DisplayPinCode, // the user types the shown PIN on the device
        DisplayPasskey
    };

    AgentDialog(Kind kind, const QString &devicePath, const QString &deviceName,
                const QString &value);

    Kind kind() const { return m_kind; }
    QString devicePath() const { return m_devicePath; }
    // DisplayPasskey: the number of digits typed on the device so far
    void setEntered(const QString &passkey, int entered);

signals:
    // accepted: the user said yes, text: the PIN or the passkey,
    // always: AuthorizeService was allowed for good (trust the device)
    void answered(bool accepted, const QString &text, bool always);

protected slots:
    virtual void slotYes();
    virtual void slotNo();
    virtual void slotOk();
    virtual void slotCancel();
    virtual void slotClose();

protected:
    virtual void hideEvent(QHideEvent *e);

private slots:
    void checkInput();

private:
    void answer(bool accepted);

    Kind m_kind;
    QString m_devicePath;
    QLabel *m_value;
    KLineEdit *m_edit;
    QCheckBox *m_always;
    bool m_answered;
};

#endif
