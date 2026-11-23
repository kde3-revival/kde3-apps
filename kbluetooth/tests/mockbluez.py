#!/usr/bin/env python3
#
# This file is part of kbluetooth, the Bluetooth applet of the KDE3 revival
#
# A mock of BlueZ 5 for the tests of kbluetooth: it owns org.bluez on the
# bus of DBUS_SYSTEM_BUS_ADDRESS (a private bus; never the real system bus)
# and exports an ObjectManager, AgentManager1, one or two Adapter1 and
# scripted Device1 and Battery1 objects.  Start kbluetooth with the same
# DBUS_SYSTEM_BUS_ADDRESS.
#
# The control interface org.kde3.MockBluez at /mock drives the scenarios:
#   gdbus call -a $DBUS_SYSTEM_BUS_ADDRESS -d org.bluez -o /mock \
#       -m org.kde3.MockBluez.<Method> ...
# Methods: AddAdapter(s name), RemoveAdapter(s name), Restart(),
# SetConnected(s address, b), SetBattery(s address, i percent),
# IncomingPair(s address)  (a device pairs from its side),
# AuthorizeService(s address, s uuid), SetPowerState(s state)
#
# The devices found by discovery pair in different ways (see DISCOVERABLE).
#
#    This program is free software; you can redistribute it and/or modify
#    it under the terms of the GNU General Public License as published by
#    the Free Software Foundation; either version 2 of the License, or
#    (at your option) any later version.

import sys
import dbus
import dbus.service
import dbus.mainloop.glib
from gi.repository import GLib

BLUEZ = 'org.bluez'
ADAPTER = 'org.bluez.Adapter1'
DEVICE = 'org.bluez.Device1'
BATTERY = 'org.bluez.Battery1'
AGENT_MANAGER = 'org.bluez.AgentManager1'
PROPS = 'org.freedesktop.DBus.Properties'
OM = 'org.freedesktop.DBus.ObjectManager'

A2DP_SINK = '0000110b-0000-1000-8000-00805f9b34fb'
AVRCP = '0000110e-0000-1000-8000-00805f9b34fb'
HFP = '0000111e-0000-1000-8000-00805f9b34fb'
HID = '00001124-0000-1000-8000-00805f9b34fb'
HOGP = '00001812-0000-1000-8000-00805f9b34fb'
BATT = '0000180f-0000-1000-8000-00805f9b34fb'
OPP = '00001105-0000-1000-8000-00805f9b34fb'

# paired before the test: (address, name, icon, connected, battery, uuids)
PAIRED = [
    ('00:1B:66:A1:00:01', 'WH-1000XM4', 'audio-headphones', False, 70, [A2DP_SINK, AVRCP, HFP]),
    ('00:1B:66:A1:00:02', 'MX Master 3', 'input-mouse', True, 55, [HOGP, BATT]),
    ('00:1B:66:A1:00:03', 'Kitchen Speaker', 'audio-card', False, None, [A2DP_SINK]),
]

# found by discovery: (address, name, icon, rssi, pairing method, connect ok)
#   confirm: RequestConfirmation 123456
#   pin:     RequestPinCode, wants 0000
#   passkey: RequestPasskey, wants 654321
#   display: DisplayPasskey 246810, the "device" types a digit a second
#   displaypin: DisplayPinCode 8642
#   justworks: no agent call
#   timeout: fails with ConnectionAttemptFailed after 2 s
DISCOVERABLE = [
    ('5C:F3:70:B0:00:11', 'Pixel 8', 'phone', -48, 'confirm', False),
    ('5C:F3:70:B0:00:12', 'Car Kit', 'audio-card', -71, 'pin', True),
    ('5C:F3:70:B0:00:13', 'BT Keyboard K380', 'input-keyboard', -55, 'display', True),
    ('5C:F3:70:B0:00:14', 'Old Mouse', 'input-mouse', -60, 'justworks', True),
    ('5C:F3:70:B0:00:15', 'Garden Speaker', 'audio-card', -90, 'timeout', True),
    ('5C:F3:70:B0:00:16', 'Label Printer', 'printer', -66, 'passkey', True),
    ('5C:F3:70:B0:00:17', 'Old Headset', 'audio-headset', -62, 'displaypin', True),
]


