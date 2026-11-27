/*
    kwhatsapp: WhatsApp Web as a KDE application (WPE WebKit)

    Copyright (C) 2026 The kde3-revival authors

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.
*/

#include <stdlib.h>

#include <kaboutdata.h>
#include <kcmdlineargs.h>
#include <kconfig.h>
#include <klocale.h>
#include <kmessagebox.h>
#include <kstartupinfo.h>
#include <kuniqueapplication.h>

#include "kwhatsapp.h"

static const char description[] = I18N_NOOP( "WhatsApp Web as a KDE application" );

static KCmdLineOptions options[] =
{
    { "hidden", I18N_NOOP( "Start in the system tray, without the window" ), 0 },
    { "url <url>", I18N_NOOP( "Open this page instead of WhatsApp Web (for tests)" ), "https://web.whatsapp.com/" },
    KCmdLineLastOption
};

// One process: starting kwhatsapp again shows its window.
class KWhatsAppApplication : public KUniqueApplication
{
public:
    KWhatsAppApplication() : m_window( 0 ) {}

    virtual int newInstance()
    {
        KCmdLineArgs* args = KCmdLineArgs::parsedArgs();
        if ( !m_window ) {
            m_window = new KWhatsApp( KURL( args->getOption( "url" ) ) );
            if ( !m_window->isValid() ) {
                KMessageBox::error( 0, i18n( "The WPE WebKit part (kde3-wpepart) with its app mode is not "
                                             "installed: KWhatsApp cannot show WhatsApp Web." ) );
                delete m_window;
                m_window = 0;
                ::exit( 1 );
            }
            setMainWidget( m_window );
            // Started at login from the first start on, until turned off.
            KWhatsApp::initStartAtLogin();
            // A session restored as it was left: hidden if it was.
            bool hidden = args->isSet( "hidden" );
            if ( isRestored() ) {
                KConfigGroup window( config(), "MainWindow" );
                hidden = window.readBoolEntry( "HiddenAtLogout", false );
            }
            if ( !hidden )
                m_window->show();
            args->clear();
            return 0;
        }
        // Started again (the K menu, a second launch): the window comes;
        // not for --hidden (the autostart entry, after a session restore
        // brought KWhatsApp back): the window stays as it is.
        if ( !args->isSet( "hidden" ) )
            m_window->showOnCurrentDesktop();
        KStartupInfo::appStarted();
        args->clear();
        return 0;
    }

    virtual void commitData( QSessionManager& sm )
    {
        if ( m_window ) {
            KConfigGroup window( config(), "MainWindow" );
            window.writeEntry( "HiddenAtLogout", !m_window->isVisible() );
            window.sync();
        }
        KUniqueApplication::commitData( sm );
    }

private:
    KWhatsApp* m_window;
};

int main( int argc, char** argv )
{
    KAboutData about( "kwhatsapp", I18N_NOOP( "KWhatsApp" ), "0.1", description, KAboutData::License_GPL,
                      "(c) 2026 The kde3-revival authors" );
    about.addCredit( "WhatsApp", I18N_NOOP( "KWhatsApp shows WhatsApp Web, WhatsApp's own web client; "
                                            "it is not made or endorsed by WhatsApp" ), 0, "https://web.whatsapp.com/" );
    KCmdLineArgs::init( argc, argv, &about );
    KCmdLineArgs::addCmdLineOptions( options );
    KUniqueApplication::addCmdLineOptions();
    if ( !KUniqueApplication::start() )
        return 0;   // the running kwhatsapp shows its window
    KWhatsAppApplication app;
    return app.exec();
}
