/***************************************************************************
 *   Copyright (C) 2026 by Serghei Amelian <serghei.amelian@gmail.com>     *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 ***************************************************************************/

// kpdfcheck: drives KPDF's document core (KPDFDocument with the poppler
// generator) without its views, for tests: renders pages, prints texts,
// search hits, links, the outline, times and memory.  Needs an X display
// (KApplication).

#include <qdatetime.h>
#include <qdom.h>
#include <qfile.h>
#include <qimage.h>
#include <qpainter.h>
#include <qpixmap.h>
#include <qtextstream.h>
#include <qwidget.h>
#include <kapplication.h>
#include <kcmdlineargs.h>
#include <kmimetype.h>
#include <kprinter.h>
#include <kurl.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core/document.h"
#include "core/generator.h"
#include "core/link.h"
#include "core/observer.h"
#include "core/page.h"
#include "core/textpage.h"
#include "ui/pagepainter.h"
#include "conf/settings.h"

// gets the pixmaps of the requests and counts them
class Observer : public DocumentObserver
{
    public:
        Observer() : done( 0 ) {}
        uint observerId() const { return PAGEVIEW_ID; }
        void notifyPageChanged( int, int flags ) { if ( flags & Pixmap ) ++done; }
        bool canUnloadPixmap( int ) { return true; }
        int done;
};

static long rssKiB()
{
    QFile f( "/proc/self/status" );
    if ( !f.open( IO_ReadOnly ) )
        return -1;
    QTextStream ts( &f );
    while ( !ts.atEnd() )
    {
        QString l = ts.readLine();
        if ( l.startsWith( "VmRSS:" ) )
            return l.mid( 6 ).stripWhiteSpace().section( ' ', 0, 0 ).toLong();
    }
    return -1;
}

static QTextStream out( stdout, IO_WriteOnly );

static void waitFor( Observer & obs, int count )
{
    while ( obs.done < count )
        kapp->processEvents( 10 );
}

static void printOutline( KPDFDocument * doc, const QDomNode & parent, int depth )
{
    for ( QDomNode n = parent.firstChild(); !n.isNull(); n = n.nextSibling() )
    {
        QDomElement e = n.toElement();
        QString vp = e.attribute( "Viewport" );
        if ( vp.isEmpty() && e.hasAttribute( "ViewportName" ) )
            vp = doc->getMetaData( "NamedViewport", e.attribute( "ViewportName" ) );
        DocumentViewport v = vp.isEmpty() ? DocumentViewport() : DocumentViewport( vp );
        out << QString().fill( ' ', 2 * depth ) << e.tagName() << " -> page " << v.pageNumber + 1;
        if ( v.rePos.enabled )
            out << QString( " at %1,%2" ).arg( v.rePos.normalizedX, 0, 'f', 3 ).arg( v.rePos.normalizedY, 0, 'f', 3 );
        out << "\n";
        printOutline( doc, n, depth + 1 );
    }
}

static QString linkText( const KPDFLink * l )
{
    switch ( l->linkType() )
    {
        case KPDFLink::Goto:
        {
            const KPDFLinkGoto * g = static_cast< const KPDFLinkGoto * >( l );
            return QString( "goto page %1%2 file '%3'" ).arg( g->destViewport().pageNumber + 1 )
                   .arg( g->destViewport().rePos.enabled ? QString( " y=%1" ).arg( g->destViewport().rePos.normalizedY, 0, 'f', 3 ) : QString() )
                   .arg( g->fileName() );
        }
        case KPDFLink::Browse:
            return "uri " + static_cast< const KPDFLinkBrowse * >( l )->url();
        case KPDFLink::Execute:
            return "launch " + static_cast< const KPDFLinkExecute * >( l )->fileName();
        case KPDFLink::Action:
            return QString( "action %1" ).arg( static_cast< const KPDFLinkAction * >( l )->actionType() );
        default:
            return "other";
    }
}

