# Chargemanager
[![GitHub sourcecode](https://img.shields.io/badge/Source-GitHub-green)](https://github.com/pvtom/chargemanager/)
[![GitHub last commit](https://img.shields.io/github/last-commit/pvtom/chargemanager)](https://github.com/pvtom/chargemanager/commits)
[![GitHub issues](https://img.shields.io/github/issues/pvtom/chargemanager)](https://github.com/pvtom/chargemanager/issues)
[![GitHub pull requests](https://img.shields.io/github/issues-pr/pvtom/chargemanager)](https://github.com/pvtom/chargemanager/pulls)
[![GitHub](https://img.shields.io/github/license/pvtom/chargemanager)](https://github.com/pvtom/chargemanager/blob/main/LICENSE)

This tool enables charging of an electric car depending on the availability of surplus energy from the photovoltaic system.
Vehicles from the Volkswagen Group that can be connected and controlled by Weconnect and E3/DC home power stations are supported.

The control takes place via the software of the vehicle, independent of the wallbox used.

Depending on the available solar power, the system switches between not charging, charging with reduced or maximum power.

In addition, the house battery can support the charging process. For this purpose, a hysteresis between two SOC values can be set.

The software module requires the tools [WeConnect-mqtt](https://github.com/tillsteinbach/WeConnect-mqtt/) and [rscp2mqtt](https://github.com/pvtom/rscp2mqtt/).

## Prerequisites

If you need to install rscp2mqtt please follow the [link](https://github.com/pvtom/rscp2mqtt/).

Please install [WeConnect-mqtt](https://github.com/tillsteinbach/WeConnect-mqtt/) as well.

The chargemanager needs the library libmosquitto. For installation please enter:

```
sudo apt-get install libmosquitto-dev
```

## Cloning the Repository

```
git clone https://github.com/pvtom/chargemanager.git
cd chargemanager
```

## Compilation

To build the program use
```
gcc chargemanager.c -o chargemanager -lmosquitto
```

## Usage

Please start the program with
```
./chargemanager
```

The available parameters will be displayed.

## Start WeConnect-mqtt

```
./weconnect-mqtt -u "test@test.de" -p "My_weconnect_password" --mqttbroker localhost --prefix weconnect --convert-times Europe/Berlin
```

## Start Chargemanager
```
./chargemanager --host localhost --vin WVXZZZ12345678900 --no_hysteresis --prefix weconnect
```

## Stop Chargemanager

The user can exit the program by pressing the Ctrl-c key. This also terminates the charging process of the vehicle.

## Tested with
- Volkswagen charger and car (ID serie)
- E3/DC S10 E 4500 W max battery power
- Photovoltaic modules with 11 kWp
- Raspberry Pi 4

## Disclaimer
The software is in alpha status. The use is at your own risk.
This applies to damages to the hardware (PV system, vehicle), the conditions of Weconnect use and any costs incurred for the purchase of electricity from the public grid and so on.
The tool is automatically terminated when the vehicle is disconnected from the wallbox or it is detected that the vehicle is driving.

## Known Issues
Sometimes there are difficulties to contact the Weconnect server. Here it may be useful to restart WeConnect-mqtt.
