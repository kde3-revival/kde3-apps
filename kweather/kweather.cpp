/***************************************************************************
                          kweather.cpp  -  description
                             -------------------
    begin                : Wed Jul  5 23:09:02 CDT 2000
    copyright            : (C) 2000-2003 by Ian Reinhart Geiser
                         : (C) 2002-2003 Nadeem Hasan <nhasan@kde.org>
                         : (C) 2026 The KDE3 revival authors (Open-Meteo)
    email                : geiseri@msoe.edu
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include <kaboutapplication.h>
#include <kapplication.h>
#include <kcmultidialog.h>
#include <kdebug.h>
#include <klocale.h>
#include <kiconloader.h>
#include <kmessagebox.h>
#include <kpopupmenu.h>
#include <kconfig.h>
#include <kjanuswidget.h>
#include <kinstance.h>
#include <knotifyclient.h>
#include <ksettings/dialog.h>
#include <ksettings/dispatcher.h>
#include <dcopclient.h>

#include <qfile.h>
#include <qtimer.h>
#include <qpalette.h>

#include "kweather.h"
#include "forecastpopup.h"
#include "dockwidget.h"
#include "weatherservice_stub.h"

extern "C"
{
    KDE_EXPORT KPanelApplet* init(QWidget *parent, const QString& configFile)
    {
        KGlobal::locale()->insertCatalogue("kweather");
        kweather *theApplet = new kweather(configFile, KPanelApplet::Normal,
                0, parent, "kweather");
        return theApplet;
    }
}

kweather::kweather(const QString& configFile, Type t, int actions,
        QWidget *parent, const char *name):
        KPanelApplet(configFile, t, actions, parent, name), weatherIface(),
        mRainAlert( false ), mRainExpected( false ),
        mFirstRun( false ), mPopup( 0 ), mClient( 0 ),
        mContextMenu( 0 ), mWeatherService( 0 ), settingsDialog( 0 )
{
    kdDebug(12004) << "Constructor " << endl;
    setObjId("weatherIface");
    
    setBackgroundOrigin( QWidget::AncestorOrigin );
    loadPrefs();
    initContextMenu();
    initDCOP();
    
    dockWidget = new dockwidget(reportLocation, this, "dockwidget");
    connect(dockWidget, SIGNAL(buttonClicked()), SLOT(doReport()));
    dockWidget->setViewMode(mViewMode);
    setLabelColor();
    
    timeOut = new QTimer(this, "timeOut" );
    connect(timeOut, SIGNAL(timeout()), SLOT(timeout()));
    timeOut->start(10*60*1000);
    
    if(mFirstRun)
        preferences();
    else
        timeout();
}

kweather::~kweather()
{
    delete mWeatherService;
}

void kweather::initContextMenu()
{
    mContextMenu = new KPopupMenu(this);
    mContextMenu->insertTitle(i18n("KWeather - %1").arg( mLocationName.isEmpty()
        ? reportLocation : mLocationName ), -1, 0);
    mContextMenu->insertItem(SmallIcon("viewmag"), i18n("Show &Forecast"),
        this, SLOT(doReport()), 0, -1, 1);
    mContextMenu->insertItem(SmallIcon("reload"), i18n("&Update Now"),
        this, SLOT(slotUpdateNow()), 0, -1, 2);
    mContextMenu->insertSeparator();
    mContextMenu->insertItem(SmallIcon("kweather"), i18n("&About KWeather"), this, SLOT(about()));
    mContextMenu->insertItem(SmallIcon("configure"),
        i18n("&Configure KWeather..."), this, SLOT(preferences()));
    setCustomMenu(mContextMenu);
}

void kweather::initDCOP()
{
    if ( !mClient )
        mClient = KApplication::dcopClient();
    
    if(!mClient->isAttached())
        mClient->attach();
    
    if(!attach())
        return;
    
    kdDebug(12004) << "attached to the server..." << endl;
    
    if ( mWeatherService )
        delete mWeatherService;
    
    mWeatherService = new WeatherService_stub( "KWeatherService", "WeatherService" );

    // the service keeps the names of the places; it may have lost them
    if ( !reportLocation.isEmpty() && !mLocationName.isEmpty() )
        mWeatherService->addLocation( reportLocation, mLocationName, mLocationCountry );
    
    if (!connectDCOPSignal(0, 0, "fileUpdate(QString)",
        "refresh(QString)",false))
        kdDebug(12004) << "Could not attach dcop signal..." << endl;
    else
        kdDebug(12004) << "attached dcop signals..." << endl;
}

/** about box */
void kweather::about(){
    kdDebug(12004) << "Telling about" << endl;
    KAboutData aboutData("KWeather", "KWeather", "3.0.0",
            I18N_NOOP("Weather applet for the Kicker"), KAboutData::License_GPL_V2,
            0, I18N_NOOP("Weather data by Open-Meteo.com (CC BY 4.0)"));
    aboutData.addAuthor("Ian Reinhart Geiser", "", "geiseri@kde.org",
            "http://www.kde.org/");
    //aboutData.addAuthor("Nadeem Hasan",  "", "nhasan@nadmm.com", "http://www.nadmm.com/");
    aboutData.addCredit("Nadeem Hasan", I18N_NOOP("Lots of bugfixes,"
            " improvements and cleanups."), "nhasan@nadmm.com");
    aboutData.addCredit("Will Andrews", I18N_NOOP("Fixed for BSD port"),
            "wca@users.sourceforge.net", "");
    aboutData.addCredit("Ben Burton", I18N_NOOP("Debian fixes"), "benb@acm.org");
    aboutData.addCredit("Otto Bruggeman", I18N_NOOP("Fixed the i18n stuff and"
            " made sure the indentation was consistent :P"), "bruggie@home.nl");
    aboutData.addCredit( "Carles Carbonell Bernado",
            I18N_NOOP( "Great new weather icons" ), "mail@carlitus.net" );
    aboutData.addCredit( "John Ratke",
            I18N_NOOP( "Improvements and more code cleanups" ), "jratke@comcast.net" );
    aboutData.addCredit( "The KDE3 revival authors",
            I18N_NOOP( "Forecasts from Open-Meteo" ) );

    KAboutApplication about(&aboutData, this, 0);
    about.setProgramLogo( KGlobal::instance()->iconLoader()->iconPath( "kweather", -KIcon::SizeLarge ) );
    about.exec();
}

