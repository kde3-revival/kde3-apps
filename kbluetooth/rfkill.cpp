/*
    This file is part of kbluetooth, the Bluetooth applet of the KDE3 revival

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.
*/

#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>

#include <qdir.h>
#include <qfile.h>
#include <qsocketnotifier.h>
#include <qstringlist.h>
#include <qtimer.h>

#include "rfkill.h"

static QString readLine(const QString &file)
{
    QFile f(file);
    if (!f.open(IO_ReadOnly))
        return QString::null;
    QString line;
    f.readLine(line, 64);
    return line.stripWhiteSpace();
}

RfKill::RfKill(QObject *parent)
    : QObject(parent, "rfkill"), m_fd(-1), m_notifier(0), m_timer(0), m_state(NoRadio)
{
    // KBLUETOOTH_RFKILL_DIR replaces /sys/class/rfkill, for the tests
    const char *dir = getenv("KBLUETOOTH_RFKILL_DIR");
    m_dir = dir && *dir ? QFile::decodeName(dir) : QString::fromLatin1("/sys/class/rfkill");
    if (!dir || !*dir)
        m_fd = ::open("/dev/rfkill", O_RDONLY | O_NONBLOCK | O_CLOEXEC);
    if (m_fd >= 0) {
        m_notifier = new QSocketNotifier(m_fd, QSocketNotifier::Read, this);
        connect(m_notifier, SIGNAL(activated(int)), SLOT(readEvents()));
    } else {
        m_timer = new QTimer(this);
        connect(m_timer, SIGNAL(timeout()), SLOT(update()));
        m_timer->start(3000);
    }
    update();
}

RfKill::~RfKill()
{
    delete m_notifier;
    if (m_fd >= 0)
        ::close(m_fd);
}

void RfKill::readEvents()
{
    // the events say which radio changed; the state is read from sysfs
    char buffer[256];
    while (::read(m_fd, buffer, sizeof(buffer)) > 0)
        ;
    // the sysfs files follow the event
    update();
}

void RfKill::update()
{
    QDir dir(m_dir);
    QStringList radios = dir.entryList("rfkill*", QDir::Dirs | QDir::System);
    State state = NoRadio;
    for (QStringList::ConstIterator it = radios.begin(); it != radios.end(); ++it) {
        QString base = m_dir + '/' + *it + '/';
        if (readLine(base + "type") != "bluetooth")
            continue;
        State s = Unblocked;
        if (readLine(base + "hard") == "1")
            s = HardBlocked;
        else if (readLine(base + "soft") == "1")
            s = SoftBlocked;
        // one usable radio is enough
        if (state == NoRadio || s < state)
            state = s;
    }
    if (state != m_state) {
        m_state = state;
        emit changed(state);
    }
}

#include "rfkill.moc"
