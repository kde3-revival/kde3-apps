/*
  Copyright (C) 2003 Nadeem Hasan <nhasan@kde.org>

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU General Public
  License as published by the Free Software Foundation; either
  version 2 of the License, or ( at your option ) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Library General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this library; see the file COPYING.  If not, write to
  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
  Boston, MA 02110-1301, USA.
*/

#include "regiongrabber.h"

#include <qapplication.h>
#include <qcursor.h>
#include <qdesktopwidget.h>
#include <qimage.h>
#include <qpainter.h>
#include <qtimer.h>

#include <klocale.h>

#include <X11/Xlib.h>

// how many times (every 50 ms) to try to get the pointer and keyboard:
// the global shortcut that started us may still hold the keyboard (until
// its key is released), an open menu holds both until it is closed (KWin
// runs us also while a menu is open: the picture is taken at once, the
// selection waits), i.e. 20 s
static const int maxGrabTries = 400;

// a key is down (the shortcut's own grab lasts until its release)
static bool keyHeld( Display *dpy )
{
  char keys[ 32 ];
  XQueryKeymap( dpy, keys );
  for ( int i = 0; i < 32; ++i )
    if ( keys[ i ] != 0 )
      return true;
  return false;
}

RegionGrabber::RegionGrabber( int delay )
  : QWidget( 0, 0, WStyle_Customize | WX11BypassWM | WNoAutoErase ),
    mouseDown( false ), done( false ), waiting( false ), grabTries( 0 )
{
  setBackgroundMode( NoBackground );
  setMouseTracking( true );
  hint = selectHint();

  QTimer::singleShot( delay, this, SLOT( initGrabber() ) );
}

RegionGrabber::~RegionGrabber()
{
}

void RegionGrabber::initGrabber()
{
  // all monitors: the root window (QDesktopWidget's geometry is the
  // bounding rectangle of the screens)
  const QRect desk = QApplication::desktop()->geometry();
  pixmap = QPixmap::grabWindow( qt_xrootwin(), desk.x(), desk.y(),
                                desk.width(), desk.height() );

  QImage img = pixmap.convertToImage().convertDepth( 32 );
  for ( int y = 0; y < img.height(); ++y ) {
    QRgb *line = reinterpret_cast<QRgb *>( img.scanLine( y ) );
    for ( int x = 0; x < img.width(); ++x )
      line[x] = qRgb( qRed( line[x] ) / 2, qGreen( line[x] ) / 2, qBlue( line[x] ) / 2 );
  }
  dimmed.convertFromImage( img );

  setGeometry( desk );
  // the hint is centred on the monitor with the pointer
  QDesktopWidget *desktop = QApplication::desktop();
  hintScreen = desktop->screenGeometry( desktop->screenNumber( QCursor::pos() ) );
  hintScreen.moveBy( -desk.x(), -desk.y() );
  setCursor( crossCursor );
  show();
  grabInput();
}

/*
 Without the keyboard Escape could not cancel, without the pointer the
 drag would go to other windows: give up when either cannot be had.
 Qt's grabs keep Qt's own bookkeeping (key events go to the grabber
 without focus); the plain X calls tell whether another client still
 holds a grab (a client may grab again what it has grabbed).
*/
void RegionGrabber::grabInput()
{
  if ( done )
    return;
  Display *dpy = qt_xdisplay();
  grabMouse( crossCursor );
  grabKeyboard();
  const bool pointer = XGrabPointer( dpy, winId(), False,
      ButtonPressMask | ButtonReleaseMask | PointerMotionMask |
      EnterWindowMask | LeaveWindowMask,
      GrabModeAsync, GrabModeAsync, None, crossCursor.handle(),
      CurrentTime ) == GrabSuccess;
  const bool keyboard = XGrabKeyboard( dpy, winId(), False, GrabModeAsync,
      GrabModeAsync, CurrentTime ) == GrabSuccess;
  if ( pointer && keyboard ) {
    setWaiting( false );
    return;
  }

  releaseMouse();
  releaseKeyboard();
  // no key down: another program has the grabs, a menu that is open
  setWaiting( !keyHeld( dpy ) );
  if ( ++grabTries < maxGrabTries )
    QTimer::singleShot( 50, this, SLOT( grabInput() ) );
  else
    finish( QPixmap() );
}

QString RegionGrabber::selectHint()
{
  return i18n( "Drag to select an area of the screen; press Esc to cancel" );
}

/*
 Until the menu open at the Print key is closed, the menu's program has
 the keyboard and the mouse: the picture (with the menu) is shown, the
 hint says what to do.  Esc and clicks go to the menu meanwhile.
*/
void RegionGrabber::setWaiting( bool wait )
{
  if ( wait == waiting )
    return;
  waiting = wait;
  const QRect old = hintRect();
  hint = wait ? i18n( "Close the menu (Esc) to select an area of the screen" ) : selectHint();
  update( old.unite( hintRect() ) );
}

