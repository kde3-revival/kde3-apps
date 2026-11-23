/* This file is part of KWeather, the weather applet of the KDE3 revival.
   Copyright (C) 2026 The KDE3 revival authors

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.
*/

#include "download.h"

#include <qtimer.h>

#include <kdebug.h>
#include <kio/job.h>
#include <klocale.h>

#include <string.h>

Download::Download(const KURL &url, uint maxBytes, int timeoutSeconds,
                   QObject *parent, const char *name)
    : QObject(parent, name), m_maxBytes(maxBytes), m_done(false)
{
    kdDebug(12006) << "Fetching " << url.url() << endl;
    m_job = KIO::get(url, true, false);
    m_job->addMetaData("cache", "reload");
    // Open-Meteo prefers "deflate", which kio_http before kdelibs 6129056
    // inflated as raw deflate while HTTP sends it with the zlib header
    // (RFC 9110): "Receiving corrupt data".  The answers are small; ask
    // for them uncompressed.  Can go once older kdelibs no longer matter.
    m_job->addMetaData("AllowCompressedPage", "false");
    connect(m_job, SIGNAL(data(KIO::Job*, const QByteArray&)),
            SLOT(slotData(KIO::Job*, const QByteArray&)));
    connect(m_job, SIGNAL(result(KIO::Job*)), SLOT(slotResult(KIO::Job*)));

    m_timer = new QTimer(this);
    connect(m_timer, SIGNAL(timeout()), SLOT(slotTimeout()));
    m_timer->start(timeoutSeconds * 1000, true);
}

Download::~Download()
{
    if (m_job)
        m_job->kill(true);
}

void Download::abort()
{
    m_done = true;
    if (m_job) {
        m_job->kill(true);      // quietly: no result()
        m_job = 0;
    }
    m_timer->stop();
    deleteLater();
}

void Download::slotData(KIO::Job *, const QByteArray &data)
{
    if (m_done || data.isEmpty())
        return;
    if (m_data.size() + data.size() > m_maxBytes) {
        finish(i18n("The answer is too large (more than %1 KiB).").arg(m_maxBytes / 1024));
        return;
    }
    uint old = m_data.size();
    m_data.resize(old + data.size());
    memcpy(m_data.data() + old, data.data(), data.size());
}

void Download::slotResult(KIO::Job *job)
{
    m_job = 0;                  // KIO deletes it
    if (m_done)
        return;
    finish(job->error() ? job->errorString() : QString::null);
}

void Download::slotTimeout()
{
    finish(i18n("No complete answer within the time limit."));
}

void Download::finish(const QString &error)
{
    if (m_done)
        return;
    m_done = true;
    m_timer->stop();
    if (m_job) {
        m_job->kill(true);
        m_job = 0;
    }
    if (!error.isEmpty())
        m_data = QByteArray();
    emit finished(this, m_data, error);
    deleteLater();
}

#include "download.moc"
