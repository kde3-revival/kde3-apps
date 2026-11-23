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
    : m_text( text ), m_rects( rects )
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

static bool sameRect( const NormalizedRect & a, const NormalizedRect & b )
{
    const double eps = 1e-6;
    return fabs( a.left - b.left ) < eps && fabs( a.top - b.top ) < eps &&
           fabs( a.right - b.right ) < eps && fabs( a.bottom - b.bottom ) < eps;
}

NormalizedRect * KPDFTextPage::findText( const QString & text, bool caseSensitive,
                                         const NormalizedRect * last ) const
{
    QString needle = text.simplifyWhiteSpace();
    if ( needle.isEmpty() || m_text.isEmpty() )
        return 0;

    // the page text with every run of white space made one space, and
    // where each of its characters comes from
    uint len = m_text.length();
    QString hay;
    hay.reserve( len );
    QMemArray<uint> from( len + 1 );
    uint n = 0;
    bool inSpace = false;
    for ( uint i = 0; i < len; ++i )
    {
        QChar c = m_text[i];
        if ( c.isSpace() )
        {
            if ( inSpace )
                continue;
            inSpace = true;
            c = ' ';
        }
        else
            inSpace = false;
        hay += c;
        from[n++] = i;
    }
    from[n] = len;

    // matches in reading order; with 'last', the one after it
    bool passedLast = false;
    NormalizedRect firstAfter;
    bool haveFirstAfter = false;
    int pos = 0;
    while ( ( pos = hay.find( needle, pos, caseSensitive ) ) >= 0 )
    {
        NormalizedRect r;
        uint start = from[pos], end = from[pos + needle.length()];
        ++pos;
        if ( !unite( this, start, end, r ) )
            continue;
        if ( !last || passedLast )
            return new NormalizedRect( r );
        if ( sameRect( r, *last ) )
        {
            passedLast = true;
            continue;
        }
        // in case 'last' is not a match of this text: the first match
        // below it, or right of it on its line
        if ( !haveFirstAfter && ( r.top > last->bottom ||
             ( r.top >= last->top - 1e-6 && r.left > last->left ) ) )
        {
            firstAfter = r;
            haveFirstAfter = true;
        }
    }
    if ( !passedLast && haveFirstAfter )
        return new NormalizedRect( firstAfter );
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
    uint len = length();
    int best = -1;
    double bestDy = 0, bestDx = 0;
    for ( uint i = 0; i < len; ++i )
    {
        if ( !hasBox( i ) )
            continue;
        double l = m_rects[4*i], t = m_rects[4*i+1], r = m_rects[4*i+2], b = m_rects[4*i+3];
        double dy = y < t ? t - y : ( y > b ? y - b : 0 );
        double dx = x < l ? l - x : ( x > r ? x - r : 0 );
        // the nearest line first, then the nearest character on it
        if ( best < 0 || dy < bestDy - 1e-9 || ( fabs( dy - bestDy ) <= 1e-9 && dx < bestDx ) )
        {
            best = i;
            bestDy = dy;
            bestDx = dx;
        }
    }
    if ( best < 0 )
        return 0;

    // before or after the character, in the direction of its line
    uint i = best;
    bool rtl = false;
    for ( int j = (int)i + 1; j < (int)len && j <= (int)i + 8; ++j )
        if ( hasBox( j ) )
        {
            if ( cy( j ) >= m_rects[4*i+1] && cy( j ) <= m_rects[4*i+3] )
                rtl = cx( j ) < cx( i );
            break;
        }
    bool after = rtl ? x < cx( i ) : x > cx( i );
    return after ? i + 1 : i;
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
