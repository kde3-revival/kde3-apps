/*
    kwhatsapp: WhatsApp Web as a KDE application (WPE WebKit)

    Copyright (C) 2026 The kde3-revival authors

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.
*/

#include "kwhatsapp.h"

#include <qbitmap.h>
#include <qcheckbox.h>
#include <qlabel.h>
#include <qlayout.h>
#include <qpushbutton.h>
#include <qpainter.h>
#include <qregexp.h>
#include <qstylesheet.h>
#include <qtimer.h>
#include <qtooltip.h>
#include <qvbox.h>
#include <qvgroupbox.h>
#include <qwhatsthis.h>

#include <kaction.h>
#include <kapplication.h>
#include <kconfig.h>
#include <kdebug.h>
#include <kglobal.h>
#include <kglobalsettings.h>
#include <kiconloader.h>
#include <klineedit.h>
#include <klocale.h>
#include <kmenubar.h>
#include <kstatusbar.h>
#include <kmessagebox.h>
#include <knotifyclient.h>
#include <knotifydialog.h>
#include <kparts/browserextension.h>
#include <kparts/componentfactory.h>
#include <kparts/part.h>
#include <kpassivepopup.h>
#include <kpopupmenu.h>
#include <krun.h>
#include <kstdaction.h>

// X11 last: its macros (None, Bool, ...) break Qt's headers
#include <kwin.h>
#include <netwm.h>

// The hovered link, the zoom: a label over the page's bottom left corner,
// as browsers have (no status bar under WhatsApp's own layout).
class KWhatsAppStatusLabel : public QLabel
{
public:
    KWhatsAppStatusLabel( QWidget* parent ) : QLabel( parent, "statusLabel" )
    {
        setFrameStyle( QFrame::Panel | QFrame::Raised );
        setLineWidth( 1 );
        setMargin( 2 );
        setTextFormat( Qt::PlainText );
        setPalette( QToolTip::palette() );
        hide();
        m_timer = new QTimer( this );
        connect( m_timer, SIGNAL( timeout() ), this, SLOT( hide() ) );
    }

    void showText( const QString& text, const QRect& area )
    {
        if ( text.isEmpty() ) {
            hide();
            return;
        }
        setText( text );
        QSize size = sizeHint().boundedTo( QSize( area.width() * 2 / 3, area.height() ) );
        setGeometry( area.left(), area.bottom() - size.height() + 1, size.width(), size.height() );
        show();
        raise();
        m_timer->start( 5000, true );
    }

private:
    QTimer* m_timer;
};

