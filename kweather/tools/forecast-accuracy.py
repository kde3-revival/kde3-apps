#!/usr/bin/env python3
#
#   forecast-accuracy.py - how well did the Open-Meteo models forecast the
#   weather at a synoptic station in the past weeks?
#
#   Copyright (C) 2026 The KDE3 revival authors
#
#   This program is free software; you can redistribute it and/or modify
#   it under the terms of the GNU General Public License as published by
#   the Free Software Foundation; either version 2 of the License, or
#   (at your option) any later version.
#
# The forecasts come from the Open-Meteo Previous Runs API: for every hour,
# <variable>_previous_dayN is the value forecast by the run of N days
# earlier, so N is the lead time in days (day 0: the latest run, lead
# 0-24 h).  The observations are the hourly SYNOP reports of a WMO
# station, from the OGIMET archive: the temperature of section 1 (group
# 1snTTT) and the precipitation of the last hour of section 3 (group
# 6RRR5).  Station observations are independent of all models; the ERA5
# reanalysis is not (it is made with the ECMWF model).
#
# Only the station coordinates are sent to Open-Meteo.
#
# Usage: forecast-accuracy.py --station 15420 --lat 44.50 --lon 26.13
#        [--days 28] [--models ecmwf_ifs,icon_eu,best_match]

import argparse
import datetime as dt
import json
import sys
import time
import urllib.parse
import urllib.request

PREVIOUS_RUNS = "https://previous-runs-api.open-meteo.com/v1/forecast"
OGIMET = "https://www.ogimet.com/cgi-bin/getsynop"
USER_AGENT = "kweather-forecast-accuracy/1.0"
MAX_LEAD = 7


def fetch(url, params):
    full = url + "?" + urllib.parse.urlencode(params)
    req = urllib.request.Request(full, headers={"User-Agent": USER_AGENT})
    for attempt in range(4):
        try:
            with urllib.request.urlopen(req, timeout=60) as resp:
                return resp.read().decode("utf-8", "replace")
        except OSError as err:
            if attempt == 3:
                raise
            print("  retrying (%s)" % err, file=sys.stderr)
            time.sleep(10 * (attempt + 1))
    return ""


# --- observations -----------------------------------------------------

def synop_groups(report):
    """Section 1 and section 3 groups of an AAXX report."""
    body = report.rstrip("=").split()
    try:
        start = body.index("AAXX") + 3       # AAXX YYGGi IIiii
    except ValueError:
        return [], []
    body = body[start:]
    sec1, sec3, cur = [], [], None
    cur = sec1
    for g in body:
        if g == "333":
            cur = sec3
            continue
        if g in ("222", "444", "555") or g.startswith("222"):
            cur = None
            continue
        if cur is not None:
            cur.append(g)
    return sec1, sec3


def synop_precip(rrr):
    """RRR of a 6RRRt group in mm (None: not reported)."""
    if not rrr.isdigit():
        return None
    v = int(rrr)
    if v == 990:
        return 0.0                           # trace
    if v > 990:
        return (v - 990) / 10.0
    return float(v)


def parse_synop(text):
    """{datetime (UTC): (temperature or None, 1 h precipitation or None)}
    and {datetime (UTC): 24 h precipitation up to that hour}"""
    obs, daily = {}, {}
    for line in text.splitlines():
        parts = line.split(",", 6)
        if len(parts) < 7 or not parts[1].isdigit():
            continue
        when = dt.datetime(int(parts[1]), int(parts[2]), int(parts[3]),
                           int(parts[4]), int(parts[5]))
        if when.minute != 0:
            continue
        sec1, sec3 = synop_groups(parts[6])
        temp = None
        # 1snTTT follows iRiXhVV and Nddff (and 00fff when ff > 99)
        for g in sec1[2:]:
            if len(g) == 5 and g[0] == "1" and g[1] in "01" and g[2:].isdigit():
                temp = int(g[2:]) / 10.0 * (-1 if g[1] == "1" else 1)
                break
        precip = None
        # iR (first digit of the first group): 3 = no precipitation, so
        # the 6RRRt groups are left out because the amount is zero.
        ir = sec1[0][0] if sec1 and sec1[0] else "/"
        for g in sec3:
            if len(g) == 5 and g[0] == "6" and g[4] == "5":
                precip = synop_precip(g[1:4])
                break
        if precip is None and ir == "3":
            precip = 0.0
        obs[when] = (temp, precip)
        # 7RRRR of section 3: the last 24 h, in 0.1 mm (9999: trace)
        for g in sec3:
            if len(g) == 5 and g[0] == "7" and g[1:].isdigit():
                daily[when] = 0.0 if g[1:] == "9999" else int(g[1:]) / 10.0
                break
        else:
            if ir == "3" and when.hour == 6:
                daily[when] = 0.0
    return obs, daily