/** prefs */
void kweather::preferences()
{
    kdDebug(12004) << "prefs" << endl;

    savePrefs();

    if ( settingsDialog == 0 )
    {
      // one page since the station list of the service is gone
      settingsDialog = new KCMultiDialog( KJanusWidget::Plain,
              i18n( "Configure KWeather" ), this );
      connect( settingsDialog, SIGNAL( configCommitted() ), SLOT( slotPrefsAccepted() ) );

      settingsDialog->addModule( "kcmweather.desktop" );
    }

    settingsDialog->show();
    settingsDialog->raise();
}

/** The help handler */
void kweather::help()
{
    kapp->invokeHelp(QString::null, QString::fromLatin1("kweather"));
}

/** Show the forecast popup. */
void kweather::doReport()
{
    if ( reportLocation.isEmpty() )
    {
        // no location defined -> open settings dialog
        preferences();
        return;
    }

    if ( mPopup )
    {
        mPopup->close();
        return;
    }
    // the click on the applet that closed the popup must not open it again
    if ( mPopupHidden.isValid() && mPopupHidden.elapsed() < 300 )
        return;

    if ( !mWeatherService )
        initDCOP();
    if ( !mWeatherService )
        return;

    kdDebug(12004) << "Showing the forecast" << endl;
    mPopup = new ForecastPopup( mWeatherService, reportLocation, this );
    connect( mPopup, SIGNAL( hidden() ), SLOT( slotPopupHidden() ) );
    mPopup->popup( QRect( mapToGlobal( QPoint( 0, 0 ) ), size() ), popupDirection() );
}