class Failed(dbus.DBusException):
    _dbus_error_name = 'org.bluez.Error.Failed'


def error(name, message):
    e = dbus.DBusException(message)
    e._dbus_error_name = name
    return e


class Obj(dbus.service.Object):
    def __init__(self, mock, path):
        self.mock = mock
        self.path = path
        self.props = {}
        dbus.service.Object.__init__(self, mock.bus, path)

    def interfaces(self):
        return self.props

    def set(self, iface, name, value):
        self.props[iface][name] = value
        self.PropertiesChanged(iface, {name: value}, [])

    @dbus.service.method(PROPS, in_signature='ss', out_signature='v')
    def Get(self, iface, name):
        return self.props[iface][name]

    @dbus.service.method(PROPS, in_signature='s', out_signature='a{sv}')
    def GetAll(self, iface):
        return self.props.get(iface, {})

    @dbus.service.method(PROPS, in_signature='ssv')
    def Set(self, iface, name, value):
        self.setProperty(iface, name, value)

    def setProperty(self, iface, name, value):
        self.set(iface, name, value)

    @dbus.service.signal(PROPS, signature='sa{sv}as')
    def PropertiesChanged(self, iface, changed, invalidated):
        pass


class Adapter(Obj):
    def __init__(self, mock, name, address):
        Obj.__init__(self, mock, '/org/bluez/' + name)
        self.name = name
        self.discovery = None
        self.props[ADAPTER] = {
            'Address': address, 'AddressType': 'public', 'Name': 'mockhost',
            'Alias': 'mockhost' if name == 'hci0' else 'mockhost #2', 'Class': dbus.UInt32(0x6c010c),
            'Powered': dbus.Boolean(True), 'PowerState': 'on',
            'Discoverable': dbus.Boolean(False), 'Pairable': dbus.Boolean(True),
            'Discovering': dbus.Boolean(False),
            'UUIDs': dbus.Array([], signature='s'),
        }

    def setProperty(self, iface, name, value):
        if iface == ADAPTER and name == 'Powered':
            if self.mock.blocked:
                raise error('org.bluez.Error.Failed', 'Blocked through rfkill')
            on = bool(value)
            self.set(ADAPTER, 'PowerState', 'off-enabling' if on else 'on-disabling')

            def done():
                self.set(ADAPTER, 'Powered', dbus.Boolean(on))
                self.set(ADAPTER, 'PowerState', 'on' if on else 'off')
                if not on:
                    self.stopDiscovery()
                    for d in self.mock.devices(self):
                        if d.props[DEVICE]['Connected']:
                            d.set(DEVICE, 'Connected', dbus.Boolean(False))
                return False
            GLib.timeout_add(400, done)
            return
        Obj.setProperty(self, iface, name, value)

    @dbus.service.method(ADAPTER)
    def StartDiscovery(self):
        if not self.props[ADAPTER]['Powered']:
            raise error('org.bluez.Error.NotReady', 'Resource Not Ready')
        if self.discovery:
            raise error('org.bluez.Error.InProgress', 'Operation already in progress')
        self.set(ADAPTER, 'Discovering', dbus.Boolean(True))
        todo = list(DISCOVERABLE)

        def found():
            if not todo or not self.discovery:
                return False
            address, name, icon, rssi, method, ok = todo.pop(0)
            self.mock.addDevice(self, address, name, icon, False, False, None, [], rssi, method, ok)
            return True
        self.discovery = GLib.timeout_add(700, found)

    def stopDiscovery(self):
        if self.discovery:
            GLib.source_remove(self.discovery)
            self.discovery = None
            self.set(ADAPTER, 'Discovering', dbus.Boolean(False))

    @dbus.service.method(ADAPTER)
    def StopDiscovery(self):
        if not self.discovery:
            raise error('org.bluez.Error.Failed', 'No discovery started')
        self.stopDiscovery()

    @dbus.service.method(ADAPTER, in_signature='a{sv}')
    def SetDiscoveryFilter(self, f):
        pass

    @dbus.service.method(ADAPTER, in_signature='o')
    def RemoveDevice(self, path):
        d = self.mock.objects.get(str(path))
        if not isinstance(d, Device):
            raise error('org.bluez.Error.DoesNotExist', 'Does Not Exist')
        self.mock.removeObject(d)


