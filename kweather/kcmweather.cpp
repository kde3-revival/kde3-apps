/*
  This file is part of KWeather.
  Copyright (c) 2003 Tobias Koenig <tokoe@kde.org>
  Copyright (c) 2026 The KDE3 revival authors (Open-Meteo)

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

  As a special exception, permission is given to link this program
  with any edition of Qt, and distribute the resulting executable,
  without including the source code for Qt in the source distribution.
*/

#include <qbuttongroup.h>
#include <qcheckbox.h>
#include <qgroupbox.h>
#include <qhbox.h>
#include <qlabel.h>
#include <qlayout.h>
#include <qlistbox.h>
#include <qlocale.h>
#include <qpushbutton.h>
#include <qradiobutton.h>
#include <qstylesheet.h>
#include <qvbox.h>
#include <qwhatsthis.h>

#include <kaboutdata.h>
#include <kapplication.h>
#include <kcolorbutton.h>
#include <kcombobox.h>
#include <kconfig.h>
#include <kdebug.h>
#include <kdialog.h>
#include <kglobal.h>
#include <klineedit.h>
#include <klocale.h>
#include <kurl.h>
#include <kurlrequester.h>

#include <math.h>

#include "dockwidget.h"
#include "download.h"
#include "json.h"
#include "kcmweather.h"
#include "weathermodels.h"
#include "weatherservers.h"
#include "weatherservice_stub.h"

namespace
{

// value and I18N_NOOP text, pairwise, 0-terminated
const char *const temperatureUnits[] = {
  "celsius", I18N_NOOP( "Celsius (\xc2\xb0""C)" ),
  "fahrenheit", I18N_NOOP( "Fahrenheit (\xc2\xb0""F)" ), 0 };
const char *const windUnits[] = {
  "kmh", I18N_NOOP( "km/h" ), "ms", I18N_NOOP( "m/s" ),
  "mph", I18N_NOOP( "mph" ), "kn", I18N_NOOP( "knots" ), 0 };
const char *const precipitationUnits[] = {
  "mm", I18N_NOOP( "millimeters" ), "inch", I18N_NOOP( "inches" ), 0 };
const char *const pressureUnits[] = {
  "hPa", I18N_NOOP( "hectopascals (hPa)" ), "inHg", I18N_NOOP( "inches of mercury (inHg)" ), 0 };

}

extern "C"
{
  KDE_EXPORT KCModule *create_weather( QWidget *parent, const char * )
  {
    return new KCMWeather( parent, "kweather" );
  }
}

