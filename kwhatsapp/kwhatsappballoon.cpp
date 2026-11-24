/*
    kwhatsapp: the new message balloon at the system tray icon

    Copyright (C) 2026 The kde3-revival authors

    Based on Kopete's KopeteBalloon (kdenetwork 3.5, kopete/kopete/
    kopeteballoon.{h,cpp}, as kept in TDE's tdenetwork):
    Copyright (c) 2002      by Duncan Mac-Vicar Prett <duncan@kde.org>
    Kopete    (c) 2002-2005 by the Kopete developers  <kopete-devel@kde.org>
    Portions based on Kim Applet code
    Copyright (c) 2000-2002 by Malte Starostik        <malte@kde.org>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.
*/

#include "kwhatsappballoon.h"

#include <stdio.h>
#include <stdlib.h>

#include <qlabel.h>
#include <qlayout.h>
#include <qpainter.h>
#include <qpointarray.h>
#include <qpushbutton.h>
#include <qtimer.h>
#include <qtooltip.h>

#include <kdialog.h>
#include <kglobalsettings.h>
#include <klocale.h>

KWhatsAppBalloon::KWhatsAppBalloon( const QString& text, const QPixmap& picture, int timeout )
    : QWidget( 0, "KWhatsAppBalloon", WStyle_StaysOnTop | WStyle_Customize | WStyle_NoBorder | WStyle_Tool
               | WX11BypassWM | WDestructiveClose ),
      m_tail( false ), m_right( true ), m_bottom( true )
{
    // (not shown: no frame; for tools that look for it)
    setCaption( i18n( "New message" ) );

    QVBoxLayout* balloonLayout = new QVBoxLayout( this, 22, KDialog::spacingHint() );

    // the picture, the text
    QHBoxLayout* layout1 = new QHBoxLayout( balloonLayout, KDialog::spacingHint() );
    if ( !picture.isNull() ) {
        QLabel* image = new QLabel( this, "picture" );
        image->setScaledContents( false );
        image->setPixmap( picture );
        image->setAlignment( Qt::AlignTop );
        layout1->addWidget( image );
    }
    QLabel* caption = new QLabel( text, this, "text" );
    caption->setTextFormat( Qt::RichText );
    caption->setPalette( QToolTip::palette() );
    caption->setSizePolicy( QSizePolicy::Minimum, QSizePolicy::Minimum );
    layout1->addWidget( caption );

    // the buttons
    QHBoxLayout* layout2 = new QHBoxLayout( balloonLayout, KDialog::spacingHint() );
    QPushButton* viewButton = new QPushButton( i18n( "to view", "View" ), this, "view" );
    QPushButton* ignoreButton = new QPushButton( i18n( "Ignore" ), this, "ignore" );
    layout2->addStretch();
    layout2->addWidget( viewButton );
    layout2->addWidget( ignoreButton );
    layout2->addStretch();

    setPalette( QToolTip::palette() );
    setAutoMask( true );

    connect( viewButton, SIGNAL( clicked() ), SIGNAL( viewClicked() ) );
    connect( ignoreButton, SIGNAL( clicked() ), SIGNAL( ignoreClicked() ) );
    if ( timeout > 0 )
        QTimer::singleShot( timeout * 1000, this, SIGNAL( timedOut() ) );
}

KWhatsAppBalloon::~KWhatsAppBalloon()
{
    if ( getenv( "KWHATSAPP_DEBUG" ) )
        fprintf( stderr, "kwhatsapp: balloon destroyed\n" );
}

void KWhatsAppBalloon::setAnchor( const QPoint& anchor, bool tail )
{
    m_anchor = anchor;
    m_tail = tail;
    adjustSize();
    layoutAnchor();
}

// The balloon opens towards the centre of the anchor's screen (its own
// geometry with several screens).
void KWhatsAppBalloon::layoutAnchor()
{
    QRect desk = KGlobalSettings::desktopGeometry( m_anchor );
    m_right = m_anchor.x() > desk.center().x();
    m_bottom = m_anchor.y() > desk.center().y();
    int x = m_right ? m_anchor.x() - width() + 1 : m_anchor.x();
    int y = m_bottom ? m_anchor.y() - height() + 1 : m_anchor.y();
    x = QMAX( desk.left(), QMIN( x, desk.right() - width() + 1 ) );
    y = QMAX( desk.top(), QMIN( y, desk.bottom() - height() + 1 ) );
    move( x, y );
    updateMask();
}

// Kopete's shape: a rectangle with round corners 10 pixels in, the tail
// from the corner at the anchor.
void KWhatsAppBalloon::updateMask()
{
    QRegion mask( 10, 10, width() - 20, height() - 20 );

    QPoint corners[8] = {
        QPoint( width() - 50, 10 ),
        QPoint( 10, 10 ),
        QPoint( 10, height() - 50 ),
        QPoint( width() - 50, height() - 50 ),
        QPoint( width() - 10, 10 ),
        QPoint( 10, 10 ),
        QPoint( 10, height() - 10 ),
        QPoint( width() - 10, height() - 10 )
    };

    for ( int i = 0; i < 4; ++i ) {
        QPointArray corner;
        corner.makeArc( corners[i].x(), corners[i].y(), 40, 40, i * 16 * 90, 16 * 90 );
        corner.resize( corner.size() + 1 );
        corner.setPoint( corner.size() - 1, corners[i + 4] );
        mask -= corner;
    }

    if ( m_tail ) {
        QPointArray arrow( 4 );
        arrow.setPoint( 0, QPoint( m_right ? width() : 0, m_bottom ? height() : 0 ) );
        arrow.setPoint( 1, QPoint( m_right ? width() - 10 : 10, m_bottom ? height() - 30 : 30 ) );
        arrow.setPoint( 2, QPoint( m_right ? width() - 30 : 30, m_bottom ? height() - 10 : 10 ) );
        arrow.setPoint( 3, arrow[0] );
        mask += QRegion( arrow );
    }
    setMask( mask );
}

// An outline in the tooltip's text colour (Kopete had none: the balloon
// vanished on a tooltip-coloured background).
void KWhatsAppBalloon::paintEvent( QPaintEvent* e )
{
    QWidget::paintEvent( e );
    QPainter p( this );
    QColor line = colorGroup().foreground();
    p.setPen( line );
    p.setBrush( NoBrush );
    int w = width() - 21, h = height() - 21;
    p.drawRoundRect( 10, 10, w + 1, h + 1, QMIN( 99, 4000 / QMAX( w, 1 ) ), QMIN( 99, 4000 / QMAX( h, 1 ) ) );
    if ( m_tail ) {
        QPoint tip( m_right ? width() - 1 : 0, m_bottom ? height() - 1 : 0 );
        QPoint a( m_right ? width() - 11 : 10, m_bottom ? height() - 31 : 30 );
        QPoint b( m_right ? width() - 31 : 30, m_bottom ? height() - 11 : 10 );
        // the rectangle's outline inside the tail: gone
        QPointArray inside( 3 );
        inside.setPoint( 0, tip );
        inside.setPoint( 1, a );
        inside.setPoint( 2, b );
        p.setPen( NoPen );
        p.setBrush( colorGroup().background() );
        p.drawPolygon( inside );
        p.setPen( line );
        p.drawLine( tip, a );
        p.drawLine( tip, b );
    }
}

// A click on the balloon (not on a button): View, as Kopete meant to.
void KWhatsAppBalloon::mouseReleaseEvent( QMouseEvent* e )
{
    if ( e->button() == LeftButton && rect().contains( e->pos() ) )
        emit viewClicked();
}

#include "kwhatsappballoon.moc"
