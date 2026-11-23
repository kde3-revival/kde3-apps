/*
  This file is part of KWeather.
  Copyright (c) 2003 Tobias Koenig <tokoe@kde.org>
  Copyright (c) 2026 The KDE3 revival authors (Open-Meteo)

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

  As a special exception, permission is given to link this program
  with any edition of Qt, and distribute the resulting executable,
  without including the source code for Qt in the source distribution.
*/

#ifndef KCMWEATHER_H
#define KCMWEATHER_H

#include <qvaluelist.h>

#include <kcmodule.h>

class QButtonGroup;
class QCheckBox;
class QLabel;
class QLineEdit;
class QListBox;
class QPushButton;
class KColorButton;
class KComboBox;
class KURLRequester;
class WeatherService_stub;

namespace KIO
{
  class Job;
}

class KCMWeather : public KCModule
{
  Q_OBJECT

  public:
    KCMWeather( QWidget *parent = 0, const char *name = 0 );
    ~KCMWeather();

    virtual void load();
    virtual void save();
    virtual void defaults();

  private slots:
    void enableLogWidgets( bool value );
    void searchLocation();
    void searchDone( KIO::Job *job );
    void locationSelected( int index );
    void configChanged();

  private:
    struct Place
    {
      QString name;
      QString country;
      double latitude;
      double longitude;
    };

    void showLocation();
    void fillCombo( KComboBox *combo, const char *const *items );
    void setComboItem( KComboBox *combo, const char *const *items,
                       const QString &value );
    QString comboItem( KComboBox *combo, const char *const *items );

    WeatherService_stub *mWeatherService;

    QLineEdit *mSearch;
    QPushButton *mSearchButton;
    QListBox *mResults;
    QLabel *mLocationLabel;
    KComboBox *mModel;
    KComboBox *mTemperature;
    KComboBox *mWind;
    KComboBox *mPrecipitation;
    KComboBox *mPressure;
    QButtonGroup *mViewMode;
    QCheckBox *mCustomColor;
    KColorButton *mTextColor;
    QCheckBox *mRainAlert;
    QCheckBox *mEnableLog;
    QLabel *mLogLabel;
    KURLRequester *mLogFile;

    KIO::Job *mSearchJob;
    QValueList<Place> mPlaces;
    Place mLocation;
    bool mHaveLocation;
};

#endif