KWhatsApp::KWhatsApp( const KURL& url )
    : KParts::MainWindow( 0, "kwhatsapp-mainwindow" ), m_part( 0 ), m_statusLabel( 0 ), m_tray( 0 ), m_showMenubar( 0 ), m_unread( 0 ),
      m_quitting( false ), m_notify( true ), m_notifyContent( true ), m_closeToTray( true )
{
    readSettings();

    // KWPEPart's app mode: WebKit's website data (the login), cookies and
    // the notification permission in $KDEHOME/share/apps/kwhatsapp/webkit/,
    // apart from Konqueror's and KMail's.
    KConfigGroup general( kapp->config(), "General" );
    QString userAgent = general.readEntry( "UserAgent" );
    if ( userAgent.isEmpty() )
        userAgent = defaultUserAgent();
    QStringList args;
    args << QString::fromLatin1( "mode=app" ) << QString::fromLatin1( "appName=kwhatsapp" )
         << QString::fromLatin1( "sites=" ) + url.host();
    if ( !userAgent.isEmpty() )
        args << QString::fromLatin1( "userAgent=" ) + userAgent;
    int error = 0;
    m_part = KParts::ComponentFactory::createPartInstanceFromLibrary<KParts::ReadOnlyPart>(
        "libkwpepart", this, "webview", this, "kwpepart", args, &error );
    // an older part without the app mode
    if ( m_part && !m_part->property( "appMode" ).toBool() ) {
        delete m_part;
        m_part = 0;
    }
    if ( !m_part ) {
        kdWarning() << "kwhatsapp: KWPEPart (libkwpepart) with its app mode is not available (error " << error << ")" << endl;
        return;
    }
    setCentralWidget( m_part->widget() );
    m_statusLabel = new KWhatsAppStatusLabel( this );

    KStdAction::quit( this, SLOT( slotQuit() ), actionCollection() );
    m_showMenubar = KStdAction::showMenubar( this, SLOT( slotShowMenubar() ), actionCollection() );
    KStdAction::configureNotifications( this, SLOT( slotConfigureNotifications() ), actionCollection() );
    KStdAction::preferences( this, SLOT( slotConfigure() ), actionCollection() );
    setXMLFile( "kwhatsappui.rc" );
    createGUI( m_part );
    // Size, menu bar (kwhatsapprc [MainWindow]); the position: savePosition().
    setAutoSaveSettings( "MainWindow", true );
    m_showMenubar->setChecked( !menuBar()->isHidden() );
    // The part's status bar extension makes one; the hovered link and the
    // zoom go to the label over the page instead.
    statusBar()->hide();

    connect( m_part, SIGNAL( pageTitleChanged( const QString& ) ), SLOT( slotTitleChanged( const QString& ) ) );
    connect( m_part, SIGNAL( notificationShown( int, const QString&, const QString& ) ),
             SLOT( slotNotification( int, const QString&, const QString& ) ) );
    connect( m_part, SIGNAL( permissionRequested( const QString&, const QString&, bool& ) ),
             SLOT( slotPermission( const QString&, const QString&, bool& ) ) );
    connect( m_part, SIGNAL( statusMessage( const QString& ) ), SLOT( showMessage( const QString& ) ) );
    connect( m_part, SIGNAL( setStatusBarText( const QString& ) ), SLOT( slotStatusText( const QString& ) ) );
    KParts::BrowserExtension* ext = KParts::BrowserExtension::childObject( m_part );
    if ( ext )
        connect( ext, SIGNAL( openURLRequest( const KURL&, const KParts::URLArgs& ) ),
                 SLOT( slotOpenURL( const KURL&, const KParts::URLArgs& ) ) );

    m_tray = new KWhatsAppTray( this );
    connect( m_tray, SIGNAL( quitSelected() ), SLOT( slotQuit() ) );
    m_tray->show();

    KConfigGroup window( kapp->config(), "MainWindow" );
    QPoint pos = window.readPointEntry( "Position" );
    QRect desktop = KGlobalSettings::desktopGeometry( pos );
    if ( window.hasKey( "Position" ) && desktop.contains( pos ) )
        move( pos );

    setCaption( i18n( "WhatsApp" ) );
    m_part->openURL( url );
}

KWhatsApp::~KWhatsApp()
{
}

// WhatsApp Web refuses browsers it does not know ("browser not
// supported"); WPE WebKit's own User-Agent says what it is (Safari on
// Linux), which it takes.  The setting [General] UserAgent overrides it.
QString KWhatsApp::defaultUserAgent()
{
    return QString::null;
}

void KWhatsApp::readSettings()
{
    KConfigGroup general( kapp->config(), "General" );
    m_notify = general.readBoolEntry( "Notifications", true );
    m_notifyContent = general.readBoolEntry( "NotificationContent", true );
    m_closeToTray = general.readBoolEntry( "CloseToTray", true );
}

void KWhatsApp::slotTitleChanged( const QString& title )
{
    // "(3) WhatsApp": three chats with unread messages.
    QRegExp count( "^\\((\\d+)\\)" );
    m_unread = count.search( title ) == 0 ? count.cap( 1 ).toInt() : 0;
    setCaption( title.isEmpty() ? i18n( "WhatsApp" ) : title );
    if ( m_tray )
        m_tray->setUnread( m_unread );
}

