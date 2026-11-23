/***************************************************************************
 *   Copyright (C) 2004 by Albert Astals Cid <tsdgeos@terra.es>            *
 *   Copyright (C) 2004 by Enrico Ros <eros.kde@email.it>                  *
 *   Copyright (C) 2026 by Serghei Amelian <serghei.amelian@gmail.com>     *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 ***************************************************************************/

// poppler-glib first: GLib names struct members 'signals', which Qt
// defines as a macro
#include <poppler.h>
#include <cairo.h>
#include <cairo-ps.h>

// qt/kde includes
#include <qdatetime.h>
#include <qfile.h>
#include <qevent.h>
#include <qimage.h>
#include <qapplication.h>
#include <qpaintdevicemetrics.h>
#include <qregexp.h>
#include <qvariant.h>
#include <kapplication.h>
#include <kglobal.h>
#include <klistview.h>
#include <klocale.h>
#include <kpassdlg.h>
#include <kwallet.h>
#include <kprinter.h>
#include <ktempfile.h>
#include <kmessagebox.h>
#include <kdebug.h>

// local includes
#include "generator_poppler.h"
#include "core/observer.h" // for PAGEVIEW_ID
#include "core/page.h"
#include "core/pagetransition.h"
#include "core/textpage.h"
#include "conf/settings.h"

#include <math.h>
#include <stdlib.h>

#include <config.h>

// id for the DATA_READY PopplerRenderThread event
#define TGE_DATAREADY_ID 6969

/** NOTES on threading (as with the xpdf generator before):
 * internal: poppler may not be used on one document from two threads at
 *           once, so every use of the document is locked with 'docLock'.
 *           Only the asynchronous render thread makes it necessary; the
 *           rest runs in the GUI thread.
 * external: canGeneratePixmap() tells if the generator is free (there is
 *           no internal queue of PixmapRequests); calling generatePixmap()
 *           when it is not gives undefined behaviour.
 * Printing or extracting text while a page renders in the background is
 * safe: the GUI thread waits for the render to finish.
 */

static QString fromUtf8( const char * s )
{
    return s ? QString::fromUtf8( s ) : QString::null;
}

static QString orUnknown( const QString & s )
{
    return s.isEmpty() ? i18n( "Unknown" ) : s;
}

PopplerGenerator::PopplerGenerator( KPDFDocument * doc )
    : Generator( doc ), m_doc( 0 ), m_usedPassword( false ), ready( true ),
    docInfoDirty( true ), docSynopsisDirty( true )
{
    reparseConfig();
    renderThread = new PopplerRenderThread( this );
}

PopplerGenerator::~PopplerGenerator()
{
    // first stop and delete the render thread
    if ( renderThread )
    {
        renderThread->wait();
        delete renderThread;
    }
    docLock.lock();
    for ( uint i = 0; i < m_pages.count(); ++i )
        if ( m_pages[i] )
            g_object_unref( m_pages[i] );
    m_pages.clear();
    if ( m_doc )
        g_object_unref( m_doc );
    m_doc = 0;
    docLock.unlock();
}


//BEGIN Generator inherited functions
bool PopplerGenerator::loadDocument( const QString & filePath, QValueVector<KPDFPage*> & pagesVector )
{
    if ( m_doc )
    {
        kdDebug() << "PopplerGenerator: multiple calls to loadDocument. Check it." << endl;
        return false;
    }

    gchar * uri = g_filename_to_uri( QFile::encodeName( filePath ), 0, 0 );
    if ( !uri )
        return false;

    GError * error = 0;
    m_doc = poppler_document_new_from_file( uri, 0, &error );

    // if the file is encrypted, ask for the password
    bool firstInput = true;
    bool triedWallet = false;
    KWallet::Wallet * wallet = 0;
    int keep = 1;
    while ( !m_doc && error && error->domain == POPPLER_ERROR &&
            error->code == POPPLER_ERROR_ENCRYPTED )
    {
        g_error_free( error );
        error = 0;
        QCString password;

        // 1.A. try to retrieve the first password from the kde wallet system
        if ( !triedWallet )
        {
            QString walletName = KWallet::Wallet::NetworkWallet();
            wallet = KWallet::Wallet::openWallet( walletName );
            if ( wallet )
            {
                // use the KPdf folder (and create if missing)
                if ( !wallet->hasFolder( "KPdf" ) )
                    wallet->createFolder( "KPdf" );
                wallet->setFolder( "KPdf" );

                // look for the pass in that folder
                QString retrievedPass;
                if ( !wallet->readPassword( filePath.section('/', -1, -1), retrievedPass ) )
                    password = retrievedPass.local8Bit();
            }
            triedWallet = true;
        }

        // 1.B. if not retrieved, ask the password using the kde password dialog
        if ( password.isNull() )
        {
            QString prompt;
            if ( firstInput )
                prompt = i18n( "Please insert the password to read the document:" );
            else
                prompt = i18n( "Incorrect password. Try again:" );
            firstInput = false;

            // if the user presses cancel, abort opening
            if ( KPasswordDialog::getPassword( password, prompt, wallet ? &keep : 0 ) != KPasswordDialog::Accepted )
                break;
        }

        // 2. reopen the document using the password (poppler takes UTF-8)
        QString pass = QString::fromLocal8Bit( password.data() );
        m_doc = poppler_document_new_from_file( uri, pass.utf8().data(), &error );
        m_usedPassword = true;

        // 3. if the password is correct and the user chose to remember it, store it to the wallet
        if ( m_doc && wallet && /*safety check*/ wallet->isOpen() && keep > 0 )
            wallet->writePassword( filePath.section('/', -1, -1), pass );
    }
    g_free( uri );
    if ( error )
    {
        kdDebug() << "PopplerGenerator: " << error->message << endl;
        g_error_free( error );
    }
    if ( !m_doc )
        return false;

    // build Pages: sizes as shown, with the page's own rotation applied
    uint pageCount = poppler_document_get_n_pages( m_doc );
    m_pages.resize( pageCount );
    for ( uint i = 0; i < pageCount; i++ )
        m_pages[i] = 0;
    pagesVector.resize( pageCount );
    for ( uint i = 0; i < pageCount ; i++ )
    {
        double w = 0, h = 0;
        PopplerPage * pp = popplerPage( i );
        if ( pp )
            poppler_page_get_size( pp, &w, &h );
        KPDFPage * page = new KPDFPage( i, w, h, 0 );
        addTransition( i, page );
        pagesVector[i] = page;
    }

    // the file has been loaded correctly
    return true;
}


