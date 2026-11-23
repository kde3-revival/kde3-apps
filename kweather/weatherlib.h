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

#include <qdict.h>
#include <qmap.h>
#include <qobject.h>
#include <qstring.h>
#include <qstringlist.h>

namespace KIO
{
	class Job;
}

/**
 * The weather of the locations, from the forecast API of Open-Meteo
 * (https://open-meteo.com/, no account or key).  A location is a key
 * "<latitude>,<longitude>,<model>" (see WeatherModels::locationId());
 * only its coordinates and the model are sent.  The values are returned
 * as text in the units of setUnits().
 */
class WeatherLib : public QObject
{
	Q_OBJECT

	public:
		class Data;

		WeatherLib(QObject *parent =0L, const char *name =0L);
		virtual ~WeatherLib();

		/** The forecast API, e.g. of a self-hosted Open-Meteo. */
		void setServer(const QString &url);

		/** "celsius"/"fahrenheit", "kmh"/"ms"/"mph"/"kn",
		    "mm"/"inch", "hPa"/"inHg"; refetches when they change. */
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
		QStringList weather(const QString &id);
		QStringList cover(const QString &id);
		QString sunRise(const QString &id);
		QString sunSet(const QString &id);
		QStringList hourly(const QString &id, int hours);
		QStringList daily(const QString &id);
		QString rainNextHour(const QString &id);
		QString error(const QString &id);
		bool hasData(const QString &id);

		QStringList stations();

		void update(const QString &id);
		void forceUpdate(const QString &id);
		void remove(const QString &id);

	signals:
		void fileUpdating(const QString &id);
		void fileUpdate(const QString &id);
		void stationRemoved(const QString &id);

	private slots:
		void slotJobDone(KIO::Job*);

	private:
		Data* findData(const QString &id);
		void getData(Data *d, bool force = false);
		void startJob(Data *d, const QString &model, bool current,
		              bool hourly, bool daily);
		QString formatTemperature(double value) const;
		QString formatTime(const Data *d, long time) const;
		QString formatPrecipitation(double value) const;

		QDict<Data> m_data;
		QString m_server;
		QMap<KIO::Job*, QString> m_jobs;
		QString m_tempUnit;
		QString m_windUnit;
		QString m_precipUnit;
		QString m_pressureUnit;
};

#endif
