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
#include <klocale.h>
#include <kurl.h>

#include "download.h"
#include "json.h"
#include "localtime.h"
#include "weathericons.h"
#include "weathermodels.h"
#include "weatherservers.h"

#include <math.h>

namespace
{

// Open-Meteo computes the current weather every 15 minutes.
const int refreshSeconds = 15 * 60;

// A forecast of 7 days is about 15 KiB.
const uint maxAnswer = 1024 * 1024;
const int timeoutSeconds = 60;

const double noValue = -1e9;

bool valid(double v)
{
    return v > noValue / 2;
}

/** @p v if it is a finite number in [lo, hi], else noValue. */
double number(const QVariant &v, double lo, double hi)
{
    if (v.type() != QVariant::Double)
        return noValue;
    double d = v.toDouble();
    if (!isfinite(d) || d < lo || d > hi)
        return noValue;
    return d;
}

/** A unix time (up to 2100), 0 if missing or out of range. */
long timeOf(const QVariant &v)
{
    double d = number(v, 1.0, 4102444800.0);
    return valid(d) ? static_cast<long>(d) : 0;
}

/** A WMO weather code, -1 if missing or out of range. */
int codeOf(const QVariant &v)
{
    double d = number(v, 0, 99);
    return valid(d) ? static_cast<int>(d) : -1;
}

// generous ranges, in any of the units
const double tLo = -300, tHi = 400;
const double windHi = 2000, precipHi = 5000;

struct Current
{
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
};

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
    long sunrise;       // 0: none (polar day or night) or unknown
    long sunset;
    int code;
};

QValueList<QVariant> array(const QVariant &block, const char *key)
{
    QVariant v = Json::member(block, key);
    if (v.type() == QVariant::List)
        return v.toList();
    return QValueList<QVariant>();
}

QVariant at(const QValueList<QVariant> &list, uint i)
{
    return i < list.count() ? list[i] : QVariant();
}

bool parseCurrent(const QVariant &root, Current *c)
{
    QVariant cur = Json::member(root, "current");
    if (cur.type() != QVariant::Map)
        return false;
    c->time = timeOf(Json::member(cur, "time"));
    c->temp = number(Json::member(cur, "temperature_2m"), tLo, tHi);
    if (c->time == 0 || !valid(c->temp))
        return false;
    c->apparent = number(Json::member(cur, "apparent_temperature"), tLo, tHi);
    c->humidity = number(Json::member(cur, "relative_humidity_2m"), 0, 100);
    c->dewPoint = number(Json::member(cur, "dew_point_2m"), tLo, tHi);
    c->pressure = number(Json::member(cur, "pressure_msl"), 300, 1200);
    c->windSpeed = number(Json::member(cur, "wind_speed_10m"), 0, windHi);
    c->windDirection = number(Json::member(cur, "wind_direction_10m"), 0, 360);
    c->gusts = number(Json::member(cur, "wind_gusts_10m"), 0, windHi);
    c->precip = number(Json::member(cur, "precipitation"), 0, precipHi);
    c->cloudCover = number(Json::member(cur, "cloud_cover"), 0, 100);
    c->code = codeOf(Json::member(cur, "weather_code"));
    c->day = number(Json::member(cur, "is_day"), 0, 1) != 0;
    return true;
}

bool parseHours(const QVariant &root, QValueList<Hour> *hours)
{
    QVariant hourly = Json::member(root, "hourly");
    if (hourly.type() != QVariant::Map)
        return false;
    QValueList<QVariant> time = array(hourly, "time");
    QValueList<QVariant> temp = array(hourly, "temperature_2m");
    if (time.isEmpty() || temp.count() != time.count())
        return false;
    QValueList<QVariant> precip = array(hourly, "precipitation");
    QValueList<QVariant> prob = array(hourly, "precipitation_probability");
    QValueList<QVariant> code = array(hourly, "weather_code");
    QValueList<QVariant> day = array(hourly, "is_day");
    hours->clear();
    long last = 0;
    for (uint i = 0; i < time.count(); ++i) {
        Hour h;
        h.time = timeOf(time[i]);
        h.temp = number(temp[i], tLo, tHi);
        // ICON-EU ends after 5 days: null temperatures
        if (h.time <= last || !valid(h.temp))
            continue;
        last = h.time;
        h.precip = number(at(precip, i), 0, precipHi);
        h.prob = number(at(prob, i), 0, 100);
        h.code = codeOf(at(code, i));
        h.day = number(at(day, i), 0, 1) != 0;
        hours->append(h);
    }
    return !hours->isEmpty();
}

