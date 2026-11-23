/* This file is part of KWeather, the weather applet of the KDE3 revival.
   Copyright (C) 2026 The KDE3 revival authors

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.
*/

#ifndef KWEATHER_DOWNLOAD_H
#define KWEATHER_DOWNLOAD_H

#include <qcstring.h>
#include <qobject.h>

#include <kurl.h>

class QTimer;

namespace KIO
{
    class Job;
}

/**
 * A download through KIO with a size limit and a deadline: KIO::storedGet()
 * keeps whatever arrives, for as long as it takes.  finished() is emitted
 * exactly once, with the data or an error, unless abort() is called; the
 * object deletes itself afterwards.
 */
class Download : public QObject
{
    Q_OBJECT

public:
    Download(const KURL &url, uint maxBytes, int timeoutSeconds,
             QObject *parent = 0, const char *name = 0);
    ~Download();

    /** Stops the download without finished(). */
    void abort();

signals:
    void finished(Download *download, const QByteArray &data, const QString &error);

private slots:
    void slotData(KIO::Job *job, const QByteArray &data);
    void slotResult(KIO::Job *job);
    void slotTimeout();

private:
    void finish(const QString &error);

    KIO::Job *m_job;
    QTimer *m_timer;
    QByteArray m_data;
    uint m_maxBytes;
    bool m_done;
};

#endif
