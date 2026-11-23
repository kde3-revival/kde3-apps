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

#ifndef _WEATHER_SERVICE
#define _WEATHER_SERVICE

#include <qstringlist.h>
#include <dcopobject.h>
#include <qpixmap.h>

class KConfig;
class WeatherLib;

/**
 * The DCOP service KWeatherService: the weather of the locations the
 * weather applets (and other programs) ask for, from Open-Meteo.
 *
 * A location ("station" in the names kept from KWeather 3.5, whose
 * stations were METAR airport codes) is the key
 * "<latitude>,<longitude>,<model>", e.g. "44.4323,26.1063,ecmwf_ifs";
 * the models are those of models().  Values are returned as text in the
 * units of the settings ([Units] in kweatherservicerc).
 */
class WeatherService : public QObject, public DCOPObject
{
	Q_OBJECT
	K_DCOP

	private:
		WeatherLib  *m_weatherLib;
		// kweatherservicerc, also written by the settings of the applet
		// (KDE 3.5 used KWeatherServicerc, whose METAR stations are no use)
		KConfig     *m_config;

	private slots:
		void updated(const QString &stationID);
		void updating(const QString &stationID);
		void slotStationRemoved(const QString &stationID);

	public:
		WeatherService(QObject *parent, const char *name);
		~WeatherService();

	k_dcop_signals:
		void fileUpdating(QString);
		void fileUpdate(QString);
		void stationRemoved(QString);

	k_dcop:
		QString temperature(const QString &stationID);
		QString apparentTemperature(const QString &stationID);
		QString dewPoint(const QString &stationID);
		QString relativeHumidity(const QString &stationID);
		QString heatIndex(const QString &stationID);
		QString windChill(const QString &stationID);
		QString wind(const QString &stationID);
		QString pressure(const QString &stationID);
		QString precipitation(const QString &stationID);
		QPixmap currentIcon(const QString &stationID);
		QPixmap icon(const QString &stationID);
		/** The icon name of the icon theme (weather-clear, ...). */
		QString currentIconString(const QString &stationID);
		/** The picture of KWeather 3.5 for the current weather. */
		QString iconFileName(const QString &stationID);
		QString date(const QString &stationID);
		QStringList cover(const QString &stationID);
		QStringList weather(const QString &stationID);
		/** Always false: there are no stations to maintain any more. */
		bool stationNeedsMaintenance(const QString &stationID);
		/** Why the last download failed, empty if it did not. */
		QString error(const QString &stationID);

		QString stationName(const QString &stationID);
		QString stationCountry(const QString &stationID);
		QString longitude(const QString &stationID);
		QString latitude(const QString &stationID);
		/** The forecast model of the location and its name. */
		QString model(const QString &stationID);
		QString modelName(const QString &stationID);
		/** The models, as "<name>\t<description>". */
		QStringList models();

		QString sunRiseTime(const QString &stationID);
		QString sunSetTime(const QString &stationID);
		/** Today at the location (ISO 8601), which may not be today here. */
		QString localDate(const QString &stationID);

		/**
		 * The next @p hours hours, one entry per hour:
		 * "<local time, ISO 8601>\t<icon>\t<temperature>\t<precipitation
		 * of the hour that ends then, empty if none>\t<probability of
		 * precipitation, empty if the model has none>\t<description>".
		 */
		QStringList hourlyForecast(const QString &stationID, int hours);
		/**
		 * Today and the next 6 days: "<date, ISO 8601>\t<icon>\t<maximum>
		 * \t<minimum>\t<precipitation>\t<probability>\t<description>".
		 */
		QStringList dailyForecast(const QString &stationID);
		/** The precipitation of the next hour if there is any, else empty. */
		QString rainNextHour(const QString &stationID);

		void update(const QString &stationID);
		void updateAll();
		void forceUpdate(const QString &stationID);
		void removeStation(const QString &stationID);
		void addStation(const QString &stationID);
		/** Adds a location with the name and country to show for it. */
		void addLocation(const QString &stationID, const QString &name,
		                 const QString &country);
		QStringList listStations();
		/** Reads the units from the configuration again. */
		void reloadSettings();

		void exit();

		// Calls of KWeather 3.5 without a meaning for Open-Meteo, kept so
		// that old clients get an answer instead of a failed call.  Their
		// station IDs (METAR codes) are not locations any more.
		/** Deprecated: empty (METAR visibility). */
		QString visibility(const QString &stationID);
		/** Deprecated: empty (needed a station database). */
		QStringList findStations(float lon, float lat);
		/** Deprecated: empty. */
		QString civilTwilightStart(const QString &stationID);
		/** Deprecated: empty. */
		QString civilTwilightEnd(const QString &stationID);
		/** The location key of a place added with addLocation(), by its
		    name; empty if there is none. */
		QString stationCode(const QString &stationName);

  private:
		void saveSettings();
};
#endif