void kweather::slotPopupHidden()
{
    // WDestructiveClose deletes it
    mPopup = 0;
    mPopupHidden.start();
}

/** load the application */
void kweather::loadPrefs(){
    kdDebug(12004) << "Loading Prefs" << endl;
    KConfig *kcConfig = config();
    kcConfig->reparseConfiguration();

    if (!kcConfig->hasGroup ("General Options") )
        mFirstRun = true;

    kcConfig->setGroup("General Options");
    logOn = kcConfig->readBoolEntry("logging", false);
    fileName = kcConfig->readPathEntry("log_file_name");
    reportLocation = kcConfig->readEntry("report_location");
    mLocationName = kcConfig->readEntry("location_name");
    mLocationCountry = kcConfig->readEntry("location_country");
    mViewMode = kcConfig->readNumEntry("smallview_mode", dockwidget::ShowTempOnly);
    mRainAlert = kcConfig->readBoolEntry("rain_alert", false);

    mTextColor = QColor();
    if ( kcConfig->hasKey("textColor") )
        mTextColor = kcConfig->readColorEntry("textColor");
}

/** Save the application mPrefs. */
void kweather::savePrefs(){
    kdDebug(12004) << "Saving Prefs..." << endl;
    KConfig *kcConfig = config();
    
    kcConfig->setGroup("General Options");
    kcConfig->writeEntry("logging", logOn);
    kcConfig->writeEntry("report_location", reportLocation);
    kcConfig->writeEntry("smallview_mode", mViewMode);
    kcConfig->writePathEntry("log_file_name", fileName );
    kcConfig->sync();
}

void kweather::showWeather()
{
    kdDebug(12004) << "Show weather" << endl;
    dockWidget->showWeather();
    emit updateLayout();
}

void kweather::writeLogEntry()
{
    // Write data line in the CSV format
    if (logOn && !fileName.isEmpty())
    {
        kdDebug(12004)<< "Try log file:" << fileName << endl;
        QFile logFile(fileName);
        QTextStream logFileStream(&logFile);
        if (logFile.open(IO_Append | IO_ReadWrite))
        {
            QString temperature = mWeatherService->temperature(reportLocation );
            QString wind        = mWeatherService->wind(reportLocation );
            QString pressure    = mWeatherService->pressure(reportLocation );
            QString date        = mWeatherService->date(reportLocation );
            QStringList weather = mWeatherService->weather(reportLocation );
            QStringList cover   = mWeatherService->cover(reportLocation );
            QString visibility; // METAR only
            logFileStream << date << ",";
            logFileStream << wind << ",";
            logFileStream << temperature << ",";
            logFileStream << pressure << ",";
            logFileStream << cover.join(";") << ",";
            logFileStream << visibility << ",";
            logFileStream << weather.join(";");
            logFileStream << endl;
        }
        else
        {
            KMessageBox::sorry( this,
                    i18n("For some reason the log file could not be written to.\n"
                    "Please check to see if your disk is full or if you "
                    "have write access to the location you are trying to "
                    "write to."),
                    i18n("KWeather Error"));
        }
        logFile.close();
    }
}

/** get new data */
void kweather::timeout()
{
    if ( !mWeatherService )
        initDCOP();
    
    if ( mWeatherService )
    {
        // isEmtpy is true for null and 0 length strings
        if ( !reportLocation.isEmpty() )
        {
        kdDebug(12004)<< "Requesting new data for " << reportLocation << endl;
        mWeatherService->update(reportLocation);
        }
    }
}

int kweather::widthForHeight(int h) const
{
    //kdDebug(12004) << "widthForHeight " << h << endl;
    dockWidget->setOrientation(Horizontal);
    int w = dockWidget->widthForHeight(h);
    return w;
}

int kweather::heightForWidth(int w) const
{
    kdDebug(12004) << "heightForWidth " << w<< endl;
    dockWidget->setOrientation(Vertical);
    int h = dockWidget->heightForWidth( w );
    return h;
}