bool parseDays(const QVariant &root, QValueList<Day> *days)
{
    QVariant daily = Json::member(root, "daily");
    if (daily.type() != QVariant::Map)
        return false;
    QValueList<QVariant> time = array(daily, "time");
    QValueList<QVariant> tmax = array(daily, "temperature_2m_max");
    QValueList<QVariant> tmin = array(daily, "temperature_2m_min");
    if (time.isEmpty() || tmax.count() != time.count() || tmin.count() != time.count())
        return false;
    QValueList<QVariant> code = array(daily, "weather_code");
    QValueList<QVariant> precip = array(daily, "precipitation_sum");
    QValueList<QVariant> prob = array(daily, "precipitation_probability_max");
    QValueList<QVariant> sunrise = array(daily, "sunrise");
    QValueList<QVariant> sunset = array(daily, "sunset");
    days->clear();
    long last = 0;
    for (uint i = 0; i < time.count(); ++i) {
        Day dy;
        dy.time = timeOf(time[i]);
        dy.tmax = number(tmax[i], tLo, tHi);
        dy.tmin = number(tmin[i], tLo, tHi);
        if (dy.time <= last || !valid(dy.tmax) || !valid(dy.tmin))
            continue;
        last = dy.time;
        dy.code = codeOf(at(code, i));
        dy.precip = number(at(precip, i), 0, precipHi);
        dy.prob = number(at(prob, i), 0, 100);
        dy.sunrise = timeOf(at(sunrise, i));
        dy.sunset = timeOf(at(sunset, i));
        days->append(dy);
    }
    return !days->isEmpty();
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

long lround0(double v)
{
    // no "-0"
    long r = lround(v);
    return r == 0 ? 0 : r;
}

QString formatTemperature(double value, const WeatherLib::Units &u)
{
    if (!valid(value))
        return QString::null;
    QString unit = u.temperature == "fahrenheit" ? QString::fromUtf8("\xc2\xb0""F")
                                                 : QString::fromUtf8("\xc2\xb0""C");
    return QString::number(lround0(value)) + unit;
}

/** The amount in millimeters, whatever the unit. */
double millimeters(double value, const WeatherLib::Units &u)
{
    return u.precipitation == "inch" ? value * 25.4 : value;
}

QString formatPrecipitation(double value, const WeatherLib::Units &u)
{
    if (!valid(value))
        return QString::null;
    KLocale *l = KGlobal::locale();
    if (u.precipitation == "inch") {
        if (value > 0 && value < 0.01)
            return i18n("less than an amount", "< %1").arg(l->formatNumber(0.01, 2)) + i18n(" in");
        return l->formatNumber(value, 2) + i18n(" in");
    }
    if (value > 0 && value < 0.1)
        return i18n("less than an amount", "< %1").arg(l->formatNumber(0.1, 1)) + i18n(" mm");
    return l->formatNumber(value, 1) + i18n(" mm");
}

/** Precipitation worth showing in a forecast: 0.05 mm or more. */
QString shownPrecipitation(double value, const WeatherLib::Units &u)
{
    if (!valid(value) || millimeters(value, u) < 0.05)
        return QString::null;
    return formatPrecipitation(value, u);
}

QString percent(double value)
{
    return valid(value) ? i18n("%1 %").arg(lround0(value)) : QString::null;
}

}

class WeatherLib::Data
{
public:
    Data()
        : latitude(0), longitude(0), utcOffset(0), pending(0), again(false),
          hasCurrent(false)
    {
    }

    QString id;
    double latitude;
    double longitude;
    QString model;
    QString zone;           // IANA name, empty if unknown
    long utcOffset;         // at the time of the answer

    QDateTime fetched;      // the last complete download
    int pending;            // downloads still running
    bool again;             // download again when they are done
    QString error;          // of the last download

    // each part keeps the units it was downloaded in
    bool hasCurrent;
    Current current;
    WeatherLib::Units currentUnits;
    QValueList<Hour> hours;
    WeatherLib::Units hourUnits;
    QValueList<Day> days;
    WeatherLib::Units dayUnits;

