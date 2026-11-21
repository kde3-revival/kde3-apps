/*
 * KSnapshot: screenshots without the dialog (the Print key)
 *
 * Released under the LGPL see file LICENSE for details.
 */

#ifndef QUICKSHOT_H
#define QUICKSHOT_H

#include <dcopobject.h>
#include <qguardedptr.h>
#include <qobject.h>
#include <qpixmap.h>

class KPassivePopup;
class RegionGrabber;

/*
 Takes one screenshot (a region the user selects, all monitors, or the
 active window), saves it as Screenshots/Screenshot From <date time>.png in
 the XDG pictures folder, puts it on the clipboard and tells the user with
 a passive popup (Open, Show in Folder).  The program then lives on, out of
 sight, as long as it owns the clipboard: X has no clipboard of its own.
*/
class QuickShot : public QObject, public DCOPObject
{
    Q_OBJECT

public:
    enum Mode { Region, FullScreen, ActiveWindow };

    QuickShot( Mode mode );
    ~QuickShot();

    static QString screenshotFolder();

    // DCOP "hidePopup()": another screenshot is about to be taken
    bool process( const QCString &fun, const QByteArray &data,
                  QCString &replyType, QByteArray &replyData );

private slots:
    void start();
    void grab();
    void slotRegionGrabbed( const QPixmap &pix );
    void slotClipboardChanged();
    void slotOpen();
    void slotShowFolder();
    void slotPopupGone();
    void maybeQuit();

private:
    bool hideOtherPopups();
    void deliver( const QPixmap &pix );
    QString save( const QImage &img, QString *error );
    void notify( const QPixmap &pix, const QString &error );

    Mode mode;
    RegionGrabber *regionGrabber;
    bool ownsClipboard;
    QString fileName;
    QGuardedPtr<KPassivePopup> popup;
};

#endif // QUICKSHOT_H
