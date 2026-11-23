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

#ifndef _KPDF_GENERATOR_POPPLER_H_
#define _KPDF_GENERATOR_POPPLER_H_

#include <qmutex.h>
#include <qcolor.h>
#include <qstring.h>
#include <qthread.h>
#include <qvaluevector.h>
#include "core/generator.h"
#include "core/document.h"
#include "core/link.h"

// poppler-glib types, without its headers (they pull in GLib)
typedef struct _PopplerDocument PopplerDocument;
typedef struct _PopplerPage PopplerPage;
typedef struct _PopplerDest PopplerDest;
typedef struct _PopplerIndexIter PopplerIndexIter;
typedef union _PopplerAction PopplerAction;

class QImage;
class QDomNode;
class ObjectRect;
class KPDFTextPage;
class PopplerRenderThread;

/**
 * @short A generator that builds contents from a PDF document with
 * poppler-glib.
 *
 * Pages are rendered by poppler through cairo straight into a QImage;
 * links, the outline, the text with its character boxes, fonts and
 * transitions come from poppler-glib as well.  It replaces the generator
 * on the xpdf code KPDF carried in its source tree.
 *
 * poppler is not thread safe on one document, so every use of the
 * document goes through 'docLock'.  Asynchronous requests are rendered in
 * PopplerRenderThread, which hands the result to the GUI thread with a
 * custom event.
 */
class PopplerGenerator : public Generator
{
    public:
        PopplerGenerator( KPDFDocument * document );
        virtual ~PopplerGenerator();

        // [INHERITED] load a document and fill up the pagesVector
        bool loadDocument( const QString & fileName, QValueVector<KPDFPage*> & pagesVector );

        // [INHERITED] document informations
        const DocumentInfo * generateDocumentInfo();
        const DocumentSynopsis * generateDocumentSynopsis();

        // [INHERITED] document permissions
        bool isAllowed( int permissions );

        // [INHERITED] perform actions on document / pages
        bool canGeneratePixmap();
        void generatePixmap( PixmapRequest * request );
        void generateSyncTextPage( KPDFPage * page );

        // [INHERITED] capability querying
        bool supportsSearching() const;
        bool hasFonts() const;

        // [INHERITED] font related
        void putFontInfo( KListView * list );

        // [INHERITED] print page using an already configured kprinter
        bool print( KPrinter & printer );

        // [INHERITED] reply to some metadata requests
        QString getMetaData( const QString & key, const QString & option );

        // [INHERITED] reparse configuration
        bool reparseConfig();

    private:
        friend class PopplerRenderThread;

        // all of these need 'docLock' held
        QImage * renderPage( int pageNumber, int width, int height );
        QValueList< ObjectRect * > objectRects( int pageNumber );
        KPDFTextPage * textPage( int pageNumber );
        KPDFLink * makeLink( PopplerAction * action );
        DocumentViewport viewportFromDest( PopplerDest * dest );
        void addSynopsisChildren( QDomNode * parent, PopplerIndexIter * iter );
        void addTransition( int pageNumber, KPDFPage * page );
        QString infoString( const char * property ) const;
        QString infoDate( const char * property ) const;

        // (async related) receive data from the render thread
        void customEvent( QCustomEvent * );

        QMutex docLock;
        PopplerDocument * m_doc;
        int m_pageCount;
        QColor paperColor;
        bool m_usedPassword;

        PopplerRenderThread * renderThread;

        bool ready;
        bool docInfoDirty;
        DocumentInfo docInfo;
        bool docSynopsisDirty;
        DocumentSynopsis docSyn;
};


/**
 * @short Renders one page for PopplerGenerator in the background.
 */
class PopplerRenderThread : public QThread
{
    public:
        PopplerRenderThread( PopplerGenerator * generator );
        ~PopplerRenderThread();

        // render the request in the thread (it is handed back with the event)
        void startGeneration( PixmapRequest * request, bool genObjectRects );
        // the GUI thread took the results
        void endGeneration();

        // results, for the GUI thread
        QImage * takeImage();
        QValueList< ObjectRect * > takeObjectRects();

    private:
        void run();

        PopplerGenerator * m_generator;
        PixmapRequest * m_request;
        bool m_genObjectRects;
        QImage * m_image;
        QValueList< ObjectRect * > m_rects;
};

#endif