const DocumentInfo * PopplerGenerator::generateDocumentInfo()
{
    if ( docInfoDirty )
    {
        docLock.lock();
        docInfo.set( "title", orUnknown( infoString( "title" ) ), i18n("Title") );
        docInfo.set( "subject", orUnknown( infoString( "subject" ) ), i18n("Subject") );
        docInfo.set( "author", orUnknown( infoString( "author" ) ), i18n("Author") );
        docInfo.set( "keywords", orUnknown( infoString( "keywords" ) ), i18n("Keywords") );
        docInfo.set( "creator", orUnknown( infoString( "creator" ) ), i18n("Creator") );
        docInfo.set( "producer", orUnknown( infoString( "producer" ) ), i18n("Producer") );
        docInfo.set( "creationDate", infoDate( "creation-datetime" ), i18n("Created") );
        docInfo.set( "modificationDate", infoDate( "mod-datetime" ), i18n("Modified") );
        docInfo.set( "mimeType", "application/pdf" );
        if ( m_doc )
        {
            guint major = 0, minor = 0;
            poppler_document_get_pdf_version( m_doc, &major, &minor );
            docInfo.set( "format", i18n( "PDF v. <version>", "PDF v. %1" )
                         .arg( QString( "%1.%2" ).arg( major ).arg( minor ) ), i18n( "Format" ) );
            // poppler-glib does not tell whether a document without a
            // user password is encrypted; restrictions mean it is
            bool encrypted = m_usedPassword ||
                ( poppler_document_get_permissions( m_doc ) & POPPLER_PERMISSIONS_FULL ) != POPPLER_PERMISSIONS_FULL;
            docInfo.set( "encryption", encrypted ? i18n( "Encrypted" ) : i18n( "Unencrypted" ),
                         i18n("Security") );
            docInfo.set( "optimization", poppler_document_is_linearized( m_doc ) ? i18n( "Yes" ) : i18n( "No" ),
                         i18n("Optimized") );
            docInfo.set( "pages", QString::number( poppler_document_get_n_pages( m_doc ) ), i18n("Pages") );
        }
        else
        {
            docInfo.set( "format", "PDF", i18n( "Format" ) );
            docInfo.set( "encryption", i18n( "Unknown Encryption" ), i18n( "Security" ) );
            docInfo.set( "optimization", i18n( "Unknown Optimization" ), i18n( "Optimized" ) );
        }
        docLock.unlock();

        // if the document is valid then we cached good info -> don't cache them again
        if ( m_doc )
            docInfoDirty = false;
    }
    return &docInfo;
}

const DocumentSynopsis * PopplerGenerator::generateDocumentSynopsis()
{
    if ( !docSynopsisDirty )
        return &docSyn;

    if ( !m_doc )
        return NULL;

    docLock.lock();
    PopplerIndexIter * iter = poppler_index_iter_new( m_doc );
    if ( !iter )
    {
        docLock.unlock();
        return NULL;
    }
    docSyn = DocumentSynopsis();
    addSynopsisChildren( &docSyn, iter );
    poppler_index_iter_free( iter );
    docLock.unlock();

    docSynopsisDirty = false;
    return &docSyn;
}

bool PopplerGenerator::isAllowed( int permissions )
{
#if !KPDF_FORCE_DRM
    if (kapp->authorize("skip_drm") && !KpdfSettings::obeyDRM()) return true;
#endif
    if ( !m_doc )
        return false;

    int p = poppler_document_get_permissions( m_doc );
    bool b = true;
    if (permissions & KPDFDocument::AllowModify) b = b && (p & POPPLER_PERMISSIONS_OK_TO_MODIFY);
    if (permissions & KPDFDocument::AllowCopy) b = b && (p & POPPLER_PERMISSIONS_OK_TO_COPY);
    if (permissions & KPDFDocument::AllowPrint) b = b && (p & POPPLER_PERMISSIONS_OK_TO_PRINT);
    if (permissions & KPDFDocument::AllowNotes) b = b && (p & POPPLER_PERMISSIONS_OK_TO_ADD_NOTES);
    return b;
}

