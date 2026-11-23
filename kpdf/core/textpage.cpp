/***************************************************************************
 *   Copyright (C) 2026 by Serghei Amelian <serghei.amelian@gmail.com>     *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 ***************************************************************************/

#include "textpage.h"

#include <math.h>

KPDFTextPage::KPDFTextPage( const QString & text, const QMemArray<float> & rects )
    : m_text( text ), m_rects( rects ), m_lastEnd( 0 ), m_lastValid( false )
{
    // one box per character; pad when the generator gave fewer
    uint have = m_rects.size() / 4;
    if ( have < m_text.length() )
    {
        m_rects.detach();
        m_rects.resize( 4 * m_text.length() );
        for ( uint i = 4 * have; i < m_rects.size(); ++i )
            m_rects[i] = 0;
    }
}

NormalizedRect KPDFTextPage::charRect( uint i ) const
{
    if ( i >= length() )
        return NormalizedRect();
    return NormalizedRect( m_rects[4*i], m_rects[4*i+1], m_rects[4*i+2], m_rects[4*i+3] );
}

bool KPDFTextPage::hasBox( uint i ) const
{
    return !m_text[i].isSpace() && m_rects[4*i+2] > m_rects[4*i] &&
           m_rects[4*i+3] > m_rects[4*i+1];
}

// the union of the boxes of the characters from..to-1; false if none has one
static bool unite( const KPDFTextPage * tp, uint from, uint to, NormalizedRect & r )
{
    bool any = false;
    for ( uint i = from; i < to; ++i )
    {
        if ( tp->text()[i].isSpace() )
            continue;
        NormalizedRect c = tp->charRect( i );
        if ( c.right <= c.left || c.bottom <= c.top )
            continue;
        if ( !any )
            r = c;
        else
        {
            r.left = QMIN( r.left, c.left );
            r.top = QMIN( r.top, c.top );
            r.right = QMAX( r.right, c.right );
            r.bottom = QMAX( r.bottom, c.bottom );
        }
        any = true;
    }
    return any;
}

// 1 left to right, -1 right to left, 0 neutral or weak
static int strongDirection( QChar c )
{
    switch ( c.direction() )
    {
        case QChar::DirL: case QChar::DirLRE: case QChar::DirLRO:
            return 1;
        case QChar::DirR: case QChar::DirAL: case QChar::DirRLE: case QChar::DirRLO:
            return -1;
        default:
            return 0;
    }
}

static bool sameRect( const NormalizedRect & a, const NormalizedRect & b )
{
    const double eps = 1e-6;
    return fabs( a.left - b.left ) < eps && fabs( a.top - b.top ) < eps &&
           fabs( a.right - b.right ) < eps && fabs( a.bottom - b.bottom ) < eps;
}

// what search compares: presentation forms and ligatures (U+FB00..U+FDFF,
// U+FE70..U+FEFF: Arabic shaping, "fi") as their letters, without
// formatting characters (direction marks), runs of white space as one space
static void appendSearchForm( QString & out, QChar c, bool & inSpace )
{
    if ( c.category() == QChar::Other_Format )
        return;
    if ( c.isSpace() )
    {
        if ( !inSpace )
            out += ' ';
        inSpace = true;
        return;
    }
    inSpace = false;
    ushort u = c.unicode();
    if ( ( u >= 0xfb00 && u <= 0xfdff ) || ( u >= 0xfe70 && u <= 0xfeff ) )
    {
        const QString & d = c.decomposition();
        if ( !d.isEmpty() )
        {
            out += d;
            return;
        }
    }
    out += c;
}

