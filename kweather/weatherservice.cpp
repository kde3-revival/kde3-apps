/* This file is part of the KDE project
   Copyright (C) 2001-2004 Ian Geiser <geiseri@kde.org>
   Copyright (C) 2002-2004 Nadeem Hasan <nhasan@kde.org>
   Copyright (C) 2003-2004 John Ratke
   Copyright (C) 2026 The KDE3 revival authors (Open-Meteo)

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; see the file COPYING.  If not, write to
   the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301, USA.
*/

#include <kdebug.h>
#include <kapplication.h>
#include <kconfig.h>
#include <kglobal.h>
#include <klocale.h>
#include <kstandarddirs.h>

#include "weatherlib.h"
#include "weatherservice.h"
#include "weathericons.h"
#include "weathermodels.h"
#include "weatherservers.h"

WeatherService::WeatherService(QObject *parent, const char *name) : QObject (parent, name),  DCOPObject("WeatherService")
{
	kdDebug(12006) << "Starting new service... " << endl;

	m_config = new KConfig("kweatherservicerc");
	m_weatherLib = new WeatherLib(this, "WeatherLib");
	connect(m_weatherLib, SIGNAL(fileUpdating( const QString&)),
			SLOT(updating( const QString&)));
	connect(m_weatherLib, SIGNAL(fileUpdate( const QString&)),
			SLOT(updated( const QString&)));
	connect(m_weatherLib, SIGNAL(stationRemoved(const QString&)),
			SLOT(slotStationRemoved(const QString&)));

	reloadSettings();

	KConfig *conf = m_config;
	conf->setGroup("WEATHERSTATIONS");
	QStringList stations =conf->readListEntry("stations");
        QStringList::Iterator it = stations.begin();
	for ( ; it != stations.end(); ++it )
		m_weatherLib->update(*it);
}

WeatherService::~WeatherService()
{
	kdDebug(12006) << "Going away... " << endl;
	delete m_config;
	// Don't need to call saveSettings() because WeatherService::exit
        // already does that. Plus it wouldn't work here anyway because
        // we can't get the config after calling kapp->quit();
}

void WeatherService::reloadSettings()
{
	KConfig *conf = m_config;
	conf->reparseConfiguration();
	conf->setGroup("Open-Meteo");
	m_weatherLib->setServer(conf->readEntry("forecast_url", WeatherServers::forecast()));
	conf->setGroup("Units");
	bool metric = KGlobal::locale()->measureSystem() == KLocale::Metric;
	m_weatherLib->setUnits(
		conf->readEntry("temperature", metric ? "celsius" : "fahrenheit"),
		conf->readEntry("wind", metric ? "kmh" : "mph"),
		conf->readEntry("precipitation", metric ? "mm" : "inch"),
		conf->readEntry("pressure", metric ? "hPa" : "inHg"));
}

void WeatherService::updated(const QString &stationID)
{
	kdDebug(12006) << "Sending update for " << stationID << endl;
	emit fileUpdate( stationID );
}

void WeatherService::updateAll()
{
	kdDebug(12006) << "Sending for all" << endl;
	QStringList stations =  m_weatherLib->stations();
	QStringList::ConstIterator end(stations.end());
	for  ( QStringList::ConstIterator it = stations.begin(); it != end; ++it ) {
		update(*it);
    	}

}

void WeatherService::updating(const QString &stationID)
{
	kdDebug(12006) << "Sending updating for " << stationID << endl;
	emit fileUpdating( stationID );
}

void WeatherService::slotStationRemoved(const QString &stationID)
{
  kdDebug(12006) << "Sending stationRemoved for " << stationID << endl;
  emit stationRemoved( stationID );
}

QString WeatherService::temperature(const QString &stationID)
{
	kdDebug (12006) << "Returning " << stationID << endl;
	return m_weatherLib->temperature(stationID);
}

QString WeatherService::apparentTemperature(const QString &stationID)
{
	return m_weatherLib->apparentTemperature(stationID);
}

QString WeatherService::dewPoint(const QString &stationID)
{
	return m_weatherLib->dewPoint(stationID);
}

QString WeatherService::relativeHumidity(const QString &stationID)
{
	return m_weatherLib->relHumidity(stationID);
}

QString WeatherService::heatIndex(const QString &stationID)
{
	return m_weatherLib->heatIndex(stationID);
}

QString WeatherService::windChill(const QString &stationID)
{
	return m_weatherLib->windChill(stationID);
}

QString WeatherService::wind(const QString &stationID)
{
	return m_weatherLib->wind(stationID);
}

QString WeatherService::pressure(const QString &stationID)
{
	return m_weatherLib->pressure(stationID);
}

QString WeatherService::precipitation(const QString &stationID)
{
	return m_weatherLib->precipitation(stationID);
}

QPixmap WeatherService::currentIcon(const QString &stationID)
{
    return icon( stationID );
}

QPixmap WeatherService::icon(const QString &stationID)
{
	kdDebug(12006) << "Get the current weather icon.." << endl;
	return WeatherIcons::load(m_weatherLib->iconName(stationID), 48);
}

QString WeatherService::currentIconString(const QString &stationID)
{
	return m_weatherLib->iconName(stationID);
}

QString WeatherService::iconFileName(const QString &stationID)
{
	return WeatherIcons::fallbackFile(m_weatherLib->iconName(stationID));
}

QString WeatherService::date(const QString &stationID)
{
	return m_weatherLib->date(stationID);
}