bool PopplerGenerator::canGeneratePixmap()
{
    return ready;
}

void PopplerGenerator::generatePixmap( PixmapRequest * request )
{
    if ( !ready )
        kdDebug() << "calling generatePixmap() when not in READY state!" << endl;
    ready = false;

    // links and image rects only for the views that use them
    bool genObjectRects = request->id & (PAGEVIEW_ID | PRESENTATION_ID);

    /** asyncronous requests (generation in PopplerRenderThread::run() **/
    if ( request->async )
    {
        renderThread->startGeneration( request, genObjectRects );
        return;
    }

    /** syncronous request: in-place generation **/
    KPDFPage * page = request->page;
    docLock.lock();
    QImage * image = renderPage( page->number(), request->width, request->height );
    QValueList< ObjectRect * > rects;
    if ( genObjectRects )
        rects = objectRects( page->number() );
    docLock.unlock();

    page->setPixmap( request->id, image ? new QPixmap( *image ) : new QPixmap() );
    delete image;
    if ( genObjectRects )
        page->setObjectRects( rects );

    ready = true;
    signalRequestDone( request );
}

void PopplerGenerator::generateSyncTextPage( KPDFPage * page )
{
    docLock.lock();
    KPDFTextPage * tp = textPage( page->number() );
    docLock.unlock();
    page->setSearchPage( tp );
}

bool PopplerGenerator::supportsSearching() const
{
    return true;
}

bool PopplerGenerator::hasFonts() const
{
    return true;
}

void PopplerGenerator::putFontInfo( KListView * list )
{
    list->addColumn(i18n("Name"));
    list->addColumn(i18n("Type"));
    list->addColumn(i18n("Embedded"));
    list->addColumn(i18n("File"));

    // in the order of PopplerFontType
    const QString fontTypeNames[12] = {
        i18n("unknown"),
        i18n("Type 1"),
        i18n("Type 1C"),
        i18n("OT means OpenType", "Type 1C (OT)"),
        i18n("Type 3"),
        i18n("TrueType"),
        i18n("OT means OpenType", "TrueType (OT)"),
        i18n("CID Type 0"),
        i18n("CID Type 0C"),
        i18n("OT means OpenType", "CID Type 0C (OT)"),
        i18n("CID TrueType"),
        i18n("OT means OpenType", "CID TrueType (OT)")
    };

    if ( !m_doc )
        return;

    docLock.lock();
    PopplerFontInfo * info = poppler_font_info_new( m_doc );
    PopplerFontsIter * iter = 0;
    while ( poppler_font_info_scan( info, 20, &iter ) )
    {
        if ( !iter )
            continue;
        do
        {
            QString name = fromUtf8( poppler_fonts_iter_get_full_name( iter ) );
            if ( name.isEmpty() )
                name = i18n("[none]");
            int type = poppler_fonts_iter_get_font_type( iter );
            if ( type < 0 || type > 11 )
                type = 0;
            bool embedded = poppler_fonts_iter_is_embedded( iter );
            QString path = embedded ? QString::null : QFile::decodeName( poppler_fonts_iter_get_file_name( iter ) );
            if ( path.isEmpty() )
                path = i18n("-");
            new KListViewItem( list, name, fontTypeNames[type],
                               embedded ? i18n("Yes") : i18n("No"), path );
        }
        while ( poppler_fonts_iter_next( iter ) );
        poppler_fonts_iter_free( iter );
        iter = 0;
    }
    g_object_unref( info );
    docLock.unlock();
}