KCMWeather::KCMWeather( QWidget *parent, const char *name )
  : KCModule( parent, name ), mSearchJob( 0 ), mHaveLocation( false )
{
  KGlobal::locale()->insertCatalogue( "kweather" );
  mWeatherService = new WeatherService_stub( "KWeatherService",
      "WeatherService" );

  QVBoxLayout *layout = new QVBoxLayout( this, 0, KDialog::spacingHint() );

  // location
  QGroupBox *box = new QGroupBox( 1, Qt::Horizontal, i18n( "Location" ), this );
  layout->addWidget( box );
  mLocationLabel = new QLabel( box );
  QHBox *searchBox = new QHBox( box );
  searchBox->setSpacing( KDialog::spacingHint() );
  mSearch = new KLineEdit( searchBox );
  // Return searches; it must not close the dialog as well
  mSearch->setTrapReturnKey( true );
  mSearchButton = new QPushButton( i18n( "&Search" ), searchBox );
  QWhatsThis::add( mSearch, i18n( "Type the name of a city or village and press "
      "Search, then choose it in the list. Only the coordinates of the place "
      "you choose are sent to Open-Meteo for the forecast; the name you type "
      "is sent to the geocoding service of Open-Meteo." ) );
  mResults = new QListBox( box );
  mResults->setMinimumHeight( mResults->fontMetrics().height() * 5 );
  connect( mSearch, SIGNAL( returnPressed() ), SLOT( searchLocation() ) );
  connect( mSearchButton, SIGNAL( clicked() ), SLOT( searchLocation() ) );
  connect( mResults, SIGNAL( highlighted( int ) ), SLOT( locationSelected( int ) ) );

  // forecast and units
  box = new QGroupBox( 2, Qt::Horizontal, i18n( "Forecast" ), this );
  layout->addWidget( box );
  QLabel *label = new QLabel( i18n( "&Model:" ), box );
  mModel = new KComboBox( box );
  label->setBuddy( mModel );
  QStringList models = WeatherModels::models();
  for ( QStringList::ConstIterator it = models.begin(); it != models.end(); ++it )
    mModel->insertItem( WeatherModels::displayName( *it ) );
  QWhatsThis::add( mModel, i18n( "The weather model of the forecast. The ECMWF "
      "IFS HRES (9 km) is the global model of the European Centre; ICON-EU (7 km, "
      "5 days) is the European model of the German weather service; the hybrid "
      "uses ICON-EU for the next hours and the ECMWF model for the days. "
      "kweather/tools/forecast-accuracy.py in the sources measures which one "
      "forecast best at a weather station near you." ) );

  label = new QLabel( i18n( "&Temperature:" ), box );
  mTemperature = new KComboBox( box );
  label->setBuddy( mTemperature );
  fillCombo( mTemperature, temperatureUnits );
  label = new QLabel( i18n( "&Wind speed:" ), box );
  mWind = new KComboBox( box );
  label->setBuddy( mWind );
  fillCombo( mWind, windUnits );
  label = new QLabel( i18n( "&Precipitation:" ), box );
  mPrecipitation = new KComboBox( box );
  label->setBuddy( mPrecipitation );
  fillCombo( mPrecipitation, precipitationUnits );
  label = new QLabel( i18n( "P&ressure:" ), box );
  mPressure = new KComboBox( box );
  label->setBuddy( mPressure );
  fillCombo( mPressure, pressureUnits );

  // panel
  mViewMode = new QButtonGroup( 1, Qt::Horizontal, i18n( "Panel Display" ), this );
  layout->addWidget( mViewMode );
  mViewMode->insert( new QRadioButton( i18n( "&Icon only" ), mViewMode ), dockwidget::ShowIconOnly );
  mViewMode->insert( new QRadioButton( i18n( "Icon and t&emperature" ), mViewMode ), dockwidget::ShowTempOnly );
  mViewMode->insert( new QRadioButton( i18n( "Icon, temperature, wi&nd and pressure" ), mViewMode ), dockwidget::ShowAll );
  QHBox *colorBox = new QHBox( mViewMode );
  colorBox->setSpacing( KDialog::spacingHint() );
  mCustomColor = new QCheckBox( i18n( "&Custom text color:" ), colorBox );
  mTextColor = new KColorButton( colorBox );
  connect( mCustomColor, SIGNAL( toggled( bool ) ), mTextColor, SLOT( setEnabled( bool ) ) );

  // notifications and log
  box = new QGroupBox( 1, Qt::Horizontal, i18n( "Notifications" ), this );
  layout->addWidget( box );
  mRainAlert = new QCheckBox( i18n( "Notify me when rain is &expected in the next hour" ), box );
  mEnableLog = new QCheckBox( i18n( "&Log the weather to a file (CSV)" ), box );
  QHBox *logBox = new QHBox( box );
  logBox->setSpacing( KDialog::spacingHint() );
  mLogLabel = new QLabel( i18n( "File:" ), logBox );
  mLogFile = new KURLRequester( logBox );
  layout->addStretch();

  connect( mEnableLog, SIGNAL( toggled( bool ) ), SLOT( enableLogWidgets( bool ) ) );
  connect( mModel, SIGNAL( activated( int ) ), SLOT( configChanged() ) );
  connect( mTemperature, SIGNAL( activated( int ) ), SLOT( configChanged() ) );
  connect( mWind, SIGNAL( activated( int ) ), SLOT( configChanged() ) );
  connect( mPrecipitation, SIGNAL( activated( int ) ), SLOT( configChanged() ) );
  connect( mPressure, SIGNAL( activated( int ) ), SLOT( configChanged() ) );
  connect( mViewMode, SIGNAL( clicked( int ) ), SLOT( configChanged() ) );
  connect( mCustomColor, SIGNAL( toggled( bool ) ), SLOT( configChanged() ) );
  connect( mTextColor, SIGNAL( changed( const QColor & ) ), SLOT( configChanged() ) );
  connect( mRainAlert, SIGNAL( toggled( bool ) ), SLOT( configChanged() ) );
  connect( mLogFile, SIGNAL( textChanged( const QString & ) ), SLOT( configChanged() ) );

  load();

  KAboutData *about = new KAboutData(
      I18N_NOOP( "kcmweather" ),
      I18N_NOOP( "KWeather Configure Dialog" ),
      0, 0, KAboutData::License_GPL,
      I18N_NOOP( "(c), 2003 Tobias Koenig" ) );

  about->addAuthor( "Tobias Koenig", 0, "tokoe@kde.org" );
  setAboutData(about);
}