class Device(Obj):
    def __init__(self, mock, adapter, address, name, icon, paired, connected, battery, uuids,
                 rssi, method, connectOk):
        Obj.__init__(self, mock, adapter.path + '/dev_' + address.replace(':', '_'))
        self.adapter = adapter
        self.method = method
        self.connectOk = connectOk
        self.pairing = None
        self.props[DEVICE] = {
            'Address': address, 'AddressType': 'public', 'Name': name, 'Alias': name,
            'Icon': icon, 'Paired': dbus.Boolean(paired), 'Bonded': dbus.Boolean(paired),
            'Trusted': dbus.Boolean(paired), 'Blocked': dbus.Boolean(False),
            'Connected': dbus.Boolean(connected), 'LegacyPairing': dbus.Boolean(False),
            'ServicesResolved': dbus.Boolean(connected),
            'UUIDs': dbus.Array(uuids, signature='s'), 'Adapter': dbus.ObjectPath(adapter.path),
        }
        if rssi is not None:
            self.props[DEVICE]['RSSI'] = dbus.Int16(rssi)
        if battery is not None:
            self.props[BATTERY] = {'Percentage': dbus.Byte(battery), 'Source': 'mock'}

    def setProperty(self, iface, name, value):
        if iface == DEVICE and name == 'Alias':
            value = value or self.props[DEVICE]['Name']
        Obj.setProperty(self, iface, name, value)

    @dbus.service.method(DEVICE, async_callbacks=('ok', 'err'))
    def Connect(self, ok, err):
        if not self.adapter.props[ADAPTER]['Powered']:
            err(error('org.bluez.Error.NotReady', 'Resource Not Ready'))
            return
        if self.props[DEVICE]['Connected']:
            err(error('org.bluez.Error.AlreadyConnected', 'Already Connected'))
            return

        def done():
            if self.connectOk:
                self.set(DEVICE, 'Connected', dbus.Boolean(True))
                self.set(DEVICE, 'ServicesResolved', dbus.Boolean(True))
                ok()
            else:
                err(error('org.bluez.Error.Failed', 'br-connection-profile-unavailable'))
            return False
        GLib.timeout_add(1500, done)

    @dbus.service.method(DEVICE, async_callbacks=('ok', 'err'))
    def Disconnect(self, ok, err):
        def done():
            self.set(DEVICE, 'ServicesResolved', dbus.Boolean(False))
            self.set(DEVICE, 'Connected', dbus.Boolean(False))
            ok()
            return False
        GLib.timeout_add(500, done)

    def paired(self):
        self.set(DEVICE, 'Paired', dbus.Boolean(True))
        self.set(DEVICE, 'Bonded', dbus.Boolean(True))
        uuids = {'phone': [HFP, OPP], 'input-keyboard': [HID], 'input-mouse': [HID],
                 'printer': [], 'audio-card': [A2DP_SINK, AVRCP],
                 'audio-headset': [HFP]}.get(self.props[DEVICE]['Icon'], [])
        self.set(DEVICE, 'UUIDs', dbus.Array(uuids, signature='s'))

    @dbus.service.method(DEVICE, async_callbacks=('ok', 'err'))
    def Pair(self, ok, err):
        if self.props[DEVICE]['Paired']:
            err(error('org.bluez.Error.AlreadyExists', 'Already Exists'))
            return
        if self.pairing:
            err(error('org.bluez.Error.InProgress', 'In Progress'))
            return
        self.pairing = (ok, err)
        m = self.method
        agent = self.mock.agent
        if m == 'timeout':
            GLib.timeout_add(2000, self.finish, 'org.bluez.Error.ConnectionAttemptFailed', 'Page Timeout')
            return
        if m == 'justworks':
            GLib.timeout_add(800, self.finish, None, None)
            return
        if not agent:
            self.finish('org.bluez.Error.AuthenticationFailed', 'Authentication Failed')
            return
        if m == 'confirm':
            self.mock.callAgent('RequestConfirmation', [dbus.ObjectPath(self.path), dbus.UInt32(123456)],
                                lambda *a: self.finish(None, None), self.agentError)
        elif m == 'pin':
            def pin(code):
                if str(code) == '0000':
                    self.finish(None, None)
                else:
                    self.finish('org.bluez.Error.AuthenticationFailed', 'Authentication Failed')
            self.mock.callAgent('RequestPinCode', [dbus.ObjectPath(self.path)], pin, self.agentError)
        elif m == 'passkey':
            def passkey(value):
                if int(value) == 654321:
                    self.finish(None, None)
                else:
                    self.finish('org.bluez.Error.AuthenticationFailed', 'Authentication Failed')
            self.mock.callAgent('RequestPasskey', [dbus.ObjectPath(self.path)], passkey, self.agentError)
        elif m == 'display':
            typed = [0]

            def step():
                if not self.pairing:
                    return False
                self.mock.callAgent('DisplayPasskey', [dbus.ObjectPath(self.path), dbus.UInt32(246810),
                                                       dbus.UInt16(typed[0])], lambda *a: None, lambda e: None)
                if typed[0] == 6:
                    GLib.timeout_add(1000, self.finish, None, None)
                    return False
                typed[0] += 1
                return True
            step()
            GLib.timeout_add(1000, step)
        elif m == 'displaypin':
            self.mock.callAgent('DisplayPinCode', [dbus.ObjectPath(self.path), '8642'],
                                lambda *a: None, lambda e: None)
            GLib.timeout_add(8000, self.finish, None, None)

    def agentError(self, e):
        name = e.get_dbus_name() if hasattr(e, 'get_dbus_name') else ''
        if name == 'org.bluez.Error.Canceled':
            self.finish('org.bluez.Error.AuthenticationCanceled', 'Authentication Canceled')
        else:
            self.finish('org.bluez.Error.AuthenticationRejected', 'Authentication Rejected')

    def finish(self, name, message):
        if not self.pairing:
            return False
        ok, err = self.pairing
        self.pairing = None
        if name:
            err(error(name, message))
        else:
            self.paired()
            ok()
        return False

    @dbus.service.method(DEVICE)
    def CancelPairing(self):
        if not self.pairing:
            raise error('org.bluez.Error.DoesNotExist', 'Does Not Exist')
        self.mock.callAgent('Cancel', [], lambda *a: None, lambda e: None)
        self.finish('org.bluez.Error.AuthenticationCanceled', 'Authentication Canceled')