bool PopplerGenerator::print( KPrinter& printer )
{
    // PageSize is a CUPS artificially created setting
    QString ps = printer.option("PageSize");
    int paperWidth, paperHeight;
    int marginTop, marginLeft, marginRight, marginBottom;
    marginTop = (int)printer.option("kde-margin-top").toDouble();
    marginLeft = (int)printer.option("kde-margin-left").toDouble();
    marginRight = (int)printer.option("kde-margin-right").toDouble();
    marginBottom = (int)printer.option("kde-margin-bottom").toDouble();
    bool forceRasterize = printer.option("kde-kpdf-forceRaster").toInt();

    if (ps.find(QRegExp("w\\d+h\\d+")) == 0)
    {
        // size not supported by Qt, CUPS gives us the size as wWIDTHhHEIGHT, at least on the printers i tester
        // remove the w
        ps = ps.mid(1);
        int hPos = ps.find("h");
        paperWidth = ps.left(hPos).toInt();
        paperHeight = ps.mid(hPos+1).toInt();
    }
    else
    {
        // size is supported by Qt, we get either the pageSize name or nothing because the CUPS driver
        // does not do any translation, then use KPrinter::pageSize to get the page size
        KPrinter::PageSize qtPageSize;
        if (!ps.isEmpty()) qtPageSize = pageNameToPageSize(ps);
        else qtPageSize = printer.pageSize();

        QPrinter dummy(QPrinter::PrinterResolution);
        dummy.setFullPage(true);
        dummy.setPageSize((QPrinter::PageSize)qtPageSize);

        QPaintDeviceMetrics metrics(&dummy);
        paperWidth = metrics.width();
        paperHeight = metrics.height();
    }
    if ( paperWidth <= 0 || paperHeight <= 0 || !m_doc )
        return false;

    docLock.lock();
    QString pstitle = infoString( "title" );
    docLock.unlock();
    if ( pstitle.isEmpty() )
        pstitle = m_document->currentDocument().fileName( false );
    if ( !pstitle.isEmpty() )
        printer.setDocName( pstitle );

    KTempFile tf( QString::null, ".ps" );
    tf.close();
    cairo_surface_t * surface = cairo_ps_surface_create( QFile::encodeName( tf.name() ),
                                                         paperWidth, paperHeight );
    if ( cairo_surface_status( surface ) != CAIRO_STATUS_SUCCESS )
    {
        cairo_surface_destroy( surface );
        tf.unlink();
        return false;
    }
    if ( !pstitle.isEmpty() )
    {
        // anything other than ASCII is not specified in the %%Title DSC
        // comment and some printers stop printing on it
        QCString t = pstitle.latin1();
        for ( unsigned char * p = (unsigned char *)t.data(); p && *p; ++p )
            if ( *p >= 0x80 || *p < 0x20 )
                *p = '?';
        cairo_ps_surface_dsc_comment( surface, QCString( "%%Title: " ) + t );
    }

    double areaWidth = paperWidth - marginLeft - marginRight,
           areaHeight = paperHeight - marginTop - marginBottom;
    if ( areaWidth <= 0 || areaHeight <= 0 )
    {
        areaWidth = paperWidth;
        areaHeight = paperHeight;
        marginLeft = marginTop = 0;
    }

    // the margins may change the aspect ratio of the paper
    bool keepAspect = true;
    double xScale = areaWidth / (double)paperWidth,
           yScale = areaHeight / (double)paperHeight;
    if ( abs((int)(xScale * 100) - (int)(yScale * 100)) > 5 ) {
        int result = KMessageBox::questionYesNo(m_document->widget(),
                                   i18n("The margins you specified change the page aspect ratio. Do you want to print with the aspect ratio changed or do you want the margins to be adapted so that the aspect ratio is preserved?"),
                                   i18n("Aspect ratio change"),
                                   i18n("Print with specified margins"),
                                   i18n("Print adapting margins to keep aspect ratio"),
                                   "kpdfStrictlyObeyMargins");
        if (result == KMessageBox::Yes) keepAspect = false;
    }

    QValueList<int> pageList;
    int pageCount = m_pages.count();
    if (!printer.previewOnly())
        pageList = printer.pageList();
    else
        for(int i = 1; i <= pageCount; i++) pageList.append(i);

    docLock.lock();
    QValueList<int>::const_iterator pIt = pageList.begin(), pEnd = pageList.end();
    for ( ; pIt != pEnd; ++pIt )
    {
        if ( *pIt < 1 || *pIt > pageCount )
            continue;
        PopplerPage * page = popplerPage( *pIt - 1 );
        if ( !page )
            continue;
        double w, h;
        poppler_page_get_size( page, &w, &h );
        if ( w <= 0 || h <= 0 )
            continue;

        // turn the page when its orientation is not the paper's
        bool turn = ( w > h ) != ( areaWidth > areaHeight );
        double pw = turn ? h : w, ph = turn ? w : h;
        double sx = areaWidth / pw, sy = areaHeight / ph;
        if ( keepAspect )
            sx = sy = QMIN( sx, sy );

        cairo_t * cr = cairo_create( surface );
        cairo_translate( cr, marginLeft + ( areaWidth - pw * sx ) / 2,
                             marginTop + ( areaHeight - ph * sy ) / 2 );
        cairo_scale( cr, sx, sy );
        if ( turn )
        {
            cairo_translate( cr, h, 0 );
            cairo_rotate( cr, M_PI / 2 );
        }
        if ( forceRasterize )
        {
            // an image at 300 dpi of the printed size
            double dpiScale = 300.0 / 72.0 * QMAX( sx, sy );
            int iw = (int)ceil( w * dpiScale ), ih = (int)ceil( h * dpiScale );
            cairo_surface_t * img = cairo_image_surface_create( CAIRO_FORMAT_RGB24, iw, ih );
            cairo_t * icr = cairo_create( img );
            cairo_set_source_rgb( icr, 1, 1, 1 );
            cairo_paint( icr );
            cairo_scale( icr, iw / w, ih / h );
            poppler_page_render_for_printing( page, icr );
            cairo_destroy( icr );
            cairo_scale( cr, w / iw, h / ih );
            cairo_set_source_surface( cr, img, 0, 0 );
            cairo_paint( cr );
            cairo_surface_destroy( img );
        }
        else
            poppler_page_render_for_printing( page, cr );
        cairo_show_page( cr );
        cairo_destroy( cr );
    }
    docLock.unlock();

    cairo_surface_finish( surface );
    bool ok = cairo_surface_status( surface ) == CAIRO_STATUS_SUCCESS;
    cairo_surface_destroy( surface );
    if ( !ok )
    {
        tf.unlink();
        return false;
    }
    printer.printFiles( tf.name(), true );
    return true;
}