    QDateTime local(long t) const
    {
        return LocalTime::at(t, zone, utcOffset);
    }
};

WeatherLib::WeatherLib(QObject *parent, const char *name)
	: QObject(parent, name),
	  m_server(WeatherServers::forecast()), m_pressureUnit("hPa")
{
	m_data.setAutoDelete(true);
	m_units.temperature = "celsius";
	m_units.wind = "kmh";
	m_units.precipitation = "mm";
}

WeatherLib::~WeatherLib()
{
	QMap<Download*, Request>::Iterator it = m_jobs.begin();
	for (; it != m_jobs.end(); ++it)
		it.key()->abort();
}

void WeatherLib::setServer(const QString &url)
{
	m_server = url.isEmpty() ? WeatherServers::forecast() : url;
}

void WeatherLib::setUnits(const QString &temperature, const QString &wind,
                          const QString &precipitation, const QString &pressure)
{
	bool refetch = temperature != m_units.temperature || wind != m_units.wind
	               || precipitation != m_units.precipitation;
	m_units.temperature = temperature;
	m_units.wind = wind;
	m_units.precipitation = precipitation;
	m_pressureUnit = pressure;	// converted here, not by Open-Meteo
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
	if (d->pending > 0) {
		// e.g. new units: the running download asked for the old ones
		if (force)
			d->again = true;
		return;
	}
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
	KURL url(m_server);
	QString query = QString::fromLatin1("latitude=") + c.toString(d->latitude, 'f', 4)
	              + QString::fromLatin1("&longitude=") + c.toString(d->longitude, 'f', 4)
	              + QString::fromLatin1("&models=") + model
	              + QString::fromLatin1("&timezone=auto&timeformat=unixtime&forecast_days=7");
	if (m_units.temperature == "fahrenheit")
		query += QString::fromLatin1("&temperature_unit=fahrenheit");
	if (m_units.wind != "kmh")
		query += QString::fromLatin1("&wind_speed_unit=") + m_units.wind;
	if (m_units.precipitation == "inch")
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

	Download *download = new Download(url, maxAnswer, timeoutSeconds, this);
	connect(download, SIGNAL(finished(Download*, const QByteArray&, const QString&)),
	        SLOT(slotDownloadDone(Download*, const QByteArray&, const QString&)));
	Request r;
	r.id = d->id;
	r.current = current;
	r.hourly = hourly;
	r.daily = daily;
	r.units = m_units;
	m_jobs.insert(download, r);
	++d->pending;
}

void WeatherLib::slotDownloadDone(Download *download, const QByteArray &answer,
                                  const QString &downloadError)
{
	QMap<Download*, Request>::Iterator it = m_jobs.find(download);
	if (it == m_jobs.end())
		return;
	Request r = *it;
	m_jobs.remove(it);
	Data *d = m_data[r.id];
	if (!d)		// removed meanwhile
		return;
	--d->pending;

	QString error = downloadError;
	if (error.isEmpty()) {
		bool ok = false;
		QVariant root = Json::parse(answer, &ok);
		QVariant err = Json::member(root, "error");
		Current current;
		QValueList<Hour> hours;
		QValueList<Day> days;
		if (!ok || root.type() != QVariant::Map) {
			error = i18n("The answer of Open-Meteo could not be read.");
		} else if (err.type() == QVariant::Bool && err.toBool()) {
			error = Json::member(root, "reason").toString().stripWhiteSpace().left(200);
			if (error.isEmpty())
				error = i18n("Open-Meteo refused the request.");
		} else if ((r.current && !parseCurrent(root, &current))
		           || (r.hourly && !parseHours(root, &hours))
		           || (r.daily && !parseDays(root, &days))) {
			error = i18n("The answer of Open-Meteo is incomplete.");
		} else {
			// all requested parts are good: take them together
			QString zone = Json::member(root, "timezone").toString();
			d->zone = LocalTime::isZone(zone) ? zone : QString::null;
			double offset = number(Json::member(root, "utc_offset_seconds"), -18 * 3600, 18 * 3600);
			d->utcOffset = valid(offset) ? static_cast<long>(offset) : 0;
			if (r.current) {
				d->current = current;
				d->currentUnits = r.units;
				d->hasCurrent = true;
			}
			if (r.hourly) {
				d->hours = hours;
				d->hourUnits = r.units;
			}
			if (r.daily) {
				d->days = days;
				d->dayUnits = r.units;
			}
		}
	}
	if (!error.isEmpty()) {
		kdDebug(12006) << "Open-Meteo, " << r.id << ": " << error << endl;
		if (d->error.isEmpty())
			d->error = error;
	}

	if (d->pending == 0) {
		if (d->error.isEmpty())
			d->fetched = QDateTime::currentDateTime();
		if (d->again) {
			d->again = false;
			getData(d, true);
			return;
		}
		emit fileUpdate(r.id);
	}
}