class AgentManager(Obj):
    def __init__(self, mock):
        Obj.__init__(self, mock, '/org/bluez')
        self.props[AGENT_MANAGER] = {}

    @dbus.service.method(AGENT_MANAGER, in_signature='os', sender_keyword='sender')
    def RegisterAgent(self, path, capability, sender=None):
        if (sender, str(path)) in self.mock.agents:
            raise error('org.bluez.Error.AlreadyExists', 'Already Exists')
        self.mock.agents.append((sender, str(path)))
        log('RegisterAgent %s %s %s' % (sender, path, capability))

    @dbus.service.method(AGENT_MANAGER, in_signature='o', sender_keyword='sender')
    def UnregisterAgent(self, path, sender=None):
        self.mock.agents.remove((sender, str(path)))
        if self.mock.agent == (sender, str(path)):
            self.mock.agent = None

    @dbus.service.method(AGENT_MANAGER, in_signature='o', sender_keyword='sender')
    def RequestDefaultAgent(self, path, sender=None):
        if (sender, str(path)) not in self.mock.agents:
            raise error('org.bluez.Error.DoesNotExist', 'Does Not Exist')
        self.mock.agent = (sender, str(path))
        log('RequestDefaultAgent %s %s' % (sender, path))


class Root(dbus.service.Object):
    def __init__(self, mock):
        self.mock = mock
        dbus.service.Object.__init__(self, mock.bus, '/')

    @dbus.service.method(OM, out_signature='a{oa{sa{sv}}}')
    def GetManagedObjects(self):
        return dict((p, o.interfaces()) for p, o in self.mock.objects.items())

    @dbus.service.signal(OM, signature='oa{sa{sv}}')
    def InterfacesAdded(self, path, interfaces):
        pass

    @dbus.service.signal(OM, signature='oas')
    def InterfacesRemoved(self, path, interfaces):
        pass