QString PopplerGenerator::getMetaData( const QString & key, const QString & option )
{
    if ( !m_doc )
        return QString();

    if ( key == "StartFullScreen" )
    {
        // asking for the 'start in fullscreen mode' (pdf property)
        if ( poppler_document_get_page_mode( m_doc ) == POPPLER_PAGE_MODE_FULL_SCREEN )
            return "yes";
    }
    else if ( key == "NamedViewport" && !option.isEmpty() )
    {
        // asking for the page related to a 'named link destination'. the
        // option is the link name. @see addSynopsisChildren.
        DocumentViewport viewport;
        docLock.lock();
        PopplerDest * dest = poppler_document_find_dest( m_doc, option.latin1() );
        if ( dest )
        {
            viewport = viewportFromDest( dest );
            poppler_dest_free( dest );
        }
        docLock.unlock();
        if ( viewport.pageNumber >= 0 )
            return viewport.toString();
    }
    else if ( key == "OpenTOC" )
    {
        if ( poppler_document_get_page_mode( m_doc ) == POPPLER_PAGE_MODE_USE_OUTLINES )
            return "yes";
    }
    return QString();
}

bool PopplerGenerator::reparseConfig()
{
    // load paper color from Settings or use the white default color
    QColor color = ( (KpdfSettings::renderMode() == KpdfSettings::EnumRenderMode::Paper ) &&
                     KpdfSettings::changeColors() ) ? KpdfSettings::paperColor() : Qt::white;
    // if paper color is changed we have to rebuild every visible pixmap.
    // other effects are just recoloring over the page rendered on
    // 'standard' white background.
    if ( color != paperColor )
    {
        docLock.lock();
        paperColor = color;
        docLock.unlock();
        return true;
    }
    return false;
}
//END Generator inherited functions

PopplerPage * PopplerGenerator::popplerPage( int pageNumber )
// note: MUTEX is LOCKED while calling this (or the document is loading)
{
    if ( !m_doc || pageNumber < 0 || pageNumber >= (int)m_pages.count() )
        return 0;
    if ( !m_pages[pageNumber] )
        m_pages[pageNumber] = poppler_document_get_page( m_doc, pageNumber );
    return m_pages[pageNumber];
}

QImage * PopplerGenerator::renderPage( int pageNumber, int width, int height )
// note: MUTEX is LOCKED while calling this
{
    PopplerPage * page = popplerPage( pageNumber );
    if ( !page || width <= 0 || height <= 0 )
        return 0;

    // cairo draws into the QImage: a 32 bit QImage is one block of rows of
    // 0xAARRGGBB words in the native byte order, as CAIRO_FORMAT_RGB24
    QImage * image = new QImage( width, height, 32 );
    if ( image->isNull() || image->bytesPerLine() != cairo_format_stride_for_width( CAIRO_FORMAT_RGB24, width ) )
    {
        delete image;
        return 0;
    }
    image->fill( paperColor.rgb() );

    cairo_surface_t * surface = cairo_image_surface_create_for_data( image->bits(),
            CAIRO_FORMAT_RGB24, width, height, image->bytesPerLine() );
    cairo_t * cr = cairo_create( surface );
    double w, h;
    poppler_page_get_size( page, &w, &h );
    if ( w > 0 && h > 0 )
        cairo_scale( cr, width / w, height / h );
    poppler_page_render( page, cr );
    cairo_destroy( cr );
    cairo_surface_flush( surface );
    cairo_surface_destroy( surface );
    return image;
}

QValueList< ObjectRect * > PopplerGenerator::objectRects( int pageNumber )
// note: MUTEX is LOCKED while calling this
{
    QValueList< ObjectRect * > rects;
    PopplerPage * page = popplerPage( pageNumber );
    if ( !page )
        return rects;
    double w, h;
    poppler_page_get_size( page, &w, &h );
    if ( w <= 0 || h <= 0 )
        return rects;

    // the areas have their origin at the bottom left corner of the page
    GList * links = poppler_page_get_link_mapping( page );
    for ( GList * l = links; l; l = l->next )
    {
        PopplerLinkMapping * m = (PopplerLinkMapping *)l->data;
        KPDFLink * link = makeLink( m->action );
        if ( !link )
            continue;
        rects.push_front( new ObjectRect( m->area.x1 / w, ( h - m->area.y2 ) / h,
                                          m->area.x2 / w, ( h - m->area.y1 ) / h,
                                          ObjectRect::Link, link ) );
    }
    poppler_page_free_link_mapping( links );

    // images, only to show them (Accessibility setting)
    if ( KpdfSettings::highlightImages() )
    {
        GList * images = poppler_page_get_image_mapping( page );
        for ( GList * l = images; l; l = l->next )
        {
            PopplerImageMapping * m = (PopplerImageMapping *)l->data;
            // as before: no frame around tiny images
            if ( fabs( m->area.x2 - m->area.x1 ) < w / 50 || fabs( m->area.y2 - m->area.y1 ) < h / 50 )
                continue;
            rects.push_back( new ObjectRect( m->area.x1 / w, ( h - m->area.y2 ) / h,
                                             m->area.x2 / w, ( h - m->area.y1 ) / h,
                                             ObjectRect::Image, 0 ) );
        }
        poppler_page_free_image_mapping( images );
    }
    return rects;
}