void KWhatsApp::slotNotification( int, const QString& title, const QString& body )
{
    if ( !m_notify )
        return;
    // KNotify: a passive popup and a sound by default; the user chooses
    // in Settings, Configure Notifications.
    QString text;
    if ( m_notifyContent ) {
        QString escaped = QStyleSheet::escape( body );
        escaped.replace( "\n", "<br>" );
        text = "<qt><b>" + QStyleSheet::escape( title ) + "</b><br>" + escaped + "</qt>";
    } else {
        text = i18n( "New message" );
    }
    KNotifyClient::event( winId(), "message", text );
}

void KWhatsApp::slotPermission( const QString& kind, const QString& origin, bool& allow )
{
    // Notifications: always for the page (the settings decide what is
    // shown).  Microphone, camera, screen sharing (voice messages,
    // calls): WebRTC in WPE WebKit is not tested here yet: no.
    if ( kind == "notifications" ) {
        allow = true;
        return;
    }
    allow = false;
    kdDebug() << "kwhatsapp: denied " << kind << " for " << origin << endl;
    if ( kind == "microphone" || kind == "camera" || kind == "screen" )
        showMessage( i18n( "kwhatsapp cannot use the microphone or the camera yet: voice messages and calls do "
                           "not work. Use WhatsApp on the phone for them." ) );
}

// A link to another site, a new window, mailto:: the KDE browser (as set
// in the Control Center, Konqueror by default) or the mail program.
// Other schemes from a web page are not run.
void KWhatsApp::slotOpenURL( const KURL& url, const KParts::URLArgs& )
{
    QString protocol = url.protocol();
    if ( protocol == "http" || protocol == "https" || protocol == "ftp" )
        kapp->invokeBrowser( url.url() );
    else if ( protocol == "mailto" )
        kapp->invokeMailer( url );
    else
        showMessage( i18n( "kwhatsapp does not open links of this kind: %1" ).arg( url.prettyURL() ) );
}

void KWhatsApp::slotStatusText( const QString& text )
{
    if ( m_statusLabel && m_part )
        m_statusLabel->showText( text, m_part->widget()->geometry() );
}

void KWhatsApp::showMessage( const QString& text )
{
    KPassivePopup::message( i18n( "WhatsApp" ), text, SmallIcon( "kwhatsapp" ), this );
}

void KWhatsApp::showOnCurrentDesktop()
{
    if ( isMinimized() )
        showNormal();
    if ( !isVisible() ) {
        // A window shown again goes to the desktop in its _NET_WM_DESKTOP:
        // set it before mapping (the client's job for a withdrawn window),
        // so KWin puts it on this desktop instead of switching to its old
        // one.  At the position it had (no placement policy).
        NETWinInfo info( qt_xdisplay(), winId(), qt_xrootwin(), NET::WMDesktop, NET::WindowManager );
        info.setDesktop( KWin::currentDesktop() );
        KConfigGroup window( kapp->config(), "MainWindow" );
        if ( window.hasKey( "Position" ) )
            move( window.readPointEntry( "Position" ) );
        show();
    } else if ( !KWin::windowInfo( winId(), NET::WMDesktop ).isOnCurrentDesktop() ) {
        KWin::setOnDesktop( winId(), KWin::currentDesktop() );
    }
    raise();
    KWin::forceActiveWindow( winId() );
}

void KWhatsApp::toggleFromTray()
{
    KWin::WindowInfo info = KWin::windowInfo( winId(), NET::WMDesktop | NET::WMState | NET::XAWMState );
    if ( isVisible() && !isMinimized() && info.isOnCurrentDesktop() && isActiveWindow() )
        hide();
    else
        showOnCurrentDesktop();
}

bool KWhatsApp::queryClose()
{
    // Closing the window keeps WhatsApp running in the tray (its
    // notifications); File, Quit (or the tray's Quit) ends it.
    if ( m_quitting || kapp->sessionSaving() || !m_closeToTray || !m_tray )
        return true;
    hide();
    return false;
}