QRect RegionGrabber::selection() const
{
  if ( !mouseDown && startPos == endPos )
    return QRect();
  // from the press to the pointer, the pointer's pixel excluded: dragging
  // 400 pixels selects 400
  int left = QMIN( startPos.x(), endPos.x() ), right = QMAX( startPos.x(), endPos.x() );
  int top = QMIN( startPos.y(), endPos.y() ), bottom = QMAX( startPos.y(), endPos.y() );
  // the pointer cannot go past the last column or row: there it takes them
  if ( right == width() - 1 && right > left )
    right = width();
  if ( bottom == height() - 1 && bottom > top )
    bottom = height();
  return QRect( left, top, right - left, bottom - top ) & rect();
}

QRect RegionGrabber::labelRect( const QRect &sel ) const
{
  const QString text = QString( "%1 %2 %3" ).arg( sel.width() )
      .arg( QChar( 0xd7 ) ).arg( sel.height() );
  const QFontMetrics fm = fontMetrics();
  QRect r( 0, 0, fm.width( text ) + 12, fm.height() + 6 );
  // under the selection, or inside it at the top when there is no room
  r.moveTopLeft( QPoint( sel.left(), sel.bottom() + 6 ) );
  if ( r.bottom() > height() - 1 )
    r.moveTopLeft( QPoint( sel.left() + 6, sel.top() + 6 ) );
  if ( r.right() > width() - 1 )
    r.moveRight( width() - 1 );
  return r;
}

QRect RegionGrabber::hintRect() const
{
  const QFontMetrics fm = fontMetrics();
  QRect r( 0, 0, fm.width( hint ) + 32, fm.height() + 20 );
  r.moveCenter( hintScreen.center() );
  return r;
}

// everything drawn besides the two pictures
QRect RegionGrabber::decorationRect() const
{
  const QRect sel = selection();
  if ( sel.isEmpty() )
    return mouseDown ? QRect() : hintRect();
  return QRect( sel.x() - 1, sel.y() - 1, sel.width() + 2, sel.height() + 2 )
      .unite( labelRect( sel ) );
}

void RegionGrabber::paintEvent( QPaintEvent *e )
{
  const QRect r = e->rect();
  if ( r.isEmpty() || dimmed.isNull() )
    return;

  // drawn off screen and copied in one go: no flicker
  QPixmap buffer( r.size() );
  bitBlt( &buffer, 0, 0, &dimmed, r.x(), r.y(), r.width(), r.height() );

  QPainter p( &buffer );
  p.translate( -r.x(), -r.y() );
  const QRect sel = selection();
  if ( !sel.isEmpty() ) {
    const QRect lit = sel & r;
    if ( !lit.isEmpty() )
      p.drawPixmap( lit.topLeft(), pixmap, lit );
    p.setPen( QPen( colorGroup().highlight(), 1 ) );
    p.setBrush( NoBrush );
    p.drawRect( sel.x() - 1, sel.y() - 1, sel.width() + 2, sel.height() + 2 );

    const QRect label = labelRect( sel );
    p.fillRect( label, Qt::black );
    p.setPen( Qt::white );
    p.drawText( label, AlignCenter, QString( "%1 %2 %3" ).arg( sel.width() )
        .arg( QChar( 0xd7 ) ).arg( sel.height() ) );
  }
  else if ( !mouseDown ) {
    const QRect box = hintRect();
    p.fillRect( box, Qt::black );
    p.setPen( Qt::white );
    p.drawRect( box );
    p.drawText( box, AlignCenter, hint );
  }
  p.end();

  bitBlt( this, r.x(), r.y(), &buffer, 0, 0, r.width(), r.height() );
}

void RegionGrabber::mousePressEvent( QMouseEvent *e )
{
  if ( e->button() == LeftButton ) {
    const QRect old = decorationRect();
    mouseDown = true;
    startPos = endPos = e->pos();
    update( old );
  }
  else if ( e->button() == RightButton )
    finish( QPixmap() );
}

void RegionGrabber::mouseMoveEvent( QMouseEvent *e )
{
  if ( !mouseDown )
    return;
  const QRect old = decorationRect();
  endPos = e->pos();
  update( old.unite( decorationRect() ) );
}

void RegionGrabber::mouseReleaseEvent( QMouseEvent *e )
{
  if ( !mouseDown || e->button() != LeftButton )
    return;
  const QRect old = decorationRect();
  mouseDown = false;
  endPos = e->pos();
  const QRect sel = selection();
  if ( sel.width() < 2 || sel.height() < 2 ) {
    // a click, not a drag: start again
    startPos = endPos;
    update( old.unite( decorationRect() ) );
    return;
  }

  // from the picture taken before the overlay was shown
  QPixmap region( sel.size() );
  bitBlt( &region, 0, 0, &pixmap, sel.x(), sel.y(), sel.width(), sel.height() );
  finish( region );
}

void RegionGrabber::keyPressEvent( QKeyEvent *e )
{
  if ( e->key() == Key_Escape )
    finish( QPixmap() );
  else
    e->ignore();
}

void RegionGrabber::finish( const QPixmap &result )
{
  if ( done )
    return;
  done = true;
  releaseMouse();
  releaseKeyboard();
  hide();
  // gone from the screen before anyone grabs it again
  QApplication::syncX();
  emit regionGrabbed( result );
}

#include "regiongrabber.moc"