KPDFTextPage * PopplerGenerator::textPage( int pageNumber )
// note: MUTEX is LOCKED while calling this
{
    PopplerPage * page = popplerPage( pageNumber );
    double w = 0, h = 0;
    if ( page )
        poppler_page_get_size( page, &w, &h );
    if ( !page || w <= 0 || h <= 0 )
        return new KPDFTextPage( QString::null, QMemArray<float>() );

    // one box per character of the text, in the orientation shown
    char * utf8 = poppler_page_get_text( page );
    PopplerRectangle * boxes = 0;
    guint nBoxes = 0;
    if ( !poppler_page_get_text_layout( page, &boxes, &nBoxes ) )
        nBoxes = 0;

    QString text;
    QMemArray<float> rects;
    if ( utf8 && g_utf8_validate( utf8, -1, 0 ) )
    {
        glong nChars = g_utf8_strlen( utf8, -1 );
        text.reserve( nChars );
        rects.resize( 4 * nChars + 8 );
        uint n = 0;
        guint box = 0;
        for ( const char * p = utf8; *p; p = g_utf8_next_char( p ), ++box )
        {
            gunichar c = g_utf8_get_char( p );
            float l = 0, t = 0, r = 0, b = 0;
            if ( box < nBoxes )
            {
                l = boxes[box].x1 / w;
                t = boxes[box].y1 / h;
                r = boxes[box].x2 / w;
                b = boxes[box].y2 / h;
            }
            // characters outside the BMP take two QChars, with the same box
            int units = c > 0xffff ? 2 : 1;
            if ( 4 * ( n + units ) > rects.size() )
                rects.resize( 2 * rects.size() + 8 );
            if ( units == 2 )
            {
                c -= 0x10000;
                text += QChar( (ushort)( 0xd800 + ( c >> 10 ) ) );
                text += QChar( (ushort)( 0xdc00 + ( c & 0x3ff ) ) );
            }
            else
                text += QChar( (ushort)c );
            for ( int u = 0; u < units; ++u, ++n )
            {
                rects[4*n] = l;
                rects[4*n+1] = t;
                rects[4*n+2] = r;
                rects[4*n+3] = b;
            }
        }
        rects.resize( 4 * n );
    }
    g_free( boxes );
    g_free( utf8 );
    return new KPDFTextPage( text, rects );
}

KPDFLink * PopplerGenerator::makeLink( PopplerAction * a )
// note: MUTEX is LOCKED while calling this
{
    KPDFLink * link = NULL;
    if ( a ) switch ( a->type )
    {
        case POPPLER_ACTION_GOTO_DEST:
            link = new KPDFLinkGoto( QString::null, viewportFromDest( a->goto_dest.dest ) );
            break;

        case POPPLER_ACTION_GOTO_REMOTE:
            {
            // a named destination of the other file can not be resolved here
            DocumentViewport vp;
            PopplerDest * d = a->goto_remote.dest;
            if ( d && d->type != POPPLER_DEST_NAMED && d->page_num > 0 )
                vp.pageNumber = d->page_num - 1;
            link = new KPDFLinkGoto( QFile::decodeName( a->goto_remote.file_name ), vp );
            }
            break;

        case POPPLER_ACTION_LAUNCH:
            link = new KPDFLinkExecute( QFile::decodeName( a->launch.file_name ),
                                        fromUtf8( a->launch.params ) );
            break;

        case POPPLER_ACTION_NAMED:
            {
            QString name = QString::fromLatin1( a->named.named_dest );
            if ( name == "NextPage" )
                link = new KPDFLinkAction( KPDFLinkAction::PageNext );
            else if ( name == "PrevPage" )
                link = new KPDFLinkAction( KPDFLinkAction::PagePrev );
            else if ( name == "FirstPage" )
                link = new KPDFLinkAction( KPDFLinkAction::PageFirst );
            else if ( name == "LastPage" )
                link = new KPDFLinkAction( KPDFLinkAction::PageLast );
            else if ( name == "GoBack" )
                link = new KPDFLinkAction( KPDFLinkAction::HistoryBack );
            else if ( name == "GoForward" )
                link = new KPDFLinkAction( KPDFLinkAction::HistoryForward );
            else if ( name == "Quit" )
                link = new KPDFLinkAction( KPDFLinkAction::Quit );
            else if ( name == "GoToPage" )
                link = new KPDFLinkAction( KPDFLinkAction::GoToPage );
            else if ( name == "Find" )
                link = new KPDFLinkAction( KPDFLinkAction::Find );
            else if ( name == "Close" )
                link = new KPDFLinkAction( KPDFLinkAction::Close );
            else
                kdDebug() << "Unknown named action: '" << name << "'" << endl;
            }
            break;

        case POPPLER_ACTION_URI:
            if ( a->uri.uri )
                link = new KPDFLinkBrowse( fromUtf8( a->uri.uri ) );
            break;

        default:
            // movies, renditions, layers, JavaScript, forms: not handled
            break;
    }

    // link may be zero at that point
    return link;
}