QString WeatherLib::temperature(const QString &id)
{
	Data *d = findData(id);
	return d && d->hasCurrent ? formatTemperature(d->current.temp, d->currentUnits) : QString::null;
}

QString WeatherLib::apparentTemperature(const QString &id)
{
	Data *d = findData(id);
	return d && d->hasCurrent ? formatTemperature(d->current.apparent, d->currentUnits) : QString::null;
}

QString WeatherLib::dewPoint(const QString &id)
{
	Data *d = findData(id);
	return d && d->hasCurrent ? formatTemperature(d->current.dewPoint, d->currentUnits) : QString::null;
}

QString WeatherLib::relHumidity(const QString &id)
{
	Data *d = findData(id);
	return d && d->hasCurrent ? percent(d->current.humidity) : QString::null;
}

QString WeatherLib::heatIndex(const QString &id)
{
	// the apparent temperature, when it is higher than the real one
	Data *d = findData(id);
	if (!d || !d->hasCurrent || !valid(d->current.apparent)
	    || lround0(d->current.apparent) <= lround0(d->current.temp))
		return QString::null;
	return formatTemperature(d->current.apparent, d->currentUnits);
}

QString WeatherLib::windChill(const QString &id)
{
	// the apparent temperature, when it is lower than the real one
	Data *d = findData(id);
	if (!d || !d->hasCurrent || !valid(d->current.apparent)
	    || lround0(d->current.apparent) >= lround0(d->current.temp))
		return QString::null;
	return formatTemperature(d->current.apparent, d->currentUnits);
}

QString WeatherLib::wind(const QString &id)
{
	Data *d = findData(id);
	if (!d || !d->hasCurrent || !valid(d->current.windSpeed))
		return QString::null;
	const QString &w = d->currentUnits.wind;
	QString unit;
	if (w == "ms")
		unit = i18n("m/s");
	else if (w == "mph")
		unit = i18n("mph");
	else if (w == "kn")
		unit = i18n("kt");
	else
		unit = i18n("km/h");
	long speed = lround0(d->current.windSpeed);
	if (speed == 0)
		return i18n("calm");
	QString text = valid(d->current.windDirection)
		? i18n("wind direction, speed and unit", "%1 %2 %3")
			.arg(compass(d->current.windDirection)).arg(speed).arg(unit)
		: i18n("wind speed and unit", "%1 %2").arg(speed).arg(unit);
	if (valid(d->current.gusts) && lround0(d->current.gusts) >= speed + 10)
		text += i18n(", gusts %1").arg(lround0(d->current.gusts));
	return text;
}

QString WeatherLib::pressure(const QString &id)
{
	Data *d = findData(id);
	if (!d || !d->hasCurrent || !valid(d->current.pressure))
		return QString::null;
	if (m_pressureUnit == "inHg")
		return i18n("%1 inHg").arg(KGlobal::locale()->formatNumber(d->current.pressure * 0.0295300, 2));
	return i18n("%1 hPa").arg(lround0(d->current.pressure));
}

QString WeatherLib::precipitation(const QString &id)
{
	Data *d = findData(id);
	return d && d->hasCurrent ? formatPrecipitation(d->current.precip, d->currentUnits) : QString::null;
}

QString WeatherLib::iconName(const QString &id)
{
	Data *d = findData(id);
	if (!d || !d->hasCurrent)
		return WeatherIcons::iconName(-1, true);
	return WeatherIcons::iconName(d->current.code, d->current.day);
}

QString WeatherLib::date(const QString &id)
{
	Data *d = findData(id);
	if (!d || !d->hasCurrent)
		return QString::null;
	return KGlobal::locale()->formatDateTime(d->local(d->current.time), true);
}