def fetch_observations(station, start, end):
    obs, daily = {}, {}
    day = start
    while day <= end:
        chunk_end = min(day + dt.timedelta(days=6), end)
        params = {"block": station,
                  "begin": day.strftime("%Y%m%d") + "0000",
                  "end": chunk_end.strftime("%Y%m%d") + "2359"}
        text = fetch(OGIMET, params)
        if "Status: 500" in text or "quota" in text.lower():
            sys.exit("OGIMET refused the request: " + text[:200])
        o, d = parse_synop(text)
        obs.update(o)
        daily.update(d)
        day = chunk_end + dt.timedelta(days=1)
        time.sleep(3)                        # OGIMET asks for few requests
    return obs, daily


# --- forecasts --------------------------------------------------------

def fetch_forecasts(model, lat, lon, start, end):
    """{lead: {datetime (UTC): (temperature, precipitation)}}"""
    variables = []
    for lead in range(MAX_LEAD + 1):
        suffix = "" if lead == 0 else "_previous_day%d" % lead
        variables += ["temperature_2m" + suffix, "precipitation" + suffix]
    params = {"latitude": lat, "longitude": lon, "models": model,
              "hourly": ",".join(variables), "timezone": "GMT",
              "start_date": start.isoformat(), "end_date": end.isoformat()}
    data = json.loads(fetch(PREVIOUS_RUNS, params))
    if data.get("error"):
        sys.exit("Open-Meteo: %s: %s" % (model, data.get("reason")))
    hourly = data["hourly"]
    times = [dt.datetime.fromisoformat(t) for t in hourly["time"]]
    result = {}
    for lead in range(MAX_LEAD + 1):
        suffix = "" if lead == 0 else "_previous_day%d" % lead
        temps = hourly.get("temperature_2m" + suffix) or []
        precs = hourly.get("precipitation" + suffix) or []
        result[lead] = {t: (temps[i] if i < len(temps) else None,
                            precs[i] if i < len(precs) else None)
                        for i, t in enumerate(times)}
    return result, (data["latitude"], data["longitude"])


# --- scores -----------------------------------------------------------

def mean(values):
    return sum(values) / len(values) if values else float("nan")


