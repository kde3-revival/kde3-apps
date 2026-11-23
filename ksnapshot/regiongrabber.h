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

#ifndef REGIONGRABBER_H
#define REGIONGRABBER_H

#include <qpixmap.h>
#include <qwidget.h>

/*
 Lets the user drag out a rectangle over a frozen picture of the whole
 desktop (all monitors), dimmed outside the selection; the region is cut
 from that picture, so neither the overlay nor anything drawn on it is in
 the result.  regionGrabbed() is emitted once: with the region, or with a
 null pixmap when cancelled (Escape, right button, or no input grab within
 20 s: a menu open meanwhile holds it until it is closed).
*/
class RegionGrabber : public QWidget
{
  Q_OBJECT

  public:
    // delay: ms to wait before grabbing the screen (time for the caller's
    // own windows to disappear)
    RegionGrabber( int delay = 200 );
    ~RegionGrabber();

  protected slots:
    void initGrabber();
    void grabInput();

  signals:
    void regionGrabbed( const QPixmap & );

  protected:
    void paintEvent( QPaintEvent *e );
    void mousePressEvent( QMouseEvent *e );
    void mouseReleaseEvent( QMouseEvent *e );
    void mouseMoveEvent( QMouseEvent *e );
    void keyPressEvent( QKeyEvent *e );

  private:
    QRect selection() const;
    QRect decorationRect() const;
    QRect labelRect( const QRect &sel ) const;
    QRect hintRect() const;
    void finish( const QPixmap &result );
    void setWaiting( bool wait );
    static QString selectHint();

    bool mouseDown;
    bool done;
    bool waiting; // for the grabs that a menu holds
    int grabTries;
    QPoint startPos;
    QPoint endPos;
    QPixmap pixmap;
    QPixmap dimmed;
    QString hint;
    QRect hintScreen;
};

#endif // REGIONGRABBER_H
