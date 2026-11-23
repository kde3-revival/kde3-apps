/***************************************************************************
                          weatherlib.cpp  -  description
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

#include "weatherlib.h"

#include <qdatetime.h>
#include <qlocale.h>
#include <qvaluelist.h>
#include <qvariant.h>

#include <kdebug.h>
#include <kglobal.h>
#include <kio/job.h>
#include <klocale.h>
#include <kurl.h>

#include "json.h"
#include "weathericons.h"
#include "weathermodels.h"

#include <math.h>

namespace
{

const char apiUrl[] = "https://api.open-meteo.com/v1/forecast";

// Open-Meteo computes the current weather every 15 minutes.
const int refreshSeconds = 15 * 60;

const double noValue = -1e9;

struct Hour
{
    long time;          // the end of the hour (unix time)
    double temp;
    double precip;      // in the hour that ends at time
    double prob;        // precipitation probability, % (noValue: unknown)
    int code;
    bool day;
};

struct Day
{
    long time;          // midnight, local time of the location (unix time)
    double tmax;
    double tmin;
    double precip;
    double prob;
    long sunrise;
    long sunset;
    int code;
};

bool valid(double v)
{
    return v > noValue / 2;
}

}

class WeatherLib::Data
{
public:
    Data()
        : latitude(0), longitude(0), utcOffset(0), pending(0),
          hasCurrent(false), time(0), temp(noValue), apparent(noValue),
          humidity(noValue), dewPoint(noValue), pressure(noValue),
          windSpeed(noValue), windDirection(noValue), gusts(noValue),
          precip(noValue), cloudCover(noValue), code(-1), day(true)
    {
    }

    QString id;
    double latitude;
    double longitude;
    QString model;
    long utcOffset;

    QDateTime fetched;      // the last complete download
    int pending;            // jobs still running
    QString error;          // of the last download

    bool hasCurrent;
    long time;
    double temp;
    double apparent;
    double humidity;
    double dewPoint;
    double pressure;        // hPa
    double windSpeed;
    double windDirection;
    double gusts;
    double precip;
    double cloudCover;
    int code;
    bool day;

    QValueList<Hour> hours;
    QValueList<Day> days;
};

namespace
{

QValueList<QVariant> array(const QVariant &block, const char *key)
{
    QVariant v = Json::member(block, key);
    if (v.type() == QVariant::List)
        return v.toList();
    return QValueList<QVariant>();
}

double at(const QValueList<QVariant> &list, uint i)
{
    return i < list.count() ? Json::number(list[i], noValue) : noValue;
}

void parseAnswer(WeatherLib::Data *d, const QVariant &root)
{
    d->utcOffset = static_cast<long>(Json::number(Json::member(root, "utc_offset_seconds"), 0));

    QVariant current = Json::member(root, "current");
    if (current.type() == QVariant::Map) {
        d->hasCurrent = true;
        d->time = static_cast<long>(Json::number(Json::member(current, "time"), 0));
        d->temp = Json::number(Json::member(current, "temperature_2m"), noValue);
        d->apparent = Json::number(Json::member(current, "apparent_temperature"), noValue);
        d->humidity = Json::number(Json::member(current, "relative_humidity_2m"), noValue);
        d->dewPoint = Json::number(Json::member(current, "dew_point_2m"), noValue);
        d->pressure = Json::number(Json::member(current, "pressure_msl"), noValue);
        d->windSpeed = Json::number(Json::member(current, "wind_speed_10m"), noValue);
        d->windDirection = Json::number(Json::member(current, "wind_direction_10m"), noValue);
        d->gusts = Json::number(Json::member(current, "wind_gusts_10m"), noValue);
        d->precip = Json::number(Json::member(current, "precipitation"), noValue);
        d->cloudCover = Json::number(Json::member(current, "cloud_cover"), noValue);
        d->code = static_cast<int>(Json::number(Json::member(current, "weather_code"), -1));
        d->day = Json::number(Json::member(current, "is_day"), 1) != 0;
    }

    QVariant hourly = Json::member(root, "hourly");
    if (hourly.type() == QVariant::Map) {
        d->hours.clear();
        QValueList<QVariant> time = array(hourly, "time");
        QValueList<QVariant> temp = array(hourly, "temperature_2m");
        QValueList<QVariant> precip = array(hourly, "precipitation");
        QValueList<QVariant> prob = array(hourly, "precipitation_probability");
        QValueList<QVariant> code = array(hourly, "weather_code");
        QValueList<QVariant> day = array(hourly, "is_day");
        for (uint i = 0; i < time.count(); ++i) {
            Hour h;
            h.time = static_cast<long>(at(time, i));
            h.temp = at(temp, i);
            h.precip = at(precip, i);
            h.prob = at(prob, i);
            double c = at(code, i);
            h.code = valid(c) ? static_cast<int>(c) : -1;
            h.day = at(day, i) != 0;
            if (valid(h.temp))      // ICON-EU ends after 5 days
                d->hours.append(h);
        }
    }

    QVariant daily = Json::member(root, "daily");
    if (daily.type() == QVariant::Map) {
        d->days.clear();
        QValueList<QVariant> time = array(daily, "time");
        QValueList<QVariant> code = array(daily, "weather_code");
        QValueList<QVariant> tmax = array(daily, "temperature_2m_max");
        QValueList<QVariant> tmin = array(daily, "temperature_2m_min");
        QValueList<QVariant> precip = array(daily, "precipitation_sum");
        QValueList<QVariant> prob = array(daily, "precipitation_probability_max");
        QValueList<QVariant> sunrise = array(daily, "sunrise");
        QValueList<QVariant> sunset = array(daily, "sunset");
        for (uint i = 0; i < time.count(); ++i) {
            Day dy;
            dy.time = static_cast<long>(at(time, i));
            double c = at(code, i);
            dy.code = valid(c) ? static_cast<int>(c) : -1;
            dy.tmax = at(tmax, i);
            dy.tmin = at(tmin, i);
            dy.precip = at(precip, i);
            dy.prob = at(prob, i);
            dy.sunrise = static_cast<long>(at(sunrise, i));
            dy.sunset = static_cast<long>(at(sunset, i));
            if (valid(dy.tmax) && valid(dy.tmin))
                d->days.append(dy);
        }
    }
}

long now()
{
    return static_cast<long>(QDateTime::currentDateTime().toTime_t());
}

QString compass(double degrees)
{
    static const char *const names[] = {
        I18N_NOOP("N"), I18N_NOOP("NE"), I18N_NOOP("E"), I18N_NOOP("SE"),
        I18N_NOOP("S"), I18N_NOOP("SW"), I18N_NOOP("W"), I18N_NOOP("NW")
    };
    int i = static_cast<int>(floor((degrees + 22.5) / 45.0)) % 8;
    if (i < 0)
        i += 8;
    return i18n(names[i]);
}

}

WeatherLib::WeatherLib(QObject *parent, const char *name)
	: QObject(parent, name),
	  m_tempUnit("celsius"), m_windUnit("kmh"), m_precipUnit("mm"),
	  m_pressureUnit("hPa")
{
	m_data.setAutoDelete(true);
}

WeatherLib::~WeatherLib()
{
	QMap<KIO::Job*, QString>::Iterator it = m_jobs.begin();
	for (; it != m_jobs.end(); ++it)
		it.key()->kill(true);
}

void WeatherLib::setUnits(const QString &temperature, const QString &wind,
                          const QString &precipitation, const QString &pressure)
{
	bool refetch = temperature != m_tempUnit || wind != m_windUnit
	               || precipitation != m_precipUnit;
	m_tempUnit = temperature;
	m_windUnit = wind;
	m_precipUnit = precipitation;
	m_pressureUnit = pressure;
	QDictIterator<Data> it(m_data);
	for (; it.current(); ++it) {
		if (refetch)
			getData(it.current(), true);
		else
			emit fileUpdate(it.currentKey());
	}
}

WeatherLib::Data* WeatherLib::findData(const QString &id)
{
	Data *d = m_data[id];
	if (!d) {
		double lat, lon;
		QString model;
		if (!WeatherModels::parseLocationId(id, &lat, &lon, &model))
			return 0;
		d = new Data;
		d->id = id;
		d->latitude = lat;
		d->longitude = lon;
		d->model = model;
		m_data.insert(id, d);
		getData(d);
	}
	return d;
}

void WeatherLib::getData(Data *d, bool force)
{
	if (d->pending > 0)
		return;
	if (!force && d->fetched.isValid() && d->error.isEmpty()
	    && d->fetched.secsTo(QDateTime::currentDateTime()) < refreshSeconds) {
		emit fileUpdate(d->id);
		return;
	}
	emit fileUpdating(d->id);
	d->error = QString::null;
	if (d->model == "hybrid") {
		// ICON-EU (7 km, every 3 h) for now and the next hours, the
		// ECMWF IFS HRES (9 km) for the days
		startJob(d, "icon_eu", true, true, false);
		startJob(d, "ecmwf_ifs", false, false, true);
	} else {
		startJob(d, d->model, true, true, true);
	}
}

void WeatherLib::startJob(Data *d, const QString &model, bool current,
                          bool hourly, bool daily)
{
	QLocale c = QLocale::c();
	KURL url(QString::fromLatin1(apiUrl));
	QString query = QString::fromLatin1("latitude=") + c.toString(d->latitude, 'f', 4)
	              + QString::fromLatin1("&longitude=") + c.toString(d->longitude, 'f', 4)
	              + QString::fromLatin1("&models=") + model
	              + QString::fromLatin1("&timezone=auto&timeformat=unixtime&forecast_days=7");
	if (m_tempUnit == "fahrenheit")
		query += QString::fromLatin1("&temperature_unit=fahrenheit");
	if (m_windUnit != "kmh")
		query += QString::fromLatin1("&wind_speed_unit=") + m_windUnit;
	if (m_precipUnit == "inch")
		query += QString::fromLatin1("&precipitation_unit=inch");
	if (current)
		query += QString::fromLatin1("&current=temperature_2m,relative_humidity_2m,"
			"apparent_temperature,is_day,precipitation,weather_code,cloud_cover,"
			"pressure_msl,wind_speed_10m,wind_direction_10m,wind_gusts_10m,dew_point_2m");
	if (hourly)
		query += QString::fromLatin1("&hourly=temperature_2m,precipitation_probability,"
			"precipitation,weather_code,is_day");
	if (daily)
		query += QString::fromLatin1("&daily=weather_code,temperature_2m_max,"
			"temperature_2m_min,precipitation_sum,precipitation_probability_max,"
			"sunrise,sunset");
	url.setQuery(query);

	kdDebug(12006) << "Fetching " << url.url() << endl;
	KIO::Job *job = KIO::storedGet(url, true, false);
	job->addMetaData("cache", "reload");
	connect(job, SIGNAL(result(KIO::Job*)), SLOT(slotJobDone(KIO::Job*)));
	m_jobs.insert(job, d->id);
	++d->pending;
}

void WeatherLib::slotJobDone(KIO::Job *job)
{
	QMap<KIO::Job*, QString>::Iterator it = m_jobs.find(job);
	if (it == m_jobs.end())
		return;
	QString id = *it;
	m_jobs.remove(it);
	Data *d = m_data[id];
	if (!d)		// removed meanwhile
		return;
	--d->pending;

	if (job->error()) {
		d->error = job->errorString();
	} else {
		QByteArray answer = static_cast<KIO::StoredTransferJob*>(job)->data();
		bool ok = false;
		QVariant root = Json::parse(answer, &ok);
		if (!ok || root.type() != QVariant::Map) {
			d->error = i18n("The answer of Open-Meteo could not be read.");
		} else if (Json::member(root, "error").toBool()) {
			d->error = Json::member(root, "reason").toString();
		} else {
			parseAnswer(d, root);
		}
	}
	if (!d->error.isEmpty())
		kdDebug(12006) << "Open-Meteo, " << id << ": " << d->error << endl;

	if (d->pending == 0) {
		if (d->error.isEmpty())
			d->fetched = QDateTime::currentDateTime();
		emit fileUpdate(id);
	}
}

QString WeatherLib::formatTemperature(double value) const
{
	if (!valid(value))
		return QString::null;
	QString unit = m_tempUnit == "fahrenheit" ? QString::fromUtf8("\xc2\xb0""F")
	                                          : QString::fromUtf8("\xc2\xb0""C");
	// no "-0"
	long rounded = lround(value);
	return QString::number(rounded) + unit;
}

QString WeatherLib::formatPrecipitation(double value) const
{
	if (!valid(value))
		return QString::null;
	if (m_precipUnit == "inch")
		return KGlobal::locale()->formatNumber(value, 2) + i18n(" in");
	return KGlobal::locale()->formatNumber(value, 1) + i18n(" mm");
}

QString WeatherLib::formatTime(const Data *d, long time) const
{
	// the local time of the location
	QDateTime dt;
	dt.setTime_t(static_cast<uint>(time + d->utcOffset), Qt::UTC);
	return KGlobal::locale()->formatTime(dt.time());
}

QString WeatherLib::temperature(const QString &id)
{
	Data *d = findData(id);
	return d && d->hasCurrent ? formatTemperature(d->temp) : QString::null;
}

QString WeatherLib::apparentTemperature(const QString &id)
{
	Data *d = findData(id);
	return d && d->hasCurrent ? formatTemperature(d->apparent) : QString::null;
}

QString WeatherLib::dewPoint(const QString &id)
{
	Data *d = findData(id);
	return d && d->hasCurrent ? formatTemperature(d->dewPoint) : QString::null;
}

QString WeatherLib::relHumidity(const QString &id)
{
	Data *d = findData(id);
	if (!d || !d->hasCurrent || !valid(d->humidity))
		return QString::null;
	return i18n("%1 %").arg(lround(d->humidity));
}

QString WeatherLib::heatIndex(const QString &id)
{
	// the apparent temperature, when it is higher than the real one
	Data *d = findData(id);
	if (!d || !d->hasCurrent || !valid(d->apparent) || !valid(d->temp)
	    || lround(d->apparent) <= lround(d->temp))
		return QString::null;
	return formatTemperature(d->apparent);
}

QString WeatherLib::windChill(const QString &id)
{
	// the apparent temperature, when it is lower than the real one
	Data *d = findData(id);
	if (!d || !d->hasCurrent || !valid(d->apparent) || !valid(d->temp)
	    || lround(d->apparent) >= lround(d->temp))
		return QString::null;
	return formatTemperature(d->apparent);
}

QString WeatherLib::wind(const QString &id)
{
	Data *d = findData(id);
	if (!d || !d->hasCurrent || !valid(d->windSpeed))
		return QString::null;
	QString unit;
	if (m_windUnit == "ms")
		unit = i18n("m/s");
	else if (m_windUnit == "mph")
		unit = i18n("mph");
	else if (m_windUnit == "kn")
		unit = i18n("kt");
	else
		unit = i18n("km/h");
	long speed = lround(d->windSpeed);
	if (speed == 0)
		return i18n("calm");
	QString text = valid(d->windDirection)
		? i18n("wind direction, speed and unit", "%1 %2 %3")
			.arg(compass(d->windDirection)).arg(speed).arg(unit)
		: i18n("wind speed and unit", "%1 %2").arg(speed).arg(unit);
	if (valid(d->gusts) && lround(d->gusts) >= speed + 10)
		text += i18n(", gusts %1").arg(lround(d->gusts));
	return text;
}

QString WeatherLib::pressure(const QString &id)
{
	Data *d = findData(id);
	if (!d || !d->hasCurrent || !valid(d->pressure))
		return QString::null;
	if (m_pressureUnit == "inHg")
		return i18n("%1 inHg").arg(KGlobal::locale()->formatNumber(d->pressure * 0.0295300, 2));
	return i18n("%1 hPa").arg(lround(d->pressure));
}

QString WeatherLib::precipitation(const QString &id)
{
	Data *d = findData(id);
	return d && d->hasCurrent ? formatPrecipitation(d->precip) : QString::null;
}

QString WeatherLib::iconName(const QString &id)
{
	Data *d = findData(id);
	if (!d || !d->hasCurrent)
		return WeatherIcons::iconName(-1, true);
	return WeatherIcons::iconName(d->code, d->day);
}

QString WeatherLib::date(const QString &id)
{
	Data *d = findData(id);
	if (!d || !d->hasCurrent)
		return QString::null;
	QDateTime dt;
	dt.setTime_t(static_cast<uint>(d->time + d->utcOffset), Qt::UTC);
	return KGlobal::locale()->formatDateTime(dt, true);
}

QStringList WeatherLib::weather(const QString &id)
{
	Data *d = findData(id);
	QStringList list;
	if (d && d->hasCurrent)
		list.append(WeatherIcons::description(d->code));
	return list;
}

QStringList WeatherLib::cover(const QString &id)
{
	Data *d = findData(id);
	QStringList list;
	if (d && d->hasCurrent && valid(d->cloudCover))
		list.append(i18n("Cloud cover %1 %").arg(lround(d->cloudCover)));
	return list;
}

QString WeatherLib::sunRise(const QString &id)
{
	Data *d = findData(id);
	if (!d || d->days.isEmpty() || d->days.first().sunrise == 0)
		return QString::null;
	return formatTime(d, d->days.first().sunrise);
}

QString WeatherLib::sunSet(const QString &id)
{
	Data *d = findData(id);
	if (!d || d->days.isEmpty() || d->days.first().sunset == 0)
		return QString::null;
	return formatTime(d, d->days.first().sunset);
}

QStringList WeatherLib::hourly(const QString &id, int hours)
{
	QStringList list;
	Data *d = findData(id);
	if (!d)
		return list;
	long t = now();
	QValueList<Hour>::ConstIterator it = d->hours.begin();
	for (; it != d->hours.end() && int(list.count()) < hours; ++it) {
		const Hour &h = *it;
		if (h.time <= t)
			continue;
		QDateTime dt;
		dt.setTime_t(static_cast<uint>(h.time + d->utcOffset), Qt::UTC);
		QString precip = valid(h.precip) && h.precip >= 0.05
			? formatPrecipitation(h.precip) : QString::null;
		QString prob = valid(h.prob) ? i18n("%1 %").arg(lround(h.prob)) : QString::null;
		list.append(dt.toString(Qt::ISODate) + '\t'
		            + WeatherIcons::iconName(h.code, h.day) + '\t'
		            + formatTemperature(h.temp) + '\t' + precip + '\t' + prob + '\t'
		            + WeatherIcons::description(h.code));
	}
	return list;
}

QStringList WeatherLib::daily(const QString &id)
{
	QStringList list;
	Data *d = findData(id);
	if (!d)
		return list;
	QValueList<Day>::ConstIterator it = d->days.begin();
	for (; it != d->days.end(); ++it) {
		const Day &dy = *it;
		QDateTime dt;
		dt.setTime_t(static_cast<uint>(dy.time + d->utcOffset), Qt::UTC);
		QString precip = valid(dy.precip) && dy.precip >= 0.05
			? formatPrecipitation(dy.precip) : QString::null;
		QString prob = valid(dy.prob) ? i18n("%1 %").arg(lround(dy.prob)) : QString::null;
		list.append(dt.date().toString(Qt::ISODate) + '\t'
		            + WeatherIcons::iconName(dy.code, true) + '\t'
		            + formatTemperature(dy.tmax) + '\t' + formatTemperature(dy.tmin) + '\t'
		            + precip + '\t' + prob + '\t' + WeatherIcons::description(dy.code));
	}
	return list;
}

QString WeatherLib::rainNextHour(const QString &id)
{
	Data *d = findData(id);
	if (!d)
		return QString::null;
	long t = now();
	// the amount of an hour is given at its end: the next hour ends
	// within the coming 60 minutes or right after
	QValueList<Hour>::ConstIterator it = d->hours.begin();
	for (; it != d->hours.end(); ++it) {
		const Hour &h = *it;
		if (h.time <= t)
			continue;
		double threshold = m_precipUnit == "inch" ? 0.004 : 0.1;
		if (valid(h.precip) && h.precip >= threshold)
			return formatPrecipitation(h.precip);
		break;
	}
	return QString::null;
}

QString WeatherLib::error(const QString &id)
{
	Data *d = findData(id);
	return d ? d->error : i18n("Unknown location");
}

bool WeatherLib::hasData(const QString &id)
{
	Data *d = findData(id);
	return d && d->hasCurrent;
}

QStringList WeatherLib::stations()
{
	QStringList list;
	QDictIterator<Data> it(m_data);
	for (; it.current(); ++it)
		list.append(it.currentKey());
	return list;
}

void WeatherLib::update(const QString &id)
{
	Data *d = findData(id);
	if (d)
		getData(d);
}

void WeatherLib::forceUpdate(const QString &id)
{
	Data *d = findData(id);
	if (d)
		getData(d, true);
}

void WeatherLib::remove(const QString &id)
{
	if (!m_data[id])
		return;
	QMap<KIO::Job*, QString>::Iterator it = m_jobs.begin();
	while (it != m_jobs.end()) {
		QMap<KIO::Job*, QString>::Iterator next = it;
		++next;
		if (*it == id) {
			it.key()->kill(true);
			m_jobs.remove(it);
		}
		it = next;
	}
	m_data.remove(id);
	emit stationRemoved(id);
}

#include "weatherlib.moc"
