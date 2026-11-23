/***************************************************************************
 *   Copyright (C) 2026 by Serghei Amelian <serghei.amelian@gmail.com>     *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 ***************************************************************************/

#ifndef _KPDF_TEXTPAGE_H_
#define _KPDF_TEXTPAGE_H_

#include <qcolor.h>
#include <qmemarray.h>
#include <qstring.h>
#include <qvaluelist.h>

#include "core/page.h"

/**
 * @short The text of a page and the box of each of its characters.
 *
 * Generators fill it (the PDF generator from poppler's text layout) and
 * KPDFPage owns it.  The text is in reading order, lines separated by
 * '\n'; character i has the box charRect(i) in normalized page
 * coordinates (0..1, origin at the top left corner of the page as
 * shown, rotation applied).  Search, the text of a rectangle and the
 * text selection work on these two only.
 */
class KPDFTextPage
{
    public:
        // 'rects' holds 4 values per character of 'text': left, top,
        // right, bottom, normalized.  Missing boxes count as empty.
        KPDFTextPage( const QString & text, const QMemArray<float> & rects );

        const QString & text() const { return m_text; }
        uint length() const { return m_text.length(); }
        NormalizedRect charRect( uint i ) const;

        // the bounding box of the next match after 'last' (the previous
        // match, or 0 to start at the top); 0 if there is none.  Runs of
        // white space (line ends too) match any run of white space.
        NormalizedRect * findText( const QString & text, bool caseSensitive,
                                   const NormalizedRect * last = 0 ) const;

        // the text of the characters whose centre lies in 'area'
        QString getText( const NormalizedRect & area ) const;

        // text selection: the cursor position (0..length()) nearest to the
        // point (x, y), between two characters
        uint cursorAt( double x, double y ) const;
        // the text and the boxes (one per line part) of the characters
        // from 'from' to 'to' (cursor positions, either order)
        QString text( uint from, uint to ) const;
        QValueList<NormalizedRect> selectionRects( uint from, uint to ) const;

    private:
        bool hasBox( uint i ) const;
        float cx( uint i ) const { return ( m_rects[4*i] + m_rects[4*i+2] ) / 2; }
        float cy( uint i ) const { return ( m_rects[4*i+1] + m_rects[4*i+3] ) / 2; }

        QString m_text;
        QMemArray<float> m_rects;
};

#endif
