/*
 * KSnapshot: screenshots without the dialog (the Print key)
 *
 * Released under the LGPL see file LICENSE for details.
 */

#include "quickshot.h"

#include <qapplication.h>
#include <qclipboard.h>
#include <qcursor.h>
#include <qdatetime.h>
#include <qdesktopwidget.h>
#include <qdir.h>
#include <qfile.h>
#include <qfileinfo.h>
#include <qhbox.h>
#include <qimage.h>
#include <qlabel.h>
#include <qtextstream.h>
#include <qtimer.h>
#include <qvbox.h>

#include <dcopclient.h>
#include <kapplication.h>
#include <kdebug.h>
#include <klocale.h>
#include <kpassivepopup.h>
#include <kpushbutton.h>
#include <krun.h>
#include <ksavefile.h>
#include <kstandarddirs.h>
#include <kurl.h>
#include <netwm.h>

// X11 headers last: their macros clash with Qt's names
#include "regiongrabber.h"
#include "windowgrabber.h"

#include <stdlib.h>

/*
 A folder of the XDG user directories (xdg-user-dirs): $XDG_<name>_DIR,
 else its line in $XDG_CONFIG_HOME/user-dirs.dirs, else ~/<fallback>; one
 set to the home folder (that is how xdg-user-dirs disables it) means
 ~/<fallback> too.
*/
static QString xdgUserDir( const char *name, const QString &fallback )
{
    const QString home = QDir::cleanDirPath( QDir::homeDirPath() );
    QString dir = QFile::decodeName( getenv( QCString( "XDG_" ) + name + "_DIR" ) );

    if ( dir.isEmpty() ) {
        QString configHome = QFile::decodeName( getenv( "XDG_CONFIG_HOME" ) );
        if ( !configHome.startsWith( "/" ) )
            configHome = home + "/.config";
        QFile file( configHome + "/user-dirs.dirs" );
        if ( file.open( IO_ReadOnly ) ) {
            QTextStream stream( &file );
            stream.setEncoding( QTextStream::UnicodeUTF8 );
            const QString key = QString( "XDG_%1_DIR=" ).arg( name );
            while ( !stream.atEnd() ) {
                QString line = stream.readLine().stripWhiteSpace();
                if ( !line.startsWith( key ) )
                    continue;
                // XDG_PICTURES_DIR="$HOME/Pictures" or an absolute path
                QString value = line.mid( key.length() );
                if ( value.length() >= 2 && value.startsWith( "\"" ) && value.endsWith( "\"" ) )
                    value = value.mid( 1, value.length() - 2 );
                value.replace( "\\\"", "\"" );
                if ( value.startsWith( "$HOME" ) )
                    value = home + value.mid( 5 );
                if ( value.startsWith( "/" ) )
                    dir = value;
            }
        }
    }

    if ( !dir.startsWith( "/" ) || QDir::cleanDirPath( dir ) == home )
        dir = home + "/" + fallback;
    return QDir::cleanDirPath( dir );
}

QString QuickShot::screenshotFolder()
{
    return xdgUserDir( "PICTURES", "Pictures" ) + "/" +
        i18n( "folder name", "Screenshots" );
}

QuickShot::QuickShot( Mode m )
    : QObject( 0, "quickshot" ), DCOPObject( "quickshot" ),
      mode( m ), regionGrabber( 0 ), ownsClipboard( false )
{
    QTimer::singleShot( 0, this, SLOT( start() ) );
}

QuickShot::~QuickShot()
{
    delete regionGrabber;
}

bool QuickShot::process( const QCString &fun, const QByteArray &data,
                         QCString &replyType, QByteArray &replyData )
{
    if ( fun == "hidePopup()" ) {
        if ( popup ) {
            popup->hide();
            // off the screen before the caller takes its picture
            QApplication::syncX();
        }
        replyType = "void";
        return true;
    }
    return DCOPObject::process( fun, data, replyType, replyData );
}

/*
 The popups of the screenshots taken just before would be in this one:
 their programs (still serving the clipboard, or waiting for the popup to
 time out) hide them first.
*/
void QuickShot::hideOtherPopups()
{
    DCOPClient *client = kapp->dcopClient();
    if ( client->registerAs( "ksnapshot" ).isEmpty() )
        return;
    const QCStringList apps = client->registeredApplications();
    for ( QCStringList::ConstIterator it = apps.begin(); it != apps.end(); ++it ) {
        if ( ( *it ).find( "ksnapshot-" ) != 0 || *it == client->appId() )
            continue;
        QCString replyType;
        QByteArray replyData;
        client->call( *it, "quickshot", "hidePopup()", QByteArray(),
                      replyType, replyData, false, 1000 );
    }
}

void QuickShot::start()
{
    hideOtherPopups();

    if ( mode == Region ) {
        regionGrabber = new RegionGrabber( 0 );
        connect( regionGrabber, SIGNAL( regionGrabbed( const QPixmap & ) ),
                 SLOT( slotRegionGrabbed( const QPixmap & ) ) );
        return;
    }

    QPixmap pix;
    if ( mode == ActiveWindow )
        pix = WindowGrabber::grabActive();
    if ( pix.isNull() ) {
        // all monitors, also when there is no active window
        const QRect desk = QApplication::desktop()->geometry();
        pix = QPixmap::grabWindow( qt_xrootwin(), desk.x(), desk.y(),
                                   desk.width(), desk.height() );
    }
    deliver( pix );
}