KCMWeather::~KCMWeather()
{
  if ( mSearchJob )
    mSearchJob->abort();
  delete mWeatherService;
}

void KCMWeather::fillCombo( KComboBox *combo, const char *const *items )
{
  for ( ; *items; items += 2 )
    combo->insertItem( i18n( items[1] ) );
}

void KCMWeather::setComboItem( KComboBox *combo, const char *const *items,
                               const QString &value )
{
  for ( int i = 0; items[2 * i]; ++i )
    if ( value == QString::fromLatin1( items[2 * i] ) )
      combo->setCurrentItem( i );
}

QString KCMWeather::comboItem( KComboBox *combo, const char *const *items )
{
  return QString::fromLatin1( items[2 * combo->currentItem()] );
}

void KCMWeather::configChanged()
{
  emit changed( true );
}

void KCMWeather::enableLogWidgets( bool value )
{
  mLogFile->setEnabled( value );
  mLogLabel->setEnabled( value );
  emit changed( true );
}

void KCMWeather::showLocation()
{
  if ( !mHaveLocation )
  {
    mLocationLabel->setText( i18n( "No location is set yet: search for it below." ) );
    return;
  }
  QString name = mLocation.country.isEmpty() ? mLocation.name
      : i18n( "place, country", "%1, %2" ).arg( mLocation.name ).arg( mLocation.country );
  mLocationLabel->setText( "<qt>" + i18n( "Weather for <b>%1</b> (%2, %3)" )
      .arg( QStyleSheet::escape( name ) )
      .arg( KGlobal::locale()->formatNumber( mLocation.latitude, 2 ) )
      .arg( KGlobal::locale()->formatNumber( mLocation.longitude, 2 ) ) + "</qt>" );
}

void KCMWeather::searchLocation()
{
  QString text = mSearch->text().stripWhiteSpace();
  if ( text.isEmpty() )
    return;
  if ( mSearchJob )
    mSearchJob->abort();

  QString language = KGlobal::locale()->language().left( 2 );
  KConfig service( "kweatherservicerc", true );
  service.setGroup( "Open-Meteo" );
  KURL url( service.readEntry( "geocoding_url", WeatherServers::geocoding() ) );
  url.setQuery( "name=" + KURL::encode_string( text, 106 /* UTF-8 */ )
                + "&count=20&format=json&language=" + KURL::encode_string( language ) );
  kdDebug() << "Geocoding " << url.url() << endl;
  // 20 places are about 10 KiB
  mSearchJob = new Download( url, 256 * 1024, 30, this );
  connect( mSearchJob, SIGNAL( finished( Download*, const QByteArray&, const QString& ) ),
           SLOT( searchDone( Download*, const QByteArray&, const QString& ) ) );
  mResults->clear();
  mPlaces.clear();
  mResults->insertItem( i18n( "Searching..." ) );
  mResults->setEnabled( false );
}