class Control(dbus.service.Object):
    def __init__(self, mock):
        self.mock = mock
        dbus.service.Object.__init__(self, mock.bus, '/mock')

    @dbus.service.method('org.kde3.MockBluez', in_signature='s')
    def AddAdapter(self, name):
        self.mock.addAdapter(str(name))

    @dbus.service.method('org.kde3.MockBluez', in_signature='s')
    def RemoveAdapter(self, name):
        a = self.mock.objects.get('/org/bluez/' + name)
        for d in self.mock.devices(a):
            self.mock.removeObject(d)
        self.mock.removeObject(a)

    @dbus.service.method('org.kde3.MockBluez')
    def Restart(self):
        # bluetoothd stops for 2 s and comes back with the same objects
        self.mock.restart()

    @dbus.service.method('org.kde3.MockBluez', in_signature='sb')
    def SetConnected(self, address, on):
        d = self.mock.device(address)
        d.set(DEVICE, 'Connected', dbus.Boolean(on))

    @dbus.service.method('org.kde3.MockBluez', in_signature='si')
    def SetBattery(self, address, percent):
        d = self.mock.device(address)
        if BATTERY in d.props:
            d.set(BATTERY, 'Percentage', dbus.Byte(percent))
        else:
            d.props[BATTERY] = {'Percentage': dbus.Byte(percent), 'Source': 'mock'}
            self.mock.root.InterfacesAdded(d.path, {BATTERY: d.props[BATTERY]})

    @dbus.service.method('org.kde3.MockBluez', in_signature='b')
    def SetBlocked(self, blocked):
        # rfkill soft block: the adapter goes off and cannot be switched on
        self.mock.blocked = bool(blocked)
        for a in self.mock.adapters():
            if blocked:
                a.set(ADAPTER, 'Powered', dbus.Boolean(False))
                a.set(ADAPTER, 'PowerState', 'off-blocked')
            else:
                a.set(ADAPTER, 'PowerState', 'off')

    @dbus.service.method('org.kde3.MockBluez', in_signature='s', out_signature='s',
                         async_callbacks=('ok', 'err'))
    def IncomingPair(self, address, ok, err):
        # the device starts the pairing: Just Works asks RequestAuthorization
        d = self.mock.device(address)

        def done(*a):
            d.paired()
            ok('paired')

        self.mock.callAgent('RequestAuthorization', [dbus.ObjectPath(d.path)], done,
                            lambda e: ok('rejected: %s' % e.get_dbus_name()))

    @dbus.service.method('org.kde3.MockBluez', in_signature='ss', out_signature='s',
                         async_callbacks=('ok', 'err'))
    def AuthorizeService(self, address, uuid, ok, err):
        d = self.mock.device(address)
        self.mock.callAgent('AuthorizeService', [dbus.ObjectPath(d.path), uuid],
                            lambda *a: ok('allowed'),
                            lambda e: ok('rejected: %s' % e.get_dbus_name()))

    @dbus.service.method('org.kde3.MockBluez', in_signature='s', out_signature='s')
    def Dump(self, address):
        d = self.mock.device(address)
        return repr(dict(d.props[DEVICE]))


def log(text):
    sys.stderr.write('mockbluez: %s\n' % text)
    sys.stderr.flush()