NormalizedRect * KPDFTextPage::findText( const QString & text, bool caseSensitive,
                                         const NormalizedRect * last ) const
{
    QString needle;
    bool inSpace = true;
    for ( uint i = 0; i < text.length(); ++i )
        appendSearchForm( needle, text[i], inSpace );
    needle = needle.stripWhiteSpace();
    if ( needle.isEmpty() || m_text.isEmpty() )
        return 0;

    // the page text in that form, and where each of its characters
    // comes from
    uint len = m_text.length();
    QString hay;
    hay.reserve( len );
    QMemArray<uint> from( len + 1 );
    uint n = 0;
    inSpace = false;
    for ( uint i = 0; i < len; ++i )
    {
        appendSearchForm( hay, m_text[i], inSpace );
        if ( hay.length() + 1 > from.size() )
            from.resize( 2 * hay.length() + 1 );
        while ( n < hay.length() )
            from[n++] = i;
    }
    from[n] = len;

    // matches in reading order.  With 'last': when it is the match this
    // page returned before, the next match begins after its text (two
    // matches can have one box: a ligature, text printed twice); else the
    // first match below 'last' or right of it on its line.
    bool continuing = last && m_lastValid && sameRect( *last, m_lastRect );
    uint minStart = continuing ? m_lastEnd : 0;
    NormalizedRect firstAfter;
    uint firstAfterEnd = 0;
    bool haveFirstAfter = false;
    int pos = 0;
    while ( ( pos = hay.find( needle, pos, caseSensitive ) ) >= 0 )
    {
        NormalizedRect r;
        uint start = from[pos], end = from[pos + needle.length() - 1] + 1;
        ++pos;
        if ( start < minStart || !unite( this, start, end, r ) )
            continue;
        if ( !last || continuing )
        {
            m_lastRect = r;
            m_lastEnd = end;
            m_lastValid = true;
            return new NormalizedRect( r );
        }
        if ( !haveFirstAfter && ( r.top > last->bottom ||
             ( r.top >= last->top - 1e-6 && r.left > last->left ) ) )
        {
            firstAfter = r;
            firstAfterEnd = end;
            haveFirstAfter = true;
        }
    }
    if ( !continuing && haveFirstAfter )
    {
        m_lastRect = firstAfter;
        m_lastEnd = firstAfterEnd;
        m_lastValid = true;
        return new NormalizedRect( firstAfter );
    }
    m_lastValid = false;
    return 0;
}

QString KPDFTextPage::getText( const NormalizedRect & area ) const
{
    uint len = length();
    QMemArray<bool> sel( len + 1 );
    for ( uint i = 0; i < len; ++i )
        sel[i] = hasBox( i ) && area.contains( cx( i ), cy( i ) );
    sel[len] = false;

    // white space counts when it lies between two selected characters
    QString result;
    bool nextBoxSelected = false;
    QMemArray<bool> keepSpace( len );
    for ( int i = (int)len - 1; i >= 0; --i )
    {
        if ( hasBox( i ) )
        {
            nextBoxSelected = sel[i];
            keepSpace[i] = false;
        }
        else
            keepSpace[i] = nextBoxSelected;
    }
    bool started = false;
    for ( uint i = 0; i < len; ++i )
    {
        if ( sel[i] )
        {
            result += m_text[i];
            started = true;
        }
        else if ( started && !hasBox( i ) && keepSpace[i] && m_text[i].isSpace() )
            result += m_text[i];
    }
    return result;
}