void KCMWeather::searchDone( Download *download, const QByteArray &data,
                             const QString &error )
{
  if ( download != mSearchJob )
    return;
  mSearchJob = 0;
  mResults->clear();
  mPlaces.clear();
  mResults->setEnabled( true );

  if ( !error.isEmpty() )
  {
    mResults->insertItem( i18n( "The search failed: %1" ).arg( error ) );
    return;
  }
  bool ok = false;
  QVariant root = Json::parse( data, &ok );
  QVariant results = Json::member( root, "results" );
  if ( !ok || results.type() != QVariant::List || results.toList().isEmpty() )
  {
    mResults->insertItem( i18n( "No place of this name was found." ) );
    return;
  }

  QValueList<QVariant> list = results.toList();
  for ( QValueList<QVariant>::ConstIterator it = list.begin(); it != list.end(); ++it )
  {
    Place p;
    p.name = Json::member( *it, "name" ).toString();
    p.country = Json::member( *it, "country" ).toString();
    p.latitude = Json::number( Json::member( *it, "latitude" ), 1000 );
    p.longitude = Json::number( Json::member( *it, "longitude" ), 1000 );
    if ( p.name.isEmpty() || !isfinite( p.latitude ) || !isfinite( p.longitude )
         || p.latitude < -90 || p.latitude > 90
         || p.longitude < -180 || p.longitude > 180 )
      continue;
    p.name = p.name.left( 100 );
    p.country = p.country.left( 100 );
    QStringList parts;
    parts.append( p.name );
    QString admin = Json::member( *it, "admin1" ).toString().left( 100 );
    if ( !admin.isEmpty() && admin != p.name )
      parts.append( admin );
    if ( !p.country.isEmpty() )
      parts.append( p.country );
    QString text = parts.join( ", " );
    double elevation = Json::number( Json::member( *it, "elevation" ), -10000 );
    if ( !isfinite( elevation ) || elevation > 10000 )
      elevation = -10000;
    text += " (" + KGlobal::locale()->formatNumber( p.latitude, 2 ) + ", "
          + KGlobal::locale()->formatNumber( p.longitude, 2 );
    if ( elevation > -1000 )
      text += ", " + i18n( "%1 m" ).arg( static_cast<long>( elevation ) );
    text += ")";
    mPlaces.append( p );
    mResults->insertItem( text );
  }
  mResults->setEnabled( true );
}

void KCMWeather::locationSelected( int index )
{
  if ( index < 0 || index >= int( mPlaces.count() ) )
    return;
  mLocation = mPlaces[index];
  mHaveLocation = true;
  showLocation();
  emit changed( true );
}