class Mock:
    def __init__(self, bus):
        self.bus = bus
        self.objects = {}
        self.agents = []
        self.agent = None
        self.blocked = False
        self.root = Root(self)
        self.control = Control(self)
        self.start()

    def start(self):
        self.agents = []
        self.agent = None
        self.name = dbus.service.BusName(BLUEZ, self.bus, do_not_queue=True)
        self.manager = AgentManager(self)
        self.objects[self.manager.path] = self.manager
        self.addAdapter('hci0', announce=False)
        a = self.objects['/org/bluez/hci0']
        for address, name, icon, connected, battery, uuids in PAIRED:
            self.addDevice(a, address, name, icon, True, connected, battery, uuids, None, None, True,
                           announce=False)
        log('ready')

    def restart(self):
        log('restart: going away')
        for o in list(self.objects.values()):
            o.remove_from_connection()
        self.objects = {}
        self.bus.release_name(BLUEZ)
        del self.name

        def back():
            self.start()
            return False
        GLib.timeout_add(2000, back)

    def adapters(self):
        return [o for o in self.objects.values() if isinstance(o, Adapter)]

    def devices(self, adapter):
        return [o for o in self.objects.values() if isinstance(o, Device) and o.adapter is adapter]

    def device(self, address):
        for o in self.objects.values():
            if isinstance(o, Device) and o.props[DEVICE]['Address'] == address:
                return o
        raise error('org.bluez.Error.DoesNotExist', 'No such device')

    def addAdapter(self, name, announce=True):
        address = '00:1A:7D:DA:71:%02X' % (len(self.adapters()) + 1)
        a = Adapter(self, name, address)
        self.objects[a.path] = a
        if announce:
            self.root.InterfacesAdded(a.path, a.interfaces())
        return a

    def addDevice(self, adapter, address, name, icon, paired, connected, battery, uuids, rssi,
                  method, connectOk, announce=True):
        d = Device(self, adapter, address, name, icon, paired, connected, battery, uuids, rssi,
                   method, connectOk)
        if d.path in self.objects:
            # seen again: a new RSSI only
            old = self.objects[d.path]
            if rssi is not None:
                old.set(DEVICE, 'RSSI', dbus.Int16(rssi))
            d.remove_from_connection()
            return old
        self.objects[d.path] = d
        if announce:
            self.root.InterfacesAdded(d.path, d.interfaces())
        return d

    def removeObject(self, o):
        del self.objects[o.path]
        o.remove_from_connection()
        self.root.InterfacesRemoved(o.path, list(o.props.keys()))

    def callAgent(self, method, args, reply, error):
        if not self.agent:
            error(dbus.DBusException('no agent'))
            return
        sender, path = self.agent
        log('agent call %s %s' % (method, [str(a) for a in args]))

        def ok(*a):
            log('agent reply %s %s' % (method, [str(x) for x in a]))
            reply(*a)

        def bad(e):
            log('agent error %s %s' % (method, e.get_dbus_name()))
            error(e)

        self.bus.call_async(sender, path, 'org.bluez.Agent1', method,
                            {'RequestConfirmation': 'ou', 'RequestPinCode': 'o', 'RequestPasskey': 'o',
                             'DisplayPasskey': 'ouq', 'DisplayPinCode': 'os', 'RequestAuthorization': 'o',
                             'AuthorizeService': 'os', 'Cancel': '', 'Release': ''}[method],
                            args, ok, bad, timeout=120)


def main():
    dbus.mainloop.glib.DBusGMainLoop(set_as_default=True)
    # the bus of DBUS_SYSTEM_BUS_ADDRESS; refuse to run on the real one
    import os
    address = os.environ.get('DBUS_SYSTEM_BUS_ADDRESS', '')
    if not address or 'run/dbus/system_bus_socket' in address:
        sys.exit('mockbluez: set DBUS_SYSTEM_BUS_ADDRESS to a private bus')
    bus = dbus.bus.BusConnection(address)
    Mock(bus)
    GLib.MainLoop().run()


if __name__ == '__main__':
    main()
