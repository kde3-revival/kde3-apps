/*
    kwhatsapp: WhatsApp Web as a KDE application (WPE WebKit)

    Copyright (C) 2026 The kde3-revival authors

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.
*/

#ifndef KWHATSAPP_H
#define KWHATSAPP_H

#include <kdialogbase.h>
#include <kparts/mainwindow.h>
#include <ksystemtray.h>
#include <kurl.h>

class QCheckBox;
class KLineEdit;
class KToggleAction;
namespace KParts { class ReadOnlyPart; class URLArgs; }
class KWhatsAppTray;
class KWhatsAppStatusLabel;

/**
 * The main window: WhatsApp Web (https://web.whatsapp.com, the official
 * web client) in kde3-wpepart's KWPEPart, app mode: a WebKit session of
 * its own under $KDEHOME/share/apps/kwhatsapp/webkit/ (the login stays),
 * the page's notifications through KNotify, links in the KDE browser,
 * downloads and uploads through KDE's dialogs.  Closing the window hides
 * it in the system tray; the tray icon shows the number of unread chats
 * (from the page title, "(N) WhatsApp").
 */
class KWhatsApp : public KParts::MainWindow
{
    Q_OBJECT

public:
    KWhatsApp( const KURL& url );
    virtual ~KWhatsApp();

    /** False: KWPEPart (with its app mode) could not be loaded. */
    bool isValid() const { return m_part != 0; }

    /** Show the window on the current desktop, raised and active. */
    void showOnCurrentDesktop();
    /** The tray's click: show (see above), or hide if it is in front. */
    void toggleFromTray();

    /** The unread chats the page title shows (0: none). */
    int unreadCount() const { return m_unread; }

    /** The default User-Agent (empty setting): WebKit's, see kwhatsapp.cpp. */
    static QString defaultUserAgent();

public slots:
    void slotQuit();

protected:
    virtual bool queryClose();
    virtual bool queryExit();
    virtual void hideEvent( QHideEvent* e );

private slots:
    void slotTitleChanged( const QString& title );
    void slotNotification( int id, const QString& title, const QString& body );
    void slotPermission( const QString& kind, const QString& origin, bool& allow );
    void slotOpenURL( const KURL& url, const KParts::URLArgs& args );
    void slotStatusText( const QString& text );
    void slotConfigure();
    void slotConfigureNotifications();
    void slotShowMenubar();
    void readSettings();
    void showMessage( const QString& text );

private:
    void savePosition();

    KParts::ReadOnlyPart* m_part;
    KWhatsAppStatusLabel* m_statusLabel;
    KWhatsAppTray* m_tray;
    KToggleAction* m_showMenubar;
    int m_unread;
    bool m_quitting;
    // settings (kwhatsapprc [General])
    bool m_notify;
    bool m_notifyContent;
    bool m_closeToTray;
};

/** The system tray icon: the application's icon, the unread count on it. */
class KWhatsAppTray : public KSystemTray
{
    Q_OBJECT

public:
    KWhatsAppTray( KWhatsApp* window );

    void setUnread( int count );

protected:
    virtual void mousePressEvent( QMouseEvent* e );
    virtual void resizeEvent( QResizeEvent* e );

private slots:
    void slotRestore();

private:
    void updateIcon();

    KWhatsApp* m_window;
    int m_unread;
};

/** Settings, Configure kwhatsapp. */
class KWhatsAppConfigDialog : public KDialogBase
{
    Q_OBJECT

public:
    KWhatsAppConfigDialog( QWidget* parent );

signals:
    void settingsChanged();

protected slots:
    virtual void slotOk();
    virtual void slotApply();
    virtual void slotDefault();

private slots:
    void updateEnabled();

private:
    void save();

    QCheckBox* m_notify;
    QCheckBox* m_notifyContent;
    QCheckBox* m_closeToTray;
    KLineEdit* m_userAgent;
};

#endif