QString WeatherLib::localDate(const QString &id)
{
	Data *d = findData(id);
	if (!d || (!d->hasCurrent && d->days.isEmpty() && d->hours.isEmpty()))
		return QString::null;
	return d->local(now()).date().toString(Qt::ISODate);
}

QStringList WeatherLib::weather(const QString &id)
{
	Data *d = findData(id);
	QStringList list;
	if (d && d->hasCurrent)
		list.append(WeatherIcons::description(d->current.code));
	return list;
}

QStringList WeatherLib::cover(const QString &id)
{
	Data *d = findData(id);
	QStringList list;
	if (d && d->hasCurrent && valid(d->current.cloudCover))
		list.append(i18n("Cloud cover %1 %").arg(lround0(d->current.cloudCover)));
	return list;
}

QString WeatherLib::sunRise(const QString &id)
{
	// the sunrise of today at the location: none in the polar night
	Data *d = findData(id);
	if (!d)
		return QString::null;
	QDate today = d->local(now()).date();
	QValueList<Day>::ConstIterator it = d->days.begin();
	for (; it != d->days.end(); ++it)
		if (d->local((*it).time + 12 * 3600).date() == today)
			return (*it).sunrise ? KGlobal::locale()->formatTime(d->local((*it).sunrise).time())
			                     : QString::null;
	return QString::null;
}

QString WeatherLib::sunSet(const QString &id)
{
	Data *d = findData(id);
	if (!d)
		return QString::null;
	QDate today = d->local(now()).date();
	QValueList<Day>::ConstIterator it = d->days.begin();
	for (; it != d->days.end(); ++it)
		if (d->local((*it).time + 12 * 3600).date() == today)
			return (*it).sunset ? KGlobal::locale()->formatTime(d->local((*it).sunset).time())
			                    : QString::null;
	return QString::null;
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
		list.append(d->local(h.time).toString(Qt::ISODate) + '\t'
		            + WeatherIcons::iconName(h.code, h.day) + '\t'
		            + formatTemperature(h.temp, d->hourUnits) + '\t'
		            + shownPrecipitation(h.precip, d->hourUnits) + '\t'
		            + percent(h.prob) + '\t'
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
	QDate today = d->local(now()).date();
	QValueList<Day>::ConstIterator it = d->days.begin();
	for (; it != d->days.end(); ++it) {
		const Day &dy = *it;
		// midnight at the location; noon is safe from any shift
		QDate date = d->local(dy.time + 12 * 3600).date();
		if (date < today)	// an old download
			continue;
		list.append(date.toString(Qt::ISODate) + '\t'
		            + WeatherIcons::iconName(dy.code, true) + '\t'
		            + formatTemperature(dy.tmax, d->dayUnits) + '\t'
		            + formatTemperature(dy.tmin, d->dayUnits) + '\t'
		            + shownPrecipitation(dy.precip, d->dayUnits) + '\t'
		            + percent(dy.prob) + '\t' + WeatherIcons::description(dy.code));
	}
	return list;
}

QString WeatherLib::rainNextHour(const QString &id)
{
	Data *d = findData(id);
	if (!d)
		return QString::null;
	// The amount of an hour is given at its end, so the hours ending
	// at T in (now, now + 2 h) overlap the coming 60 minutes.
	long t = now();
	double most = noValue;
	QValueList<Hour>::ConstIterator it = d->hours.begin();
	for (; it != d->hours.end(); ++it) {
		const Hour &h = *it;
		if (h.time <= t)
			continue;
		if (h.time >= t + 2 * 3600)
			break;
		if (valid(h.precip) && h.precip > most)
			most = h.precip;
	}
	if (!valid(most) || millimeters(most, d->hourUnits) < 0.1)
		return QString::null;
	return formatPrecipitation(most, d->hourUnits);
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
	QMap<Download*, Request>::Iterator it = m_jobs.begin();
	while (it != m_jobs.end()) {
		QMap<Download*, Request>::Iterator next = it;
		++next;
		if ((*it).id == id) {
			it.key()->abort();
			m_jobs.remove(it);
		}
		it = next;
	}
	m_data.remove(id);
	emit stationRemoved(id);
}

#include "weatherlib.moc"
