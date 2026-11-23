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
    QStringList entries = dir.entryList("rfkill*", QDir::Dirs | QDir::System);
    QMap<QString, State> radios;
    State state = NoRadio;
    for (QStringList::ConstIterator it = entries.begin(); it != entries.end(); ++it) {
        QString base = m_dir + '/' + *it + '/';
        if (readLine(base + "type") != "bluetooth")
            continue;
        State s = Unblocked;
        if (readLine(base + "hard") == "1")
            s = HardBlocked;
        else if (readLine(base + "soft") == "1")
            s = SoftBlocked;
        QString name = readLine(base + "name");
        if (name.isEmpty())
            name = *it;
        radios[name] = QMAX(radios.contains(name) ? radios[name] : NoRadio, s);
        // one usable radio is enough
        if (state == NoRadio || s < state)
            state = s;
    }
    bool same = radios.count() == m_radios.count();
    for (QMap<QString, State>::ConstIterator it = radios.begin(); same && it != radios.end(); ++it)
        same = m_radios.contains(it.key()) && m_radios[it.key()] == it.data();
    if (state != m_state || !same) {
        m_state = state;
        m_radios = radios;
        emit changed(state);
    }
}

RfKill::State RfKill::state(const QString &adapter) const
{
    State state = NoRadio;
    for (QMap<QString, State>::ConstIterator it = m_radios.begin(); it != m_radios.end(); ++it) {
        // the radio of the adapter, and the switches that are not an adapter
        if (it.key() == adapter || !it.key().startsWith("hci"))
            state = QMAX(state, it.data());
    }
    return state;
}

#include "rfkill.moc"
