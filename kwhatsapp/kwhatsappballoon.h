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

#ifndef KWHATSAPPBALLOON_H
#define KWHATSAPPBALLOON_H

#include <qwidget.h>

class QPixmap;

/**
 * Kopete's balloon: a rounded tooltip-coloured window with a tail
 * pointing at the anchor (the tray icon), the sender's picture, the text
 * (rich text, already escaped) and the buttons View and Ignore.  A click
 * on the balloon itself is View too.  It does not delete itself: its
 * owner does, on one of the signals.
 */
class KWhatsAppBalloon : public QWidget
{
    Q_OBJECT

public:
    /** @p timeout in seconds (0: none): timedOut(). */
    KWhatsAppBalloon( const QString& text, const QPixmap& picture, int timeout );
    virtual ~KWhatsAppBalloon();

    /**
     * Placed at @p anchor (global): with the tail at it when @p tail,
     * else with its corner there.  The side is chosen by the screen of
     * the anchor (the balloon opens towards its centre).
     */
    void setAnchor( const QPoint& anchor, bool tail );

signals:
    void viewClicked();
    void ignoreClicked();
    void timedOut();

protected:
    virtual void updateMask();
    virtual void paintEvent( QPaintEvent* e );
    virtual void mouseReleaseEvent( QMouseEvent* e );

private:
    void layoutAnchor();

    QString m_text;
    class QLabel* m_caption;
    QPoint m_anchor;
    bool m_tail;
    bool m_right;    // the anchor is at the right ...
    bool m_bottom;   // ... and at the bottom of the balloon
};

#endif