bool KWhatsApp::queryExit()
{
    if ( isVisible() )
        savePosition();
    return true;
}

void KWhatsApp::hideEvent( QHideEvent* e )
{
    // Hidden in the tray: where it was (shown there again).
    if ( !isMinimized() )
        savePosition();
    KParts::MainWindow::hideEvent( e );
}

void KWhatsApp::savePosition()
{
    KConfigGroup window( kapp->config(), "MainWindow" );
    window.writeEntry( "Position", pos() );
    window.sync();
}

void KWhatsApp::slotQuit()
{
    m_quitting = true;
    if ( isVisible() )
        savePosition();
    saveMainWindowSettings( kapp->config(), "MainWindow" );
    kapp->config()->sync();
    kapp->quit();
}

void KWhatsApp::slotShowMenubar()
{
    if ( m_showMenubar->isChecked() )
        menuBar()->show();
    else
        menuBar()->hide();
}

void KWhatsApp::slotConfigure()
{
    KWhatsAppConfigDialog* dialog = new KWhatsAppConfigDialog( this );
    connect( dialog, SIGNAL( settingsChanged() ), SLOT( readSettings() ) );
    dialog->show();
}

void KWhatsApp::slotConfigureNotifications()
{
    KNotifyDialog::configure( this );
}

// ---------------------------------------------------------------------

KWhatsAppTray::KWhatsAppTray( KWhatsApp* window )
    : KSystemTray( window, "kwhatsapp-tray" ), m_window( window ), m_unread( -1 )
{
    // The menu's Restore as the click: on the current desktop.
    KAction* restore = actionCollection()->action( "minimizeRestore" );
    if ( restore ) {
        restore->disconnect( SIGNAL( activated() ), this );
        connect( restore, SIGNAL( activated() ), SLOT( slotRestore() ) );
    }
    setUnread( 0 );
}

void KWhatsAppTray::slotRestore()
{
    if ( m_window->isVisible() )
        m_window->hide();
    else
        m_window->showOnCurrentDesktop();
}

void KWhatsAppTray::setUnread( int count )
{
    if ( count == m_unread )
        return;
    m_unread = count;
    QToolTip::remove( this );
    QToolTip::add( this, count ? i18n( "WhatsApp: 1 unread chat", "WhatsApp: %n unread chats", count )
                               : i18n( "WhatsApp" ) );
    updateIcon();
}

// The application's icon, the unread count in a red disc on its lower
// right part (as KMail's tray does with new mail).
void KWhatsAppTray::updateIcon()
{
    QPixmap icon = loadIcon( "kwhatsapp" );
    if ( m_unread > 0 && !icon.isNull() ) {
        QString text = m_unread > 99 ? QString::fromLatin1( "99+" ) : QString::number( m_unread );
        QFont font = KGlobalSettings::generalFont();
        font.setBold( true );
        font.setPixelSize( QMAX( 8, icon.height() / 2 ) );
        QFontMetrics metrics( font );
        int h = metrics.height();
        int w = QMAX( h, metrics.width( text ) + h / 2 );
        w = QMIN( w, icon.width() );
        QRect disc( icon.width() - w, icon.height() - h, w, h );
        QPainter p( &icon );
        p.setPen( QColor( 128, 0, 0 ) );
        p.setBrush( QColor( 220, 30, 30 ) );
        p.drawRoundRect( disc, 99 * h / QMAX( w, 1 ), 99 );
        p.setFont( font );
        p.setPen( Qt::white );
        p.drawText( disc, Qt::AlignCenter, text );
        p.end();
        // A mask with the disc (the icon's own mask would cut it).
        if ( icon.mask() ) {
            QBitmap mask = *icon.mask();
            QPainter m( &mask );
            m.setPen( Qt::color1 );
            m.setBrush( Qt::color1 );
            m.drawRoundRect( disc, 99 * h / QMAX( w, 1 ), 99 );
            m.end();
            icon.setMask( mask );
        }
    }
    setPixmap( icon );
}