def scores(obs, daily, fc):
    """Temperature and precipitation scores of one model at one lead."""
    t_err = []
    for when, (t_obs, _p) in obs.items():
        t_fc = fc.get(when, (None, None))[0]
        if t_obs is not None and t_fc is not None:
            t_err.append(t_fc - t_obs)

    # hourly rain / no rain (>= 0.1 mm): the "rain in the next hour" alert
    hits = misses = false_alarms = 0
    for when, (_t, p_obs) in obs.items():
        p_fc = fc.get(when, (None, None))[1]
        if p_obs is None or p_fc is None:
            continue
        wet_obs, wet_fc = p_obs >= 0.1, p_fc >= 0.1
        hits += wet_obs and wet_fc
        misses += wet_obs and not wet_fc
        false_alarms += wet_fc and not wet_obs

    # 24 h sums from 06 to 06 UTC (the 7RRRR group of the 06 UTC report);
    # an hourly forecast value is the amount of the hour that ends then
    d_err, d_wet = [], [0, 0]
    for when, s_obs in daily.items():
        if when.hour != 6:
            continue
        fcs = [fc.get(when - dt.timedelta(hours=h), (None, None))[1]
               for h in range(24)]
        if any(p is None for p in fcs):
            continue
        s_fc = sum(fcs)
        d_err.append(s_fc - s_obs)
        d_wet[0] += (s_obs >= 1.0) == (s_fc >= 1.0)
        d_wet[1] += 1

    return {
        "t_n": len(t_err),
        "t_mae": mean([abs(e) for e in t_err]),
        "t_bias": mean(t_err),
        "t_rmse": mean([e * e for e in t_err]) ** 0.5 if t_err else float("nan"),
        "pod": hits / (hits + misses) if hits + misses else float("nan"),
        "far": false_alarms / (hits + false_alarms) if hits + false_alarms else float("nan"),
        "h_events": hits + misses,
        "d_n": len(d_err),
        "d_mae": mean([abs(e) for e in d_err]),
        "d_bias": mean(d_err),
        "d_wet": d_wet[0] / d_wet[1] if d_wet[1] else float("nan"),
    }


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--station", required=True, help="WMO station index (OGIMET)")
    ap.add_argument("--lat", type=float, required=True, help="station latitude")
    ap.add_argument("--lon", type=float, required=True, help="station longitude")
    ap.add_argument("--days", type=int, default=28, help="days to compare")
    ap.add_argument("--models", default="ecmwf_ifs,icon_eu,best_match")
    args = ap.parse_args()

    # the last full UTC day, so that every observation is in
    end = dt.datetime.now(dt.timezone.utc).date() - dt.timedelta(days=1)
    start = end - dt.timedelta(days=args.days - 1)
    print("Station %s (%.3f, %.3f), %s .. %s (UTC)" %
          (args.station, args.lat, args.lon, start, end))
    obs, daily = fetch_observations(args.station, start, end)
    n_t = sum(1 for t, _p in obs.values() if t is not None)
    n_p = sum(1 for _t, p in obs.values() if p is not None)
    wet = sum(1 for _t, p in obs.values() if p is not None and p >= 0.1)
    print("Observations: %d hours, %d with temperature, %d with 1 h "
          "precipitation (%d wet hours, %.1f mm)" %
          (len(obs), n_t, n_p, wet,
           sum(p for _t, p in obs.values() if p is not None)))
    d06 = [v for w, v in daily.items() if w.hour == 6]
    print("24 h totals at 06 UTC: %d days, %.1f mm, %d wet days (>= 1 mm)" %
          (len(d06), sum(d06), sum(1 for v in d06 if v >= 1.0)))

    rows = []
    for model in args.models.split(","):
        fc, grid = fetch_forecasts(model, args.lat, args.lon, start, end)
        print("%s: grid point %.3f, %.3f" % (model, grid[0], grid[1]))
        for lead in range(MAX_LEAD + 1):
            s = scores(obs, daily, fc[lead])
            if s["t_n"]:
                rows.append((model, lead, s))

    print()
    print("Temperature at 2 m, hourly (forecast - observed, degC)")
    print("%-12s %4s %6s %6s %6s %6s" % ("model", "day", "n", "MAE", "bias", "RMSE"))
    for model, lead, s in rows:
        print("%-12s %4d %6d %6.2f %+6.2f %6.2f" %
              (model, lead, s["t_n"], s["t_mae"], s["t_bias"], s["t_rmse"]))
    print()
    print("Precipitation: hourly rain >= 0.1 mm (POD: share of the observed")
    print("wet hours that were forecast, FAR: share of the forecast wet hours")
    print("that stayed dry) and 24 h sums (06-06 UTC, mm; wet ok: the forecast")
    print("got wet (>= 1 mm) or dry right)")
    print("%-12s %4s %6s %5s %5s %6s %6s %6s %6s" %
          ("model", "day", "wet h", "POD", "FAR", "days", "MAE", "bias", "wet ok"))
    for model, lead, s in rows:
        print("%-12s %4d %6d %5.2f %5.2f %6d %6.2f %+6.2f %6.2f" %
              (model, lead, s["h_events"], s["pod"], s["far"], s["d_n"],
               s["d_mae"], s["d_bias"], s["d_wet"]))


if __name__ == "__main__":
    main()