QStringList WeatherService::cover(const QString &stationID)
{
	return m_weatherLib->cover(stationID);
}

QStringList WeatherService::weather(const QString &stationID)
{
	return m_weatherLib->weather(stationID);
}

bool WeatherService::stationNeedsMaintenance(const QString &)
{
	return false;
}

QString WeatherService::error(const QString &stationID)
{
	return m_weatherLib->error(stationID);
}

QStringList WeatherService::hourlyForecast(const QString &stationID, int hours)
{
	return m_weatherLib->hourly(stationID, hours);
}

QStringList WeatherService::dailyForecast(const QString &stationID)
{
	return m_weatherLib->daily(stationID);
}

QStringList WeatherService::hourlyValues(const QString &stationID, int hours)
{
	return m_weatherLib->hourlyValues(stationID, hours);
}

QStringList WeatherService::dailyValues(const QString &stationID)
{
	return m_weatherLib->dailyValues(stationID);
}

QStringList WeatherService::dayHours(const QString &stationID, const QString &date)
{
	return m_weatherLib->dayHours(stationID, date);
}

QString WeatherService::currentTime(const QString &stationID)
{
	return m_weatherLib->currentTime(stationID);
}

QString WeatherService::rainNextHour(const QString &stationID)
{
	return m_weatherLib->rainNextHour(stationID);
}

void WeatherService::update(const QString &stationID)
{
	m_weatherLib->update(stationID);
}

void WeatherService::forceUpdate(const QString &stationID)
{
	m_weatherLib->forceUpdate(stationID);
}

void WeatherService::removeStation(const QString &stationID)
{
	m_weatherLib->remove(stationID);
	KConfig *conf = m_config;
	conf->deleteGroup("Location " + stationID);
	saveSettings();
}

void WeatherService::addStation(const QString &stationID)
{
	double lat, lon;
	QString m;
	if (!WeatherModels::parseLocationId(stationID, &lat, &lon, &m))
		return;
	m_weatherLib->update(stationID);
	saveSettings();
}

void WeatherService::addLocation(const QString &stationID, const QString &name,
                                 const QString &country)
{
	double lat, lon;
	QString m;
	if (!WeatherModels::parseLocationId(stationID, &lat, &lon, &m))
		return;
	KConfig *conf = m_config;
	conf->setGroup("Location " + stationID);
	conf->writeEntry("name", name);
	conf->writeEntry("country", country);
	addStation(stationID);
}

void WeatherService::exit()
{
	saveSettings();
	kapp->quit();
}

QStringList WeatherService::listStations()
{
	return m_weatherLib->stations();
}

void WeatherService::saveSettings()
{
	KConfig *conf = m_config;
	conf->setGroup("WEATHERSTATIONS");
	conf->writeEntry( "stations", m_weatherLib->stations());
	conf->sync();
}

QString WeatherService::stationName(const QString &stationID)
{
	KConfig *conf = m_config;
	conf->setGroup("Location " + stationID);
	QString name = conf->readEntry("name");
	if (!name.isEmpty())
		return name;
	return latitude(stationID) + ", " + longitude(stationID);
}

QString WeatherService::stationCountry(const QString &stationID)
{
	KConfig *conf = m_config;
	conf->setGroup("Location " + stationID);
	return conf->readEntry("country");
}

QString WeatherService::longitude(const QString &stationID)
{
	double lat, lon;
	QString m;
	if (!WeatherModels::parseLocationId(stationID, &lat, &lon, &m))
		return i18n("Unknown Station");
	return KGlobal::locale()->formatNumber(lon, 4);
}

QString WeatherService::latitude(const QString &stationID)
{
	double lat, lon;
	QString m;
	if (!WeatherModels::parseLocationId(stationID, &lat, &lon, &m))
		return i18n("Unknown Station");
	return KGlobal::locale()->formatNumber(lat, 4);
}

QString WeatherService::model(const QString &stationID)
{
	double lat, lon;
	QString m;
	if (!WeatherModels::parseLocationId(stationID, &lat, &lon, &m))
		return QString::null;
	return m;
}

QString WeatherService::modelName(const QString &stationID)
{
	return WeatherModels::displayName(model(stationID));
}

QStringList WeatherService::models()
{
	QStringList list;
	QStringList names = WeatherModels::models();
	for (QStringList::ConstIterator it = names.begin(); it != names.end(); ++it)
		list.append(*it + '\t' + WeatherModels::displayName(*it));
	return list;
}

QString WeatherService::sunRiseTime(const QString &stationID)
{
	return m_weatherLib->sunRise(stationID);
}

QString WeatherService::sunSetTime(const QString &stationID)
{
	return m_weatherLib->sunSet(stationID);
}

QString WeatherService::localDate(const QString &stationID)
{
	return m_weatherLib->localDate(stationID);
}

QString WeatherService::visibility(const QString &)
{
	return QString::null;
}

QStringList WeatherService::findStations(float, float)
{
	return QStringList();
}

QString WeatherService::civilTwilightStart(const QString &)
{
	return QString::null;
}

QString WeatherService::civilTwilightEnd(const QString &)
{
	return QString::null;
}

QString WeatherService::stationCode(const QString &stationName)
{
	QStringList groups = m_config->groupList();
	for (QStringList::ConstIterator it = groups.begin(); it != groups.end(); ++it) {
		if (!(*it).startsWith("Location "))
			continue;
		m_config->setGroup(*it);
		if (m_config->readEntry("name") == stationName)
			return (*it).mid(9);
	}
	return QString::null;
}

#include "weatherservice.moc"