void kweather::refresh(QString stationID)
{
    kdDebug(12004) << "refresh " << stationID << endl;
    if( stationID == reportLocation)
    {
        showWeather();
        writeLogEntry();
        checkRain();
    }
}

void kweather::checkRain()
{
    QString rain;
    if ( mRainAlert && mWeatherService )
        rain = mWeatherService->rainNextHour( reportLocation );
    // once per rain spell, not at every update
    if ( !rain.isEmpty() && !mRainExpected )
    {
        kdDebug(12004) << "rain expected: " << rain << endl;
        static KInstance *instance = new KInstance( "kweather" );
        KNotifyClient::Instance notifyInstance( instance );
        // the window of the panel: the applet's own is not a top level one
        KNotifyClient::event( topLevelWidget()->winId(), "rain",
                i18n( "Rain is expected in the next hour in %1: %2." )
                .arg( mWeatherService->stationName( reportLocation ) ).arg( rain ) );
    }
    mRainExpected = !rain.isEmpty();
}

void kweather::slotPrefsAccepted()
{
    // Preferences have been saved in the config file by the KCModule,
    // so read them out.
    loadPrefs();
    mRainExpected = false;
    delete mContextMenu;    // its title names the place
    initContextMenu();

    dockWidget->setLocationCode(reportLocation);
    dockWidget->setViewMode(mViewMode);
    setLabelColor();
    emit updateLayout();

    if (logOn && !fileName.isEmpty())
    {
        QFile logFile(fileName);
        // Open the file, create it if not already exists
        if (logFile.open(IO_ReadWrite))
        {
            if (logFile.size() == 0)
            {
                // Empty file, put the header
                QTextStream logFileStream(&logFile);
                logFileStream << "Date,Wind Speed & Direction,Temperature,Pressure,Cover,Visibility,Current Weather" << endl;
            }
            logFile.close();
        }
        else
        {
            kdDebug(12004) << "There was an error opening the file...." << endl;
            KMessageBox::sorry( this,
                    i18n("For some reason a new log file could not be opened.\n"
                    "Please check to see if your disk is full or if you have "
                    "write access to the location you are trying to write to."),
                    i18n("KWeather Error"));
        }
    }

    timeout();
}

void kweather::mousePressEvent(QMouseEvent *e)
{
    if ( e->button() != RightButton )
    {
        KPanelApplet::mousePressEvent( e );
        return;
    }

    mContextMenu->exec(e->globalPos());
}

void kweather::slotUpdateNow()
{
    if ( !mWeatherService )
         initDCOP();

    if ( mWeatherService && !reportLocation.isEmpty() )
        mWeatherService->forceUpdate(reportLocation );
}

bool kweather::attach()
{
    QString error;
    kdDebug(12004) << "Looking for dcop service..." << endl;
    if (!mClient->isApplicationRegistered("KWeatherService"))
    {
        kdDebug(12004) << "Could not find service so I am starting it..."
            << endl;
        if (!KApplication::startServiceByDesktopName("kweatherservice",
            QStringList(), &error))
        {
            kdDebug(12004) << "Starting KWeatherService failed with message: "
                << error << endl;
            return false;
        }

        kdDebug (12004) << "Service Started..." << endl;
    }
    else
    {
        kdDebug(12004) << "Found weather service..." << endl;
    }

    return true;
}

void kweather::resizeEvent(QResizeEvent *e)
{
    kdDebug(12004) << "KWeather Resize event " << e->size() << endl;
    dockWidget->resizeView(e->size());
}

void kweather::setLabelColor()
{
    if ( mTextColor.isValid() )
        setPaletteForegroundColor(mTextColor);
    else
        unsetPalette();
}

void kweather::paletteChange(const QPalette &)
{
    if ( mTextColor.isValid() && paletteForegroundColor() != mTextColor )
        setLabelColor();
}

#include "kweather.moc"