DocumentViewport PopplerGenerator::viewportFromDest( PopplerDest * dest )
// note: MUTEX is LOCKED while calling this
{
    DocumentViewport vp( -1 );
    if ( !dest )
        return vp;

    if ( dest->type == POPPLER_DEST_NAMED )
    {
        PopplerDest * found = dest->named_dest ? poppler_document_find_dest( m_doc, dest->named_dest ) : 0;
        if ( found && found->type != POPPLER_DEST_NAMED )
            vp = viewportFromDest( found );
        if ( found )
            poppler_dest_free( found );
        return vp;
    }

    vp.pageNumber = dest->page_num - 1;
    PopplerPage * page = popplerPage( vp.pageNumber );
    if ( !page )
    {
        vp.pageNumber = -1;
        return vp;
    }
    double w, h;
    poppler_page_get_size( page, &w, &h );
    if ( w <= 0 || h <= 0 )
        return vp;

    // the position, in PDF coordinates (origin at the bottom left corner)
    bool useLeft = false, useTop = false;
    switch ( dest->type )
    {
        case POPPLER_DEST_XYZ:
            useLeft = dest->change_left;
            useTop = dest->change_top;
            break;
        case POPPLER_DEST_FITH:
        case POPPLER_DEST_FITBH:
            useTop = dest->change_top;
            break;
        case POPPLER_DEST_FITR:
            useLeft = useTop = true;
            break;
        default:
            break;
    }
    if ( useLeft || useTop )
    {
        double x = useLeft ? dest->left / w : 0.0,
               y = useTop ? ( h - dest->top ) / h : 0.0;
        vp.rePos.normalizedX = QMAX( 0.0, QMIN( 1.0, x ) );
        vp.rePos.normalizedY = QMAX( 0.0, QMIN( 1.0, y ) );
        vp.rePos.enabled = true;
        vp.rePos.pos = DocumentViewport::TopLeft;
    }
    return vp;
}

void PopplerGenerator::addSynopsisChildren( QDomNode * parent, PopplerIndexIter * iter )
// note: MUTEX is LOCKED while calling this
{
    do
    {
        PopplerAction * a = poppler_index_iter_get_action( iter );
        if ( !a )
            continue;

        // 1. create element using the item's title as tagName
        QString name = fromUtf8( a->any.title );
        if ( name.isEmpty() )
        {
            poppler_action_free( a );
            continue;
        }
        QDomElement item = docSyn.createElement( name );
        parent->appendChild( item );

        // 2. find the page the item refers to
        if ( a->type == POPPLER_ACTION_GOTO_DEST && a->goto_dest.dest )
        {
            PopplerDest * dest = a->goto_dest.dest;
            if ( dest->type == POPPLER_DEST_NAMED )
                // resolving every named destination now is slow: store the
                // name, the viewport is asked for as metadata on demand
                item.setAttribute( "ViewportName", QString::fromLatin1( dest->named_dest ) );
            else
            {
                DocumentViewport vp = viewportFromDest( dest );
                if ( vp.pageNumber >= 0 )
                    item.setAttribute( "Viewport", vp.toString() );
            }
        }
        else if ( a->type == POPPLER_ACTION_GOTO_REMOTE )
        {
            PopplerDest * dest = a->goto_remote.dest;
            if ( dest && dest->type != POPPLER_DEST_NAMED && dest->page_num > 0 )
                item.setAttribute( "Viewport", DocumentViewport( dest->page_num - 1 ).toString() );
            item.setAttribute( "ExternalFileName", QFile::decodeName( a->goto_remote.file_name ) );
        }
        poppler_action_free( a );

        item.setAttribute( "Open", QVariant( (bool)poppler_index_iter_is_open( iter ) ).toString() );

        // 3. recursively descend over children
        PopplerIndexIter * child = poppler_index_iter_get_child( iter );
        if ( child )
        {
            addSynopsisChildren( &item, child );
            poppler_index_iter_free( child );
        }
    }
    while ( poppler_index_iter_next( iter ) );
}

void PopplerGenerator::addTransition( int pageNumber, KPDFPage * page )
// note: called while loading
{
    PopplerPage * pp = popplerPage( pageNumber );
    if ( !pp )
        return;

    PopplerPageTransition * pt = poppler_page_get_transition( pp );
    if ( !pt )
        return;
    if ( pt->type == POPPLER_PAGE_TRANSITION_REPLACE )
    {
        poppler_page_transition_free( pt );
        return;
    }

    KPDFPageTransition *transition = new KPDFPageTransition();
    switch ( pt->type ) {
        case POPPLER_PAGE_TRANSITION_REPLACE:
            // won't get here, added to avoid warning
            break;
        case POPPLER_PAGE_TRANSITION_SPLIT:
            transition->setType( KPDFPageTransition::Split );
            break;
        case POPPLER_PAGE_TRANSITION_BLINDS:
            transition->setType( KPDFPageTransition::Blinds );
            break;
        case POPPLER_PAGE_TRANSITION_BOX:
            transition->setType( KPDFPageTransition::Box );
            break;
        case POPPLER_PAGE_TRANSITION_WIPE:
            transition->setType( KPDFPageTransition::Wipe );
            break;
        case POPPLER_PAGE_TRANSITION_DISSOLVE:
            transition->setType( KPDFPageTransition::Dissolve );
            break;
        case POPPLER_PAGE_TRANSITION_GLITTER:
            transition->setType( KPDFPageTransition::Glitter );
            break;
        case POPPLER_PAGE_TRANSITION_FLY:
            transition->setType( KPDFPageTransition::Fly );
            break;
        case POPPLER_PAGE_TRANSITION_PUSH:
            transition->setType( KPDFPageTransition::Push );
            break;
        case POPPLER_PAGE_TRANSITION_COVER:
            transition->setType( KPDFPageTransition::Cover );
            break;
        case POPPLER_PAGE_TRANSITION_UNCOVER:
            transition->setType( KPDFPageTransition::Uncover );
            break;
        case POPPLER_PAGE_TRANSITION_FADE:
            transition->setType( KPDFPageTransition::Fade );
            break;
    }

    transition->setDuration( pt->duration );

    switch ( pt->alignment ) {
        case POPPLER_PAGE_TRANSITION_HORIZONTAL:
            transition->setAlignment( KPDFPageTransition::Horizontal );
            break;
        case POPPLER_PAGE_TRANSITION_VERTICAL:
            transition->setAlignment( KPDFPageTransition::Vertical );
            break;
    }

    switch ( pt->direction ) {
        case POPPLER_PAGE_TRANSITION_INWARD:
            transition->setDirection( KPDFPageTransition::Inward );
            break;
        case POPPLER_PAGE_TRANSITION_OUTWARD:
            transition->setDirection( KPDFPageTransition::Outward );
            break;
    }

    transition->setAngle( pt->angle );
    transition->setScale( pt->scale );
    transition->setIsRectangular( pt->rectangular );
    poppler_page_transition_free( pt );

    page->setTransition( transition );
}