uint KPDFTextPage::cursorAt( double x, double y ) const
{
    // 1. the line: the character box nearest to y; when several boxes
    //    contain y (tall font boxes of neighbouring lines overlap), the one
    //    whose centre is nearest
    uint len = length();
    int line = -1;
    double bestDy = 0, bestDc = 0;
    for ( uint i = 0; i < len; ++i )
    {
        if ( !hasBox( i ) )
            continue;
        double t = m_rects[4*i+1], b = m_rects[4*i+3];
        double dy = y < t ? t - y : ( y > b ? y - b : 0 );
        double dc = fabs( cy( i ) - y );
        if ( line < 0 || dy < bestDy - 1e-9 || ( fabs( dy - bestDy ) <= 1e-9 && dc < bestDc ) )
        {
            line = i;
            bestDy = dy;
            bestDc = dc;
        }
    }
    if ( line < 0 )
        return 0;

    // 2. on that line (boxes overlapping it vertically by half), the
    //    character nearest to x
    double lt = m_rects[4*line+1], lb = m_rects[4*line+3];
    int best = line;
    double bestDx = -1;
    for ( uint i = 0; i < len; ++i )
    {
        if ( !hasBox( i ) )
            continue;
        double t = m_rects[4*i+1], b = m_rects[4*i+3];
        double overlap = QMIN( b, lb ) - QMAX( t, lt );
        if ( overlap < QMIN( b - t, lb - lt ) / 2 )
            continue;
        double l = m_rects[4*i], r = m_rects[4*i+2];
        double dx = x < l ? l - x : ( x > r ? x - r : 0 );
        if ( bestDx < 0 || dx < bestDx )
        {
            best = i;
            bestDx = dx;
        }
    }

    // before or after the character, in its writing direction: its own
    // bidi class, else the nearest strong character of its line, else the
    // order of the boxes
    uint i = best;
    int dir = strongDirection( m_text[i] );
    for ( uint k = 1; dir == 0 && k <= 32; ++k )
    {
        bool any = false;
        if ( i + k < len && m_text[i + k] != '\n' )
        {
            dir = strongDirection( m_text[i + k] );
            any = true;
        }
        if ( dir == 0 && i >= k && m_text[i - k] != '\n' )
        {
            dir = strongDirection( m_text[i - k] );
            any = true;
        }
        if ( !any )
            break;
    }
    if ( dir == 0 )
    {
        dir = 1;
        for ( int j = (int)i + 1; j < (int)len && j <= (int)i + 8; ++j )
            if ( hasBox( j ) )
            {
                if ( cy( j ) >= m_rects[4*i+1] && cy( j ) <= m_rects[4*i+3] && cx( j ) < cx( i ) )
                    dir = -1;
                break;
            }
    }
    bool after = dir < 0 ? x < cx( i ) : x > cx( i );

    // a character outside the BMP is two QChars with one box: never
    // between them
    uint start = i, end = i + 1;
    ushort u = m_text[i].unicode();
    if ( u >= 0xd800 && u <= 0xdbff && end < len &&
         m_text[end].unicode() >= 0xdc00 && m_text[end].unicode() <= 0xdfff )
        ++end;
    else if ( u >= 0xdc00 && u <= 0xdfff && start > 0 &&
              m_text[start - 1].unicode() >= 0xd800 && m_text[start - 1].unicode() <= 0xdbff )
        --start;
    return after ? end : start;
}

QString KPDFTextPage::text( uint from, uint to ) const
{
    uint a = QMIN( from, to ), b = QMIN( QMAX( from, to ), length() );
    if ( a >= b )
        return QString::null;
    return m_text.mid( a, b - a );
}

QValueList<NormalizedRect> KPDFTextPage::selectionRects( uint from, uint to ) const
{
    QValueList<NormalizedRect> list;
    uint a = QMIN( from, to ), b = QMIN( QMAX( from, to ), length() );
    bool open = false;
    NormalizedRect run;
    for ( uint i = a; i < b; ++i )
    {
        if ( !hasBox( i ) )
            continue;
        NormalizedRect c = charRect( i );
        if ( open )
        {
            // same line: the boxes overlap vertically by half the smaller one
            double overlap = QMIN( run.bottom, c.bottom ) - QMAX( run.top, c.top );
            double minHeight = QMIN( run.bottom - run.top, c.bottom - c.top );
            if ( overlap >= minHeight / 2 )
            {
                run.left = QMIN( run.left, c.left );
                run.top = QMIN( run.top, c.top );
                run.right = QMAX( run.right, c.right );
                run.bottom = QMAX( run.bottom, c.bottom );
                continue;
            }
            list.append( run );
        }
        run = c;
        open = true;
    }
    if ( open )
        list.append( run );
    return list;
}
