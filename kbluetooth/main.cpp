/*
    This file is part of kbluetooth, the Bluetooth applet of the KDE3 revival

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.
*/

#include <kaboutdata.h>
#include <kcmdlineargs.h>
#include <klocale.h>
#include <kuniqueapplication.h>

#include "bluez.h"
#include "tray.h"

static const char description[] = I18N_NOOP("Bluetooth applet for the system tray (BlueZ 5)");

class KBluetoothApp : public KUniqueApplication
{
public:
    KBluetoothApp() : m_tray(0) {}
    virtual ~KBluetoothApp()
    {
        delete m_tray;
        delete Bluez::self();
    }

    virtual int newInstance()
    {
        if (!m_tray) {
            m_tray = new BluetoothTray;
            setMainWidget(m_tray);
        } else {
            // started again: show the icon (it may hide without an
            // adapter) and the devices
            m_tray->activate();
        }
        return 0;
    }

private:
    BluetoothTray *m_tray;
};

int main(int argc, char **argv)
{
    KAboutData about("kbluetooth", I18N_NOOP("kbluetooth"), "0.1", description,
                     KAboutData::License_GPL, "(c) 2026 The KDE3 revival developers");
    KCmdLineArgs::init(argc, argv, &about);
    KUniqueApplication::addCmdLineOptions();

    if (!KUniqueApplication::start())
        return 0;  // the running kbluetooth shows itself

    KBluetoothApp app;
    app.disableSessionManagement();  // started by autostart, not by the session
    return app.exec();
}