QString PopplerGenerator::infoString( const char * property ) const
// note: MUTEX is LOCKED while calling this
{
    if ( !m_doc )
        return QString::null;
    gchar * value = 0;
    g_object_get( m_doc, property, &value, NULL );
    QString result = fromUtf8( value );
    g_free( value );
    return result;
}

QString PopplerGenerator::infoDate( const char * property ) const
// note: MUTEX is LOCKED while calling this
{
    if ( !m_doc )
        return i18n( "Unknown Date" );
    GDateTime * value = 0;
    g_object_get( m_doc, property, &value, NULL );
    if ( !value )
        return i18n( "Unknown Date" );
    QDateTime dt;
    dt.setTime_t( (uint)g_date_time_to_unix( value ) );
    g_date_time_unref( value );
    return KGlobal::locale()->formatDateTime( dt, false, true );
}

void PopplerGenerator::customEvent( QCustomEvent * event )
{
    // catch generator 'ready events' only
    if ( event->type() != TGE_DATAREADY_ID )
        return;

    // 1. the mutex must be unlocked now
    if ( docLock.locked() )
    {
        kdWarning() << "PopplerGenerator: 'data available' but mutex still "
                    << "held. Recovering." << endl;
        // syncronize GUI thread (must not happen)
        docLock.lock();
        docLock.unlock();
    }

    // 2. put thread's generated data into the KPDFPage
    PixmapRequest * request = static_cast< PixmapRequest * >( event->data() );
    QImage * outImage = renderThread->takeImage();
    QValueList< ObjectRect * > outRects = renderThread->takeObjectRects();

    request->page->setPixmap( request->id, outImage ? new QPixmap( *outImage ) : new QPixmap() );
    delete outImage;
    if ( !outRects.isEmpty() )
        request->page->setObjectRects( outRects );

    // 3. tell the thread that data has been taken
    renderThread->endGeneration();

    ready = true;
    signalRequestDone( request );
}



/** The Poppler Render Thread **/

PopplerRenderThread::PopplerRenderThread( PopplerGenerator * gen )
    : m_generator( gen ), m_request( 0 ), m_genObjectRects( false ), m_image( 0 )
{
}

PopplerRenderThread::~PopplerRenderThread()
{
    // delete what the gui thread did not take
    delete m_image;
    QValueList< ObjectRect * >::iterator it = m_rects.begin(), end = m_rects.end();
    for ( ; it != end; ++it )
        delete *it;
    delete m_request;
}

void PopplerRenderThread::startGeneration( PixmapRequest * request, bool genObjectRects )
{
    if ( m_request )
    {
        kdDebug() << "PopplerRenderThread: requesting a pixmap "
                  << "when another is being generated." << endl;
        delete request;
        return;
    }
    m_request = request;
    m_genObjectRects = genObjectRects;
    start( QThread::InheritPriority );
}

void PopplerRenderThread::endGeneration()
{
    m_request = 0;
}

QImage * PopplerRenderThread::takeImage()
{
    QImage * img = m_image;
    m_image = 0;
    return img;
}

QValueList< ObjectRect * > PopplerRenderThread::takeObjectRects()
{
    QValueList< ObjectRect * > rects = m_rects;
    m_rects.clear();
    return rects;
}

void PopplerRenderThread::run()
{
    int pageNumber = m_request->page->number();

    m_generator->docLock.lock();
    m_image = m_generator->renderPage( pageNumber, m_request->width, m_request->height );
    if ( m_genObjectRects )
        m_rects = m_generator->objectRects( pageNumber );
    m_generator->docLock.unlock();

    // notify the GUI thread that data is pending and can be read
    QCustomEvent * readyEvent = new QCustomEvent( TGE_DATAREADY_ID );
    readyEvent->setData( m_request );
    QApplication::postEvent( m_generator, readyEvent );
}