void KCMWeather::load()
{
  kdDebug() << "Load" << endl;
  KConfig config( "weather_panelappletrc" );

  config.setGroup( "General Options" );
  bool enabled = config.readBoolEntry( "logging", false );
  mEnableLog->setChecked( enabled );
  enableLogWidgets( enabled );
  mLogFile->setURL( config.readPathEntry( "log_file_name" ) );

  mCustomColor->setChecked( config.hasKey( "textColor" ) );
  mTextColor->setEnabled( mCustomColor->isChecked() );
  static QColor black( Qt::black );
  mTextColor->setColor( config.readColorEntry( "textColor", &black ) );

  mRainAlert->setChecked( config.readBoolEntry( "rain_alert", false ) );

  double lat, lon;
  QString model;
  mHaveLocation = WeatherModels::parseLocationId( config.readEntry( "report_location" ),
                                                  &lat, &lon, &model );
  if ( mHaveLocation )
  {
    mLocation.name = config.readEntry( "location_name" );
    mLocation.country = config.readEntry( "location_country" );
    mLocation.latitude = lat;
    mLocation.longitude = lon;
  }
  else
  {
    model = WeatherModels::defaultModel();
  }
  showLocation();
  mModel->setCurrentItem( WeatherModels::models().findIndex( model ) );

  int mode = config.readNumEntry( "smallview_mode", dockwidget::ShowTempOnly );
  mViewMode->setButton( mode );

  KConfig service( "kweatherservicerc", true );
  service.setGroup( "Units" );
  bool metric = KGlobal::locale()->measureSystem() == KLocale::Metric;
  setComboItem( mTemperature, temperatureUnits,
                service.readEntry( "temperature", metric ? "celsius" : "fahrenheit" ) );
  setComboItem( mWind, windUnits, service.readEntry( "wind", metric ? "kmh" : "mph" ) );
  setComboItem( mPrecipitation, precipitationUnits,
                service.readEntry( "precipitation", metric ? "mm" : "inch" ) );
  setComboItem( mPressure, pressureUnits,
                service.readEntry( "pressure", metric ? "hPa" : "inHg" ) );

  emit changed( false );
}

void KCMWeather::save()
{
  kdDebug() << "Save" << endl;

  KConfig service( "kweatherservicerc" );
  service.setGroup( "Units" );
  service.writeEntry( "temperature", comboItem( mTemperature, temperatureUnits ) );
  service.writeEntry( "wind", comboItem( mWind, windUnits ) );
  service.writeEntry( "precipitation", comboItem( mPrecipitation, precipitationUnits ) );
  service.writeEntry( "pressure", comboItem( mPressure, pressureUnits ) );
  service.sync();

  KConfig config( "weather_panelappletrc" );
  config.setGroup( "General Options" );
  config.writeEntry( "logging", mEnableLog->isChecked() );
  config.writePathEntry( "log_file_name", mLogFile->url() );
  if ( mCustomColor->isChecked() )
    config.writeEntry( "textColor", mTextColor->color() );
  else
    config.deleteEntry( "textColor" );
  config.writeEntry( "rain_alert", mRainAlert->isChecked() );
  config.writeEntry( "smallview_mode", mViewMode->selectedId() );

  QString model = WeatherModels::models()[ mModel->currentItem() ];
  QString old = config.readEntry( "report_location" );
  if ( mHaveLocation )
  {
    QString id = WeatherModels::locationId( mLocation.latitude, mLocation.longitude, model );
    config.writeEntry( "report_location", id );
    config.writeEntry( "location_name", mLocation.name );
    config.writeEntry( "location_country", mLocation.country );
    config.sync();
    mWeatherService->reloadSettings();
    mWeatherService->addLocation( id, mLocation.name, mLocation.country );
    if ( !old.isEmpty() && old != id )
      mWeatherService->removeStation( old );
  }
  else
  {
    config.sync();
    mWeatherService->reloadSettings();
  }

  emit changed( false );
}

void KCMWeather::defaults()
{
  mEnableLog->setChecked( false );
  enableLogWidgets( false );
  mLogFile->setURL( "" );
  mCustomColor->setChecked( false );
  mRainAlert->setChecked( false );
  mModel->setCurrentItem( WeatherModels::models().findIndex( WeatherModels::defaultModel() ) );
  mViewMode->setButton( dockwidget::ShowTempOnly );
  bool metric = KGlobal::locale()->measureSystem() == KLocale::Metric;
  setComboItem( mTemperature, temperatureUnits, metric ? "celsius" : "fahrenheit" );
  setComboItem( mWind, windUnits, metric ? "kmh" : "mph" );
  setComboItem( mPrecipitation, precipitationUnits, metric ? "mm" : "inch" );
  setComboItem( mPressure, pressureUnits, metric ? "hPa" : "inHg" );

  emit changed( true );
}

#include "kcmweather.moc"
