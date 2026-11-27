/*
    kwhatsapp: WhatsApp Web as a KDE application (WPE WebKit)

    Copyright (C) 2026 The kde3-revival authors

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.
*/

#include "kwhatsapp.h"

#include <stdio.h>
#include <stdlib.h>
#include <sys/utsname.h>

#include <qbitmap.h>
#include <qcheckbox.h>
#include <qdatastream.h>
#include <qdesktopwidget.h>
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

#include <kaboutdata.h>
#include <kaction.h>
#include <kapplication.h>
#include <kballoon.h>
#include <kconfig.h>
#include <dcopclient.h>
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
#include <X11/Xlib.h>

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
    : KParts::MainWindow( 0, "kwhatsapp-mainwindow" ), m_shown( 0 ), m_part( 0 ), m_statusLabel( 0 ), m_tray( 0 ), m_showMenubar( 0 ), m_unread( 0 ),
      m_quitting( false ), m_notify( true ), m_notifyContent( true ), m_closeToTray( true ), m_balloonTimeout( 30 ),
      m_dockTimer( 0 ), m_dockChecks( 0 )
{
    readSettings();

    // KWPEPart's app mode: WebKit's website data (the login), cookies and
    // the notification permission in $KDEHOME/share/apps/kwhatsapp/webkit/,
    // apart from Konqueror's and KMail's.
    KConfigGroup general( kapp->config(), "General" );
    QString userAgent = general.readEntry( "UserAgent" );
    if ( userAgent.isEmpty() )
        userAgent = defaultUserAgent();
    m_site = url.host().lower();
    QStringList args;
    args << QString::fromLatin1( "mode=app" ) << QString::fromLatin1( "appName=kwhatsapp" )
         << QString::fromLatin1( "sites=" ) + url.host();
    if ( !userAgent.isEmpty() )
        args << QString::fromLatin1( "userAgent=" ) + userAgent;
    // WhatsApp's notifications are what KWhatsApp is for: allowed from
    // the start (unless denied before), so WhatsApp does not start with
    // its own message notifications switched off waiting for the user.
    QString origin = url.protocol() + "://" + url.host();
    if ( url.port() )
        origin += ":" + QString::number( url.port() );
    args << QString::fromLatin1( "notifications=" ) + origin;
    // The senders' pictures: WhatsApp's own CDN (its CSP's img-src lists
    // https://*.whatsapp.net; profile pictures come from pps.whatsapp.net,
    // which allows CORS).  [General] PictureOrigins replaces it.
    QString pictures = general.readEntry( "PictureOrigins", "https://*.whatsapp.net" );
    args << QString::fromLatin1( "pictureOrigins=" ) + pictures;
    if ( debug() )
        args << QString::fromLatin1( "debug=true" );
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
    connect( m_part, SIGNAL( notificationShown( int, const QString&, const QString&, const QImage& ) ),
             SLOT( slotNotification( int, const QString&, const QString&, const QImage& ) ) );
    connect( m_part, SIGNAL( notificationClosed( int ) ), SLOT( slotNotificationClosed( int ) ) );
    connect( this, SIGNAL( activateNotification( int ) ), m_part, SLOT( activateNotification( int ) ) );
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

// The balloon is a window of its own (at the tray icon): it goes with
// this one (first).
KWhatsApp::~KWhatsApp()
{
    m_messages.clear();
    m_shown = 0;
    delete static_cast<KBalloon*>( m_balloon );
    if ( debug() )
        fprintf( stderr, "kwhatsapp: main window destroyed\n" );
}

// WhatsApp Web refuses browsers it does not know ("browser not
// supported").  Safari's user agent on Linux, KWhatsApp named after it (WhatsApp lists
// the linked device as "Safari (Linux)").  WebKit's own would be replaced
// for web.whatsapp.com by a Mac's (its site-specific quirks), and
// WhatsApp then offers its Mac application.  The setting [General]
// UserAgent overrides it.
QString KWhatsApp::defaultUserAgent()
{
    struct utsname name;
    QString machine = uname( &name ) == 0 ? QString::fromLatin1( name.machine ) : QString::fromLatin1( "x86_64" );
    const KAboutData* about = KGlobal::instance() ? KGlobal::instance()->aboutData() : 0;
    QString version = about ? about->version() : QString::fromLatin1( "0.1" );
    return QString::fromLatin1( "Mozilla/5.0 (X11; Linux %1) AppleWebKit/605.1.15 (KHTML, like Gecko) "
                                "Version/18.5 Safari/605.1.15 KWhatsApp/%2" ).arg( machine ).arg( version );
}

void KWhatsApp::readSettings()
{
    KConfigGroup general( kapp->config(), "General" );
    m_notify = general.readBoolEntry( "Notifications", true );
    m_notifyContent = general.readBoolEntry( "NotificationContent", true );
    m_closeToTray = general.readBoolEntry( "CloseToTray", true );
    m_balloonTimeout = QMAX( 0, general.readNumEntry( "BalloonTimeout", 30 ) );
    if ( !m_notify )
        dropBalloons();
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

// The code point at @p i of @p s (a surrogate pair's whole one), and
// the one before @p i.
static uint codePointAt( const QString& s, uint i )
{
    ushort c = s[i].unicode();
    if ( c >= 0xd800 && c <= 0xdbff && i + 1 < s.length() ) {
        ushort d = s[i + 1].unicode();
        if ( d >= 0xdc00 && d <= 0xdfff )
            return 0x10000 + ( ( c - 0xd800 ) << 10 ) + ( d - 0xdc00 );
    }
    return c;
}

static uint codePointBefore( const QString& s, uint i )
{
    ushort c = s[i - 1].unicode();
    if ( c >= 0xdc00 && c <= 0xdfff && i >= 2 ) {
        ushort h = s[i - 2].unicode();
        if ( h >= 0xd800 && h <= 0xdbff )
            return 0x10000 + ( ( h - 0xd800 ) << 10 ) + ( c - 0xdc00 );
    }
    return c;
}

static bool isRegionalIndicator( uint c )
{
    return c >= 0x1f1e6 && c <= 0x1f1ff;
}

// Indic_Conjunct_Break: the viramas that join consonants (Linker) and
// the consonants of those scripts (Devanagari, Bengali, Gujarati, Oriya,
// Telugu, Malayalam).
static bool isConjunctLinker( uint c )
{
    return c == 0x094d || c == 0x09cd || c == 0x0acd || c == 0x0b4d || c == 0x0c4d || c == 0x0d4d;
}

static bool isConjunctConsonant( uint c )
{
    if ( !( ( c >= 0x0900 && c <= 0x0b7f ) || ( c >= 0x0c00 && c <= 0x0c7f ) || ( c >= 0x0d00 && c <= 0x0d7f ) ) )
        return false;
    return QChar( ushort( c ) ).category() == QChar::Letter_Other;
}

// Unicode's grapheme cluster boundaries (UAX #29), the ones a cut of
// a sender's name or a message meets: none inside a surrogate pair,
// before an extending character (combining marks, ZWJ, variation
// selectors, emoji modifiers, tag characters, Hangul vowel and final
// jamo), after a ZWJ (emoji sequences), inside a pair of regional
// indicators (a flag) or inside an Indic conjunct (a consonant after a
// virama that joins it, GB9c).
static bool isGraphemeBoundary( const QString& s, uint i )
{
    if ( i == 0 || i >= s.length() )
        return true;
    ushort u = s[i].unicode();
    if ( u >= 0xdc00 && u <= 0xdfff )
        return false;
    uint c = codePointAt( s, i );
    if ( s[i].isMark() || c == 0x200d || ( c >= 0xfe00 && c <= 0xfe0f ) || ( c >= 0xe0100 && c <= 0xe01ef )
         || ( c >= 0x1f3fb && c <= 0x1f3ff ) || ( c >= 0xe0020 && c <= 0xe007f ) || ( c >= 0x1160 && c <= 0x11ff ) )
        return false;
    if ( codePointBefore( s, i ) == 0x200d )
        return false;
    if ( isConjunctConsonant( c ) ) {
        // consonant [Extend | Linker]* Linker [Extend | Linker]* | consonant
        bool linker = false;
        uint j = i;
        while ( j > 0 && ( s[j - 1].isMark() || s[j - 1].unicode() == 0x200d ) ) {
            linker = linker || isConjunctLinker( s[j - 1].unicode() );
            --j;
        }
        if ( linker && j > 0 && isConjunctConsonant( s[j - 1].unicode() ) )
            return false;
    }
    if ( isRegionalIndicator( c ) ) {
        uint before = 0;
        for ( uint j = i; j > 0 && isRegionalIndicator( codePointBefore( s, j ) ); j -= 2 )
            ++before;
        if ( before % 2 )
            return false;
    }
    return true;
}

// The first @p length characters (UTF-16 units) of @p text at most, cut
// at a grapheme boundary: whole characters as they are shown (a flag,
// an emoji sequence, a letter with its marks), never half of one.
static QString safeLeft( const QString& text, uint length )
{
    if ( text.length() <= length )
        return text;
    uint n = length;
    while ( n > 0 && !isGraphemeBoundary( text, n ) )
        --n;
    return text.left( n );
}

// One line, the first @p length characters.
static QString squash( const QString& text, uint length )
{
    QString line = text.simplifyWhiteSpace();
    if ( line.length() > length )
        line = safeLeft( line, length ) + QString::fromLatin1( " ..." );
    return line;
}

// The page's text in the balloon's sentence: its own direction controls
// out (they would reach the punctuation and the text around it), then
// embedded in its direction (its first strong character) and closed with
// the mark of ours, so it reorders only itself.
static QString isolated( const QString& text )
{
    QString clean;
    for ( uint i = 0; i < text.length(); ++i ) {
        ushort c = text[i].unicode();
        if ( c == 0x200e || c == 0x200f || c == 0x061c || ( c >= 0x202a && c <= 0x202e ) || ( c >= 0x2066 && c <= 0x2069 ) )
            continue;
        clean += text[i];
    }
    bool rightToLeft = false;
    for ( uint i = 0; i < clean.length(); ++i ) {
        QChar::Direction direction = clean[i].direction();
        if ( direction == QChar::DirL )
            break;
        if ( direction == QChar::DirR || direction == QChar::DirAL ) {
            rightToLeft = true;
            break;
        }
    }
    return QChar( rightToLeft ? 0x202b : 0x202a ) + clean + QChar( 0x202c ) + QChar( QApplication::reverseLayout() ? 0x200f : 0x200e );
}

// KNotify's event "message" without its passive popup: the rest of what
// the user chose in Configure Notifications (sound, log file, command,
// ...) by the KNotify daemon, as KNotifyClient::event() sends it but with
// the presentation given.
static void notifyWithout( int present, const QString& text, WId window )
{
    if ( present <= 0 )
        return;
    DCOPClient* client = kapp->dcopClient();
    if ( !client->isAttached() )
        client->attach();
    if ( !client->isAttached() || !KNotifyClient::startDaemon() )
        return;
    QByteArray data;
    QDataStream ds( data, IO_WriteOnly );
    ds << QString::fromLatin1( "message" ) << QString::fromLatin1( KNotifyClient::instance()->instanceName() ) << text
       << QString::null << QString::null << present << int( KNotifyClient::Default ) << int( window )
       << kMax( 1, kapp->random() );
    client->send( "knotify", "Notify", "notify(QString,QString,QString,QString,QString,int,int,int,int)", data );
}

void KWhatsApp::slotNotification( int id, const QString& title, const QString& body, const QImage& icon )
{
    if ( debug() )
        fprintf( stderr, "kwhatsapp: notification %d%s: %s\n", id, icon.isNull() ? "" : " with a picture", title.utf8().data() );
    if ( !m_notify )
        return;
    // KNotify: the user chooses in Settings, Configure Notifications (a
    // passive popup and a sound by default).  Its passive popup is our
    // balloon at the tray icon (KBalloon); the rest is KNotify's.
    QString text;
    if ( m_notifyContent ) {
        QString escaped = QStyleSheet::escape( body );
        escaped.replace( "\n", "<br>" );
        text = "<qt><b>" + QStyleSheet::escape( title ) + "</b><br>" + escaped + "</qt>";
    } else {
        text = i18n( "New message" );
    }
    int present = KNotifyClient::getPresentation( "message" );
    if ( present == -1 )
        present = KNotifyClient::getDefaultPresentation( "message" );
    if ( present == -1 )
        present = KNotifyClient::Sound | KNotifyClient::PassivePopup;
    if ( !( present & KNotifyClient::PassivePopup ) ) {
        KNotifyClient::event( winId(), "message", text );
        return;
    }
    // (KNotify shows no message box with its passive popup either.)
    notifyWithout( present & ~( KNotifyClient::PassivePopup | KNotifyClient::Messagebox ), text, winId() );
    // Read in the window already: no balloon.
    if ( isInFront() )
        return;
    Message message;
    message.id = id;
    if ( m_notifyContent ) {
        // What the balloon can show of it (see showBalloon()).
        message.title = safeLeft( title, 200 );
        message.body = safeLeft( body, 200 );
        if ( !icon.isNull() && icon.width() <= 96 && icon.height() <= 96 )
            message.icon = icon.width() > 48 || icon.height() > 48 ? icon.smoothScale( 48, 48, QImage::ScaleMin ) : icon;
    }
    m_messages.append( message );
    // At most 20 waiting, and their bytes bounded: the oldest one not in
    // the balloon goes.
    uint bytes = 0;
    for ( QValueList<Message>::ConstIterator it = m_messages.begin(); it != m_messages.end(); ++it )
        bytes += messageBytes( *it );
    while ( m_messages.count() > maxMessages || ( bytes > maxMessageBytes && m_messages.count() > 1 ) ) {
        QValueList<Message>::Iterator oldest = m_messages.begin();
        if ( m_balloon )
            ++oldest;
        bytes -= messageBytes( *oldest );
        if ( debug() )
            fprintf( stderr, "kwhatsapp: balloon queue full: %d dropped\n", ( *oldest ).id );
        m_messages.remove( oldest );
    }
    if ( !m_balloon )
        showBalloon();
}

// KWHATSAPP_DEBUG set: what the balloon shows, on stderr (the tests),
// and (the part's debug=true) why a notification's picture was not
// taken, with only the scheme and host of its URL.
bool KWhatsApp::debug()
{
    static int set = -1;
    if ( set < 0 )
        set = getenv( "KWHATSAPP_DEBUG" ) ? 1 : 0;
    return set;
}

uint KWhatsApp::messageBytes( const Message& message )
{
    return ( message.title.length() + message.body.length() ) * sizeof( QChar ) + ( message.icon.isNull() ? 0 : message.icon.numBytes() );
}

// The window is shown on this desktop and has the focus.
bool KWhatsApp::isInFront() const
{
    if ( !isVisible() || isMinimized() || !isActiveWindow() )
        return false;
    return KWin::windowInfo( winId(), NET::WMDesktop ).isOnCurrentDesktop();
}

// The first message waiting in the balloon: a KBalloon at the tray icon
// (its tail at the icon's edge towards the middle of its monitor), else
// in the corner of the screen.  Title and text are plain text (nothing
// in them is markup); the sender and the text are cut short and kept in
// their own direction (isolated()).
void KWhatsApp::showBalloon()
{
    if ( m_balloon || m_messages.isEmpty() )
        return;
    const Message& message = m_messages.first();
    QString title, text;
    QPixmap picture;
    if ( message.title.isEmpty() && message.body.isEmpty() ) {
        // NotificationContent off (or the page said nothing)
        title = i18n( "New message" );
    } else {
        title = i18n( "New message from %1" ).arg( isolated( squash( message.title, 40 ) ) );
        text = isolated( squash( message.body, 30 ) );
        if ( !message.icon.isNull() )
            picture.convertFromImage( message.icon );   // at most 48 pixels (slotNotification())
    }
    if ( picture.isNull() )
        picture = DesktopIcon( "kwhatsapp", 48 );
    if ( debug() ) {
        if ( !message.icon.isNull() )
            fprintf( stderr, "kwhatsapp: balloon picture %d\n", message.id );
        fprintf( stderr, "kwhatsapp: balloon %d: %s | %s\n", message.id, title.utf8().data(), text.utf8().data() );
    }
    KBalloon* balloon = new KBalloon( this, "balloon" );
    m_balloon = balloon;
    m_shown = balloon;
    // (for tools that look for it: no frame shows it)
    balloon->setCaption( i18n( "New message" ) );
    balloon->setTitle( title );
    balloon->setText( text );
    balloon->setPixmap( picture );
    balloon->insertAction( i18n( "to view", "View" ), ViewAction );
    balloon->insertAction( i18n( "Ignore" ), IgnoreAction );
    balloon->setTimeout( m_balloonTimeout * 1000 );   // 0: none
    connect( balloon, SIGNAL( clicked() ), SLOT( slotBalloonView() ) );
    connect( balloon, SIGNAL( actionActivated( int ) ), SLOT( slotBalloonAction( int ) ) );
    connect( balloon, SIGNAL( timedOut() ), SLOT( slotBalloonIgnore() ) );
    connect( balloon, SIGNAL( destroyed() ), SLOT( slotBalloonDestroyed() ) );
    if ( m_tray && m_tray->isDocked() )
        balloon->setAnchor( m_tray );
    balloon->show();
    // (its window is made when it is first shown)
    balloon->setCaption( i18n( "New message" ) );
}

// The balloon goes: by itself (it was clicked, an action chosen, its
// time out: it closes after its signal), or hidden here.  It deletes
// itself.
void KWhatsApp::closeBalloon( bool byItself )
{
    KBalloon* balloon = m_balloon;
    m_balloon = 0;
    m_shown = 0;
    if ( !balloon )
        return;
    balloon->disconnect( this );
    connect( balloon, SIGNAL( destroyed() ), SLOT( slotBalloonDestroyed() ) );
    if ( !byItself )
        balloon->hide();
}

void KWhatsApp::slotBalloonDestroyed()
{
    if ( debug() )
        fprintf( stderr, "kwhatsapp: balloon destroyed\n" );
    if ( !m_shown || sender() != m_shown )
        return;
    // Dismissed (a right or middle click): as Ignore, the next message.
    m_shown = 0;
    m_balloon = 0;
    if ( !m_messages.isEmpty() ) {
        if ( debug() )
            fprintf( stderr, "kwhatsapp: balloon dismissed: %d\n", m_messages.first().id );
        m_messages.remove( m_messages.begin() );
    }
    QTimer::singleShot( 0, this, SLOT( showBalloon() ) );
}

void KWhatsApp::slotBalloonAction( int action )
{
    if ( action == ViewAction )
        slotBalloonView();
    else
        slotBalloonIgnore();
}

// The window is in front: every message waiting is read there.
void KWhatsApp::dropBalloons()
{
    m_messages.clear();
    closeBalloon();
}

// View (or a click on the balloon): the window on this desktop, the chat
// opened by the page (its notification's click).  (The balloon's signal:
// it closes by itself.)
void KWhatsApp::slotBalloonView()
{
    closeBalloon( true );
    if ( m_messages.isEmpty() )
        return;
    int id = m_messages.first().id;
    m_messages.remove( m_messages.begin() );
    showOnCurrentDesktop();
    emit activateNotification( id );
    QTimer::singleShot( 0, this, SLOT( showBalloon() ) );
}

// Ignore, the timeout: the next message.
void KWhatsApp::slotBalloonIgnore()
{
    closeBalloon( true );
    if ( !m_messages.isEmpty() )
        m_messages.remove( m_messages.begin() );
    QTimer::singleShot( 0, this, SLOT( showBalloon() ) );
}

// The page closed its notification (read on the phone, ...).
void KWhatsApp::slotNotificationClosed( int id )
{
    for ( QValueList<Message>::Iterator it = m_messages.begin(); it != m_messages.end(); ++it ) {
        if ( ( *it ).id != id )
            continue;
        bool shown = it == m_messages.begin() && m_balloon;
        m_messages.remove( it );
        if ( shown ) {
            closeBalloon();
            QTimer::singleShot( 0, this, SLOT( showBalloon() ) );
        }
        return;
    }
}

void KWhatsApp::windowActivationChange( bool oldActive )
{
    KParts::MainWindow::windowActivationChange( oldActive );
    if ( isInFront() )
        dropBalloons();
}

void KWhatsApp::slotPermission( const QString& kind, const QString& origin, bool& allow )
{
    // Notifications: always for the page (the settings decide what is
    // shown).  Microphone, camera, screen sharing (voice messages,
    // calls): WebRTC in WPE WebKit is not tested here yet: no.
    // Notifications: for WhatsApp's own page only (the part shows no
    // other site, this is the second check).
    KURL from( origin );
    if ( kind == "notifications" && ( from.protocol() == "https" || from.protocol() == "http" )
         && from.host().lower() == m_site ) {
        allow = true;
        return;
    }
    allow = false;
    kdDebug() << "kwhatsapp: denied " << kind << " for " << origin << endl;
    if ( kind == "microphone" || kind == "camera" || kind == "screen" )
        showMessage( i18n( "KWhatsApp cannot use the microphone or the camera yet: voice messages and calls do "
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
        showMessage( i18n( "KWhatsApp does not open links of this kind: %1" ).arg( url.prettyURL() ) );
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
    // Minimized: shown again as it was (showNormal() would also take back
    // a maximization, e.g. the window manager's vertical one).
    if ( isMinimized() )
        setWindowState( windowState() & ~WindowMinimized );
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

void KWhatsApp::startHidden()
{
    if ( !m_dockTimer ) {
        m_dockTimer = new QTimer( this, "kwhatsapp-docktimer" );
        connect( m_dockTimer, SIGNAL( timeout() ), SLOT( slotCheckDocked() ) );
    }
    m_dockChecks = 0;
    m_dockTimer->start( 500 );
}

void KWhatsApp::slotCheckDocked()
{
    // docked, or the window shown meanwhile: nothing to do
    if ( isVisible() || !m_tray || m_tray->isDocked() ) {
        m_dockTimer->stop();
        return;
    }
    if ( ++m_dockChecks < 30 )
        return;
    m_dockTimer->stop();
    showOnCurrentDesktop();
}

bool KWhatsApp::startAtLogin()
{
    KConfigGroup general( kapp->config(), "General" );
    return general.readBoolEntry( "Autostart", false );
}

void KWhatsApp::setStartAtLogin( bool start )
{
    KConfigGroup general( kapp->config(), "General" );
    general.writeEntry( "Autostart", start );
    general.sync();
}

void KWhatsApp::initStartAtLogin()
{
    KConfigGroup general( kapp->config(), "General" );
    if ( !general.hasKey( "Autostart" ) )
        setStartAtLogin( true );
}

// ---------------------------------------------------------------------

KWhatsAppTray::KWhatsAppTray( KWhatsApp* window )
    : KSystemTray( window, "kwhatsapp-tray" ), m_window( window ), m_unread( -1 )
{
    // Between the title and KSystemTray's own Quit, as KOrganizer's
    // reminder daemon has it; checked as the setting is when shown.
    m_autostartId = contextMenu()->insertItem( i18n( "Start KWhatsApp at Login" ), this,
                                               SLOT( slotToggleAutostart() ) );

    // The menu's Restore as the click: on the current desktop.
    KAction* restore = actionCollection()->action( "minimizeRestore" );
    if ( restore ) {
        restore->disconnect( SIGNAL( activated() ), this );
        connect( restore, SIGNAL( activated() ), SLOT( slotRestore() ) );
    }
    setUnread( 0 );
}

// KSystemTray puts a separator between the title and its own Minimize
// and Quit, for the entries an application adds in between; KWhatsApp
// adds none, so the separator would follow the title directly.  Minimize
// (Restore) is left out too: a left click on the icon does it.
void KWhatsAppTray::contextMenuAboutToShow( KPopupMenu* menu )
{
    menu->setItemChecked( m_autostartId, KWhatsApp::startAtLogin() );
    KAction* restore = actionCollection()->action( "minimizeRestore" );
    if ( restore && restore->isPlugged( menu ) )
        restore->unplug( menu );
    if ( menu->count() > 1 ) {
        int id = menu->idAt( 1 );
        QMenuItem* item = menu->findItem( id );
        if ( item && item->isSeparator() )
            menu->removeItem( id );
    }
}

// In a system tray: there is one (the owner of the freedesktop.org
// system tray selection: kicker's) and it took our window (reparented);
// without one the window is its own (or the window manager's frame's).
bool KWhatsAppTray::isDocked() const
{
    if ( !isVisible() )
        return false;
    QCString selection;
    selection.sprintf( "_NET_SYSTEM_TRAY_S%d", qt_xscreen() );
    if ( XGetSelectionOwner( qt_xdisplay(), XInternAtom( qt_xdisplay(), selection.data(), False ) ) == 0 )
        return false;
    Window root = 0, parent = 0;
    Window* children = 0;
    unsigned int count = 0;
    if ( !XQueryTree( qt_xdisplay(), winId(), &root, &parent, &children, &count ) )
        return false;
    if ( children )
        XFree( children );
    return parent != 0 && parent != root;
}

void KWhatsAppTray::slotRestore()
{
    if ( m_window->isVisible() )
        m_window->hide();
    else
        m_window->showOnCurrentDesktop();
}

void KWhatsAppTray::slotToggleAutostart()
{
    KWhatsApp::setStartAtLogin( !KWhatsApp::startAtLogin() );
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
    : KDialogBase( parent, "kwhatsapp-config", false, i18n( "Configure KWhatsApp" ),
                   Ok | Apply | Cancel | Default, Ok, true )
{
    setWFlags( getWFlags() | WDestructiveClose );
    QVBox* page = makeVBoxMainWidget();

    QVGroupBox* notifications = new QVGroupBox( i18n( "Notifications" ), page );
    m_notify = new QCheckBox( i18n( "&Show a notification for new messages" ), notifications );
    m_notifyContent = new QCheckBox( i18n( "Show the &sender, the text and the picture (otherwise only \"New message\")" ), notifications );
    QPushButton* configure = new QPushButton( i18n( "Configure &Notifications..." ), notifications );
    QWhatsThis::add( configure, i18n( "The popup, the sound and other ways KDE tells about new messages." ) );
    connect( configure, SIGNAL( clicked() ), parent, SLOT( slotConfigureNotifications() ) );
    connect( m_notify, SIGNAL( toggled( bool ) ), SLOT( updateEnabled() ) );

    QVGroupBox* window = new QVGroupBox( i18n( "Window" ), page );
    m_closeToTray = new QCheckBox( i18n( "&Keep running in the system tray when the window is closed" ), window );
    m_autostart = new QCheckBox( i18n( "Start KWhatsApp at &login (in the system tray)" ), window );

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
    m_autostartLoaded = KWhatsApp::startAtLogin();
    m_autostart->setChecked( m_autostartLoaded );
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
    // only a change made here: the tray icon's menu may have changed the
    // setting since the dialog read it
    if ( m_autostart->isChecked() != m_autostartLoaded ) {
        m_autostartLoaded = m_autostart->isChecked();
        KWhatsApp::setStartAtLogin( m_autostartLoaded );
    }
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
    m_autostart->setChecked( true );
    m_userAgent->clear();
    updateEnabled();
}

#include "kwhatsapp.moc"