int main( int argc, char ** argv )
{
    if ( argc < 3 )
    {
        fprintf( stderr,
            "usage: kpdfcheck render FILE PAGE WIDTH OUT.png|-\n"
            "       kpdfcheck text FILE PAGE\n"
            "       kpdfcheck boxes FILE PAGE FROM TO     (character boxes)\n"
            "       kpdfcheck cursor FILE PAGE X Y        (text cursor position at a point)\n"
            "       kpdfcheck images FILE PAGE            (image rects)\n"
            "       kpdfcheck rect FILE PAGE L T R B      (text of a normalized rect)\n"
            "       kpdfcheck select FILE PAGE X1 Y1 X2 Y2 (text selection between two points)\n"
            "       kpdfcheck search FILE WORD [case]\n"
            "       kpdfcheck links FILE PAGE\n"
            "       kpdfcheck outline FILE\n"
            "       kpdfcheck info FILE\n"
            "       kpdfcheck firstpage FILE WIDTH        (time to first page, threaded)\n"
            "       kpdfcheck walk FILE WIDTH [PAGES]     (render pages in turn, memory)\n"
            "       kpdfcheck print FILE OUT.ps FROM TO   (print to a PostScript file)\n" );
        return 2;
    }
    // KApplication takes the command line; keep ours
    QString cmd = argv[1], file = QFile::decodeName( argv[2] );
    QStringList args;
    for ( int i = 3; i < argc; ++i )
        args << QString::fromLocal8Bit( argv[i] );
    int fakeArgc = 1;
    KCmdLineArgs::init( fakeArgc, argv, "kpdfcheck", "kpdfcheck", "KPDF core check", "1.0" );
    KApplication app;

    QTime t;
    t.start();
    QWidget w;
    KPDFDocument * doc = new KPDFDocument( &w );
    Observer obs;
    doc->addObserver( &obs );
    // as Part does: closing sets up the viewport history
    doc->closeDocument();
    KURL url;
    url.setPath( file );
    if ( !doc->openDocument( file, url, KMimeType::findByPath( file ) ) )
    {
        out << "open failed\n";
        return 1;
    }
    int openMs = t.elapsed();
    uint pages = doc->pages();

    if ( cmd == "info" )
    {
        out << "pages " << pages << " open_ms " << openMs << "\n";
        const DocumentInfo * info = doc->documentInfo();
        for ( QDomNode n = info->documentElement().firstChild(); !n.isNull(); n = n.nextSibling() )
        {
            QDomElement e = n.toElement();
            out << e.tagName() << ": " << e.attribute( "value" ) << "\n";
        }
        out << "copy_allowed " << doc->isAllowed( KPDFDocument::AllowCopy ) << "\n";
        out << "print_allowed " << doc->isAllowed( KPDFDocument::AllowPrint ) << "\n";
        for ( uint i = 0; i < pages && i < 8; ++i )
            out << "page " << i + 1 << " size " << doc->page( i )->width() << "x" << doc->page( i )->height() << "\n";
    }
    else if ( cmd == "render" && args.count() >= 3 )
    {
        int page = args[0].toInt() - 1, width = args[1].toInt();
        if ( page < 0 || page >= (int)pages || width <= 0 )
        {
            out << "bad page or width\n";
            return 1;
        }
        const KPDFPage * kp = doc->page( page );
        int height = (int)( kp->ratio() * width + 0.5 );
        QValueList< PixmapRequest * > req;
        req.push_back( new PixmapRequest( PAGEVIEW_ID, page, width, height, 1, false ) );
        t.restart();
        doc->requestPixmaps( req );
        waitFor( obs, 1 );
        out << "render_ms " << t.elapsed() << " size " << width << "x" << height << "\n";
        if ( !kp->hasPixmap( PAGEVIEW_ID, width, height ) )
        {
            out << "no pixmap\n";
            return 1;
        }
        // through the painter of the views, without their decorations
        QPixmap pm( width, height );
        QPainter painter( &pm );
        PagePainter::paintPageOnPainter( kp, PAGEVIEW_ID, 0, &painter, QRect( 0, 0, width, height ), width, height );
        painter.end();
        QImage img = pm.convertToImage();
        long ink = 0;
        for ( int y = 0; y < img.height(); ++y )
            for ( int x = 0; x < img.width(); ++x )
                if ( ( img.pixel( x, y ) & 0xffffff ) != 0xffffff )
                    ++ink;
        out << "non_white_pixels " << ink << "\n";
        if ( args[2] != "-" && !img.save( args[2], "PNG" ) )
        {
            out << "save failed\n";
            return 1;
        }
    }
    else if ( ( cmd == "text" || cmd == "boxes" || cmd == "cursor" || cmd == "images" || cmd == "rect" || cmd == "select" || cmd == "links" ) && args.count() >= 1 )
    {
        int page = args[0].toInt() - 1;
        if ( page < 0 || page >= (int)pages )
            return 1;
        const KPDFPage * kp = doc->page( page );
        doc->requestTextPage( page );
        const KPDFTextPage * tp = kp->textPage();
        if ( cmd == "text" )
            out << tp->text();
        else if ( cmd == "cursor" && args.count() >= 3 )
            out << "cursor " << tp->cursorAt( args[1].toDouble(), args[2].toDouble() ) << " of " << tp->length() << "\n";
        else if ( cmd == "images" )
        {
            // image rects come with a pixmap of the page view, when shown
            KpdfSettings::setHighlightImages( true );
            QValueList< PixmapRequest * > req;
            req.push_back( new PixmapRequest( PAGEVIEW_ID, page, 200, (int)( 200 * kp->ratio() ), 1, false ) );
            doc->requestPixmaps( req );
            waitFor( obs, 1 );
            const QValueList< ObjectRect * > & rects = kp->objectRects();
            QValueList< ObjectRect * >::const_iterator it = rects.begin(), end = rects.end();
            for ( ; it != end; ++it )
                if ( (*it)->objectType() == ObjectRect::Image )
                    out << QString( "image %1 %2 %3 %4\n" ).arg( (*it)->left, 0, 'f', 3 ).arg( (*it)->top, 0, 'f', 3 )
                           .arg( (*it)->right, 0, 'f', 3 ).arg( (*it)->bottom, 0, 'f', 3 );
        }
        else if ( cmd == "boxes" && args.count() >= 3 )
            for ( uint i = args[1].toUInt(); i < tp->length() && i < args[2].toUInt(); ++i )
            {
                NormalizedRect r = tp->charRect( i );
                out << QString( "%1 U+%2 %3 %4 %5 %6\n" ).arg( i ).arg( tp->text()[i].unicode(), 4, 16 )
                       .arg( r.left, 0, 'f', 3 ).arg( r.top, 0, 'f', 3 ).arg( r.right, 0, 'f', 3 ).arg( r.bottom, 0, 'f', 3 );
            }
        else if ( cmd == "rect" && args.count() >= 5 )
            out << kp->getText( NormalizedRect( args[1].toDouble(), args[2].toDouble(), args[3].toDouble(), args[4].toDouble() ) );
        else if ( cmd == "select" && args.count() >= 5 )
        {
            uint a = tp->cursorAt( args[1].toDouble(), args[2].toDouble() ),
                 b = tp->cursorAt( args[3].toDouble(), args[4].toDouble() );
            out << "cursor " << a << " " << b << "\n" << tp->text( a, b ) << "\n";
            QValueList< NormalizedRect > rects = tp->selectionRects( a, b );
            out << "rects " << rects.count() << "\n";
        }
        else if ( cmd == "links" )
        {
            // links come with a pixmap of the page view
            QValueList< PixmapRequest * > req;
            req.push_back( new PixmapRequest( PAGEVIEW_ID, page, 200, (int)( 200 * kp->ratio() ), 1, false ) );
            doc->requestPixmaps( req );
            waitFor( obs, 1 );
            int n = 0;
            const QValueList< ObjectRect * > & rects = kp->objectRects();
            QValueList< ObjectRect * >::const_iterator it = rects.begin(), end = rects.end();
            for ( ; it != end; ++it )
            {
                if ( (*it)->objectType() != ObjectRect::Link )
                    continue;
                ++n;
                const KPDFLink * l = static_cast< const KPDFLink * >( (*it)->pointer() );
                out << QString( "link %1 %2 %3 %4 " ).arg( (*it)->left, 0, 'f', 3 ).arg( (*it)->top, 0, 'f', 3 )
                       .arg( (*it)->right, 0, 'f', 3 ).arg( (*it)->bottom, 0, 'f', 3 ) << linkText( l ) << "\n";
            }
            out << "links " << n << "\n";
        }
    }
    else if ( cmd == "search" && args.count() >= 1 )
    {
        bool cs = args.count() > 1 && args[1] == "case";
        t.restart();
        int total = 0, pagesWithHits = 0;
        for ( uint p = 0; p < pages; ++p )
        {
            const KPDFPage * kp = doc->page( p );
            if ( !kp->hasSearchPage() )
                doc->requestTextPage( p );
            NormalizedRect * last = 0;
            int n = 0;
            while ( NormalizedRect * r = kp->findText( args[0], cs, last ) )
            {
                if ( n < 2 && total < 6 )
                    out << QString( "hit page %1 rect %2 %3 %4 %5\n" ).arg( p + 1 ).arg( r->left, 0, 'f', 3 )
                           .arg( r->top, 0, 'f', 3 ).arg( r->right, 0, 'f', 3 ).arg( r->bottom, 0, 'f', 3 );
                delete last;
                last = r;
                if ( ++n > 100000 )
                    break;
            }
            delete last;
            total += n;
            if ( n )
                ++pagesWithHits;
        }
        out << "hits " << total << " pages " << pagesWithHits << " search_ms " << t.elapsed() << "\n";
    }
    else if ( cmd == "outline" )
    {
        const DocumentSynopsis * syn = doc->documentSynopsis();
        if ( !syn )
            out << "no outline\n";
        else
            printOutline( doc, *syn, 0 );
        out << "OpenTOC " << doc->getMetaData( "OpenTOC" ) << "\n";
    }
    else if ( cmd == "firstpage" && args.count() >= 1 )
    {
        int width = args[0].toInt();
        const KPDFPage * kp = doc->page( 0 );
        QValueList< PixmapRequest * > req;
        req.push_back( new PixmapRequest( PAGEVIEW_ID, 0, width, (int)( kp->ratio() * width ), 1, true ) );
        doc->requestPixmaps( req );
        waitFor( obs, 1 );
        out << "pages " << pages << " open_ms " << openMs << " first_page_ms " << t.elapsed()
            << " rss_kib " << rssKiB() << "\n";
    }
    else if ( cmd == "walk" && args.count() >= 1 )
    {
        int width = args[0].toInt();
        uint n = args.count() > 1 ? QMIN( (uint)args[1].toInt(), pages ) : pages;
        long peak = 0;
        t.restart();
        for ( uint p = 0; p < n; ++p )
        {
            const KPDFPage * kp = doc->page( p );
            QValueList< PixmapRequest * > req;
            req.push_back( new PixmapRequest( PAGEVIEW_ID, p, width, (int)( kp->ratio() * width ), 1, true ) );
            doc->requestPixmaps( req );
            waitFor( obs, p + 1 );
            long rss = rssKiB();
            if ( rss > peak )
                peak = rss;
        }
        out << "pages " << n << " walk_ms " << t.elapsed() << " ms_per_page " << ( n ? t.elapsed() / n : 0 )
            << " rss_peak_kib " << peak << " rss_end_kib " << rssKiB() << "\n";
    }
    else if ( cmd == "print" && args.count() >= 3 )
    {
        KPrinter printer;
        printer.setOutputToFile( true );
        printer.setOutputFileName( args[0] );
        printer.setPageSelection( KPrinter::ApplicationSide );
        printer.setMinMax( 1, pages );
        printer.setFromTo( args[1].toInt(), args[2].toInt() );
        t.restart();
        bool ok = doc->print( printer );
        out << "print " << ( ok ? "ok" : "failed" ) << " print_ms " << t.elapsed() << "\n";
    }
    else
    {
        fprintf( stderr, "bad command\n" );
        return 2;
    }
    out.device()->flush();
    doc->closeDocument();
    delete doc;
    return 0;
}
