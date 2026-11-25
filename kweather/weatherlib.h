/***************************************************************************
                          weatherlib.h  -  description
                             -------------------
    begin                : Wed Jul 5 2000
    copyright            : (C) 2000 by Ian Reinhart Geiser
                           (C) 2026 The KDE3 revival authors (Open-Meteo)
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

#ifndef WEATHERLIB_H
#define WEATHERLIB_H

#include <qcstring.h>
#include <qdict.h>
#include <qmap.h>
#include <qobject.h>
#include <qstring.h>
#include <qstringlist.h>

class Download;

/**
 * The weather of the locations, from the forecast API of Open-Meteo
 * (https://open-meteo.com/, no account or key).  A location is a key
 * "<latitude>,<longitude>,<model>" (see WeatherModels::locationId());
 * only its coordinates and the model are sent.  The values are returned
 * as text, in the units they were downloaded in (see setUnits()).
 */
class WeatherLib : public QObject
{
	Q_OBJECT

	public:
		class Data;
		struct Units
		{
			QString temperature;	// celsius, fahrenheit
			QString wind;		// kmh, ms, mph, kn
			QString precipitation;	// mm, inch
		};

		WeatherLib(QObject *parent =0L, const char *name =0L);
		virtual ~WeatherLib();

		/** The forecast API, e.g. of a self-hosted Open-Meteo. */
		void setServer(const QString &url);

		/** "celsius"/"fahrenheit", "kmh"/"ms"/"mph"/"kn",
		    "mm"/"inch", "hPa"/"inHg"; downloads again when they change.
		    Until the new data is there, the old keeps its units. */
		void setUnits(const QString &temperature, const QString &wind,
		              const QString &precipitation, const QString &pressure);

		QString temperature(const QString &id);
		QString apparentTemperature(const QString &id);
		QString dewPoint(const QString &id);
		QString relHumidity(const QString &id);
		QString heatIndex(const QString &id);
		QString windChill(const QString &id);
		QString wind(const QString &id);
		QString pressure(const QString &id);
		QString precipitation(const QString &id);
		QString iconName(const QString &id);
		QString date(const QString &id);
		QString localDate(const QString &id);
		QStringList weather(const QString &id);
		QStringList cover(const QString &id);
		QString sunRise(const QString &id);
		QString sunSet(const QString &id);
		QStringList hourly(const QString &id, int hours);
		QStringList daily(const QString &id);
		QStringList hourlyValues(const QString &id, int hours);
		QStringList dailyValues(const QString &id);
		QString currentTime(const QString &id);
		QStringList dayHours(const QString &id, const QString &date);
		QString rainNextHour(const QString &id);
		QString error(const QString &id);
		bool hasData(const QString &id);

		QStringList stations();

		/**
		 * Takes @p answer, an answer of Open-Meteo for the location
		 * @p id, as if it had been downloaded: canned data, e.g. for
		 * tests.  Current, hourly and daily, or with @p later hourly
		 * and daily of the second model of the hybrid one (ECMWF).
		 * False if it is no good (see error()); the data from before
		 * stays then.  In @p units, by default those of setUnits().
		 */
		bool setAnswer(const QString &id, const QByteArray &answer,
		               bool later = false, const Units *units = 0);
		/** The time is @p t (unix time) from now on, 0: the clock (tests). */
		static void setClock(long t);

		void update(const QString &id);
		void forceUpdate(const QString &id);
		void remove(const QString &id);

	signals:
		void fileUpdating(const QString &id);
		void fileUpdate(const QString &id);
		void stationRemoved(const QString &id);

	private slots:
		void slotDownloadDone(Download *download, const QByteArray &data,
		                      const QString &error);

	private:
		struct Request
		{
			QString id;
			bool current;
			bool hourly;
			bool daily;
			bool later;	// hours after those of the first model
			Units units;
		};

		Data* findData(const QString &id);
		Data* newData(const QString &id);
		QString takeAnswer(Data *d, const Request &r, const QByteArray &answer);
		void getData(Data *d, bool force = false);
		void startJob(Data *d, const QString &model, bool current,
		              bool hourly, bool daily, bool later = false);

		QDict<Data> m_data;
		QString m_server;
		QMap<Download*, Request> m_jobs;
		Units m_units;
		QString m_pressureUnit;
};

#endif