void QuickShot::slotRegionGrabbed( const QPixmap &pix )
{
    // called from the grabber's event handler: delete it later
    regionGrabber->deleteLater();
    regionGrabber = 0;
    if ( pix.isNull() ) {
        // cancelled
        maybeQuit();
        return;
    }
    deliver( pix );
}

void QuickShot::deliver( const QPixmap &pix )
{
    const QImage img = pix.convertToImage();

    QString error;
    fileName = save( img, &error );

    QClipboard *clipboard = QApplication::clipboard();
    clipboard->setImage( img, QClipboard::Clipboard );
    ownsClipboard = clipboard->ownsClipboard();
    connect( clipboard, SIGNAL( dataChanged() ), SLOT( slotClipboardChanged() ) );

    notify( pix, error );
    maybeQuit();
}

QString QuickShot::save( const QImage &img, QString *error )
{
    const QString dir = screenshotFolder();
    if ( !KStandardDirs::exists( dir + "/" ) && !KStandardDirs::makeDir( dir ) ) {
        *error = i18n( "The folder %1 could not be created." ).arg( dir );
        return QString::null;
    }

    const QString base = dir + "/" + i18n( "file name; %1 is the date and time",
        "Screenshot From %1" ).arg(
        QDateTime::currentDateTime().toString( "yyyy-MM-dd hh-mm-ss" ) );
    QString name = base + ".png";
    for ( int n = 2; QFile::exists( name ); ++n )
        name = QString( "%1 (%2).png" ).arg( base ).arg( n );

    KSaveFile file( name );
    if ( file.status() == 0 && img.save( file.file(), "PNG" ) && file.close() )
        return name;

    file.abort();
    kdWarning() << "KSnapshot could not save " << name << endl;
    *error = i18n( "The screenshot could not be saved to %1." ).arg( name );
    return QString::null;
}

void QuickShot::notify( const QPixmap &pix, const QString &error )
{
    KPassivePopup *p = new KPassivePopup( ( QWidget * ) 0, "screenshot popup" );
    QHBox *view = new QHBox( p );
    view->setSpacing( 10 );

    QLabel *thumbnail = new QLabel( view );
    QImage small = pix.convertToImage();
    if ( small.width() > 160 || small.height() > 120 )
        small = small.smoothScale( 160, 120, QImage::ScaleMin );
    thumbnail->setPixmap( QPixmap( small ) );
    thumbnail->setAlignment( Qt::AlignCenter );

    QVBox *column = new QVBox( view );
    column->setSpacing( 6 );
    QLabel *title = new QLabel( column );
    title->setText( i18n( "Screenshot Captured" ) );
    QFont bold = title->font();
    bold.setBold( true );
    title->setFont( bold );
    QLabel *text = new QLabel( column );
    if ( error.isEmpty() )
        text->setText( i18n( "You can paste the image from the clipboard.\n"
                             "It is also saved in the %1 folder." )
                       .arg( QFileInfo( QFileInfo( fileName ).dirPath() ).fileName() ) );
    else
        text->setText( i18n( "You can paste the image from the clipboard.\n%1" ).arg( error ) );

    if ( !fileName.isEmpty() ) {
        QHBox *buttons = new QHBox( column );
        buttons->setSpacing( 6 );
        KPushButton *open = new KPushButton( KGuiItem( i18n( "&Open" ), "fileopen" ), buttons );
        connect( open, SIGNAL( clicked() ), SLOT( slotOpen() ) );
        KPushButton *folder = new KPushButton( KGuiItem( i18n( "Show in &Folder" ), "folder" ), buttons );
        connect( folder, SIGNAL( clicked() ), SLOT( slotShowFolder() ) );
        new QWidget( buttons ); // the buttons on the left
    }

    p->setView( view );
    p->setTimeout( 8000 );
    p->setAutoDelete( true );
    popup = p;
    connect( p, SIGNAL( destroyed() ), SLOT( slotPopupGone() ) );

    // bottom right of the work area of the monitor with the pointer
    p->resize( p->sizeHint() );
    QDesktopWidget *desktop = QApplication::desktop();
    QRect area = desktop->screenGeometry( desktop->screenNumber( QCursor::pos() ) );
    NETRootInfo info( qt_xdisplay(), NET::CurrentDesktop | NET::WorkArea, -1, false );
    info.activate();
    const NETRect work = info.workArea( info.currentDesktop() );
    const QRect workArea( work.pos.x, work.pos.y, work.size.width, work.size.height );
    if ( workArea.intersects( area ) )
        area &= workArea;
    p->show( QPoint( area.right() - p->width() - 10, area.bottom() - p->height() - 10 ) );
}

void QuickShot::slotOpen()
{
    KRun::runURL( KURL::fromPathOrURL( fileName ), "image/png" );
    if ( popup )
        popup->hide();
}

void QuickShot::slotShowFolder()
{
    KRun::runURL( KURL::fromPathOrURL( QFileInfo( fileName ).dirPath( true ) ), "inode/directory" );
    if ( popup )
        popup->hide();
}

void QuickShot::slotClipboardChanged()
{
    if ( !QApplication::clipboard()->ownsClipboard() ) {
        // someone else's now: nothing left to serve
        ownsClipboard = false;
        maybeQuit();
    }
}

void QuickShot::slotPopupGone()
{
    // the guarded pointer may not know yet
    QTimer::singleShot( 0, this, SLOT( maybeQuit() ) );
}

void QuickShot::maybeQuit()
{
    if ( !ownsClipboard && !popup && !regionGrabber )
        qApp->quit();
}

#include "quickshot.moc"