void KWhatsAppTray::mousePressEvent( QMouseEvent* e )
{
    // Left click: the window on the current desktop at once (KSystemTray
    // switches to the desktop the window was on), or hidden if it is in
    // front.
    if ( e->button() == LeftButton && rect().contains( e->pos() ) ) {
        m_window->toggleFromTray();
        return;
    }
    KSystemTray::mousePressEvent( e );
}

void KWhatsAppTray::resizeEvent( QResizeEvent* e )
{
    KSystemTray::resizeEvent( e );
    updateIcon();
}

// ---------------------------------------------------------------------

KWhatsAppConfigDialog::KWhatsAppConfigDialog( QWidget* parent )
    : KDialogBase( parent, "kwhatsapp-config", false, i18n( "Configure kwhatsapp" ),
                   Ok | Apply | Cancel | Default, Ok, true )
{
    setWFlags( getWFlags() | WDestructiveClose );
    QVBox* page = makeVBoxMainWidget();

    QVGroupBox* notifications = new QVGroupBox( i18n( "Notifications" ), page );
    m_notify = new QCheckBox( i18n( "&Show a notification for new messages" ), notifications );
    m_notifyContent = new QCheckBox( i18n( "Show the &sender and the text (otherwise only \"New message\")" ), notifications );
    QPushButton* configure = new QPushButton( i18n( "Configure &Notifications..." ), notifications );
    QWhatsThis::add( configure, i18n( "The popup, the sound and other ways KDE tells about new messages." ) );
    connect( configure, SIGNAL( clicked() ), parent, SLOT( slotConfigureNotifications() ) );
    connect( m_notify, SIGNAL( toggled( bool ) ), SLOT( updateEnabled() ) );

    QVGroupBox* window = new QVGroupBox( i18n( "Window" ), page );
    m_closeToTray = new QCheckBox( i18n( "&Keep running in the system tray when the window is closed" ), window );

    QVGroupBox* advanced = new QVGroupBox( i18n( "Advanced" ), page );
    QLabel* label = new QLabel( i18n( "&User agent (empty: WebKit's own; used from the next start):" ), advanced );
    m_userAgent = new KLineEdit( advanced );
    label->setBuddy( m_userAgent );
    QWhatsThis::add( m_userAgent, i18n( "What the page is told about the browser. WhatsApp Web refuses browsers "
                                        "it does not know; Safari's user agent is one it knows." ) );

    KConfigGroup general( kapp->config(), "General" );
    m_notify->setChecked( general.readBoolEntry( "Notifications", true ) );
    m_notifyContent->setChecked( general.readBoolEntry( "NotificationContent", true ) );
    m_closeToTray->setChecked( general.readBoolEntry( "CloseToTray", true ) );
    m_userAgent->setText( general.readEntry( "UserAgent" ) );
    updateEnabled();
}

void KWhatsAppConfigDialog::updateEnabled()
{
    m_notifyContent->setEnabled( m_notify->isChecked() );
}

void KWhatsAppConfigDialog::save()
{
    KConfigGroup general( kapp->config(), "General" );
    general.writeEntry( "Notifications", m_notify->isChecked() );
    general.writeEntry( "NotificationContent", m_notifyContent->isChecked() );
    general.writeEntry( "CloseToTray", m_closeToTray->isChecked() );
    general.writeEntry( "UserAgent", m_userAgent->text().stripWhiteSpace() );
    general.sync();
    emit settingsChanged();
}

void KWhatsAppConfigDialog::slotOk()
{
    save();
    accept();
}

void KWhatsAppConfigDialog::slotApply()
{
    save();
}

void KWhatsAppConfigDialog::slotDefault()
{
    m_notify->setChecked( true );
    m_notifyContent->setChecked( true );
    m_closeToTray->setChecked( true );
    m_userAgent->clear();
    updateEnabled();
}

#include "kwhatsapp.moc"
