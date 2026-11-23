/***************************************************************************
                          dockwidget.cpp  -  description
                             -------------------
    begin                : Thu Jul 6 2000
    copyright            : (C) 2000-2003 by Ian Reinhart Geiser
                         : (C) 2002-2003 Nadeem Hasan <nhasan@kde.org>
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

#include "dockwidget.h"
#include "weatherbutton.h"
#include "weatherservice_stub.h"
#include "weathericons.h"

#include <qtooltip.h>
#include <qlayout.h>
#include <qlabel.h>
#include <qtimer.h>
#include <qobjectlist.h>
#include <qstylesheet.h>

#include <kdebug.h>
#include <kglobalsettings.h>
#include <klocale.h>

dockwidget::dockwidget(const QString &location, QWidget *parent,
        const char *name) : QWidget(parent,name), m_locationCode( location ), m_orientation( Horizontal ),
        m_iconSize( 22 )
{
    m_font = KGlobalSettings::generalFont();
    setBackgroundOrigin( AncestorOrigin );
    initDock();
    connect(m_button, SIGNAL( clicked() ), SIGNAL( buttonClicked() ));

    m_weatherService = new WeatherService_stub( "KWeatherService", "WeatherService" );
}

dockwidget::~dockwidget()
{
    delete m_weatherService;
}

void dockwidget::setLocationCode(const QString &locationCode)
{
    m_locationCode = locationCode;
    showWeather();
}

void dockwidget::setViewMode(int _mode)
{
    kdDebug(12004) << "View mode " << _mode << endl;
    m_mode = _mode;
    if (m_mode == ShowIconOnly)
    {
        m_lblTemp->hide();
        m_lblWind->hide();
        m_lblPres->hide();
    }
    else if (m_mode == ShowTempOnly)
    {
        m_lblTemp->show();
        m_lblWind->hide();
        m_lblPres->hide();
    }
    else if (m_mode == ShowAll)
    {
        m_lblTemp->show();
        m_lblWind->show();
        m_lblPres->show();
    }
}

void dockwidget::showWeather()
{
    QString tip = "<qt>";

    QString temp     = "?";
    QString wind     = "?";
    QString pressure = "?";
    QString iconName = WeatherIcons::iconName( -1, true );

    if ( !m_locationCode.isEmpty() )
    {
        QString t = m_weatherService->temperature( m_locationCode );
        if ( !t.isEmpty() )
        {
            temp     = t;
            wind     = m_weatherService->wind( m_locationCode );
            pressure = m_weatherService->pressure( m_locationCode );
            iconName = m_weatherService->currentIconString( m_locationCode );
        }

        QString name    = m_weatherService->stationName( m_locationCode );
        QString country = m_weatherService->stationCountry( m_locationCode );
        tip += "<b><nobr>" + QStyleSheet::escape( country.isEmpty() ? name
                : i18n( "place, country", "%1, %2" ).arg( name ).arg( country ) )
             + "</nobr></b><br>";

        if ( t.isEmpty() )
        {
            QString error = m_weatherService->error( m_locationCode );
            tip += "<nobr>" + QStyleSheet::escape( error.isEmpty()
                    ? i18n( "Waiting for the weather data..." )
                    : i18n( "The weather could not be fetched: %1" ).arg( error ) )
                 + "</nobr>";
        }
        else
        {
            QStringList weather = m_weatherService->weather( m_locationCode );
            tip += "<nobr>" + QStyleSheet::escape( temp + ", " + weather.join( ", " ) ) + "</nobr>";
            QString apparent = m_weatherService->apparentTemperature( m_locationCode );
            if ( !apparent.isEmpty() && apparent != temp )
                tip += "<br><nobr>" + QStyleSheet::escape( i18n( "Feels like %1" ).arg( apparent ) ) + "</nobr>";
            if ( !wind.isEmpty() )
                tip += "<br><nobr>" + QStyleSheet::escape( i18n( "Wind: %1" ).arg( wind ) ) + "</nobr>";
            tip += "<br><nobr>" + QStyleSheet::escape( i18n( "Click for the forecast" ) ) + "</nobr>";
        }
    }
    else
    {
        tip += i18n( "No location is set: click to choose one." );
    }
    tip += "</qt>";

    QToolTip::remove(this);
    QToolTip::add(this, tip);

    kdDebug(12004) << "show weather: " << m_locationCode << " " << temp << " "
                   << wind << " " << pressure << " " << iconName << endl;

    m_lblTemp->setText(temp);
    m_lblWind->setText(wind);
    m_lblPres->setText(pressure);
    m_iconName = iconName;
    m_button->setPixmap( WeatherIcons::load( iconName, m_iconSize ) );
}

void dockwidget::initDock()
{
    kdDebug(12004) << "Init dockwidget " << endl;

    m_button= new WeatherButton(this,"m_button");

    m_lblTemp= new QLabel(this,"lblTemp");
    m_lblWind= new QLabel(this,"lblWind");
    m_lblPres= new QLabel(this,"lblPres");

    m_lblTemp->setBackgroundOrigin(AncestorOrigin);
    m_lblWind->setBackgroundOrigin(AncestorOrigin);
    m_lblPres->setBackgroundOrigin(AncestorOrigin);

    m_lblTemp->setMargin(0);
    m_lblWind->setMargin(0);
    m_lblPres->setMargin(0);

    QBoxLayout *mainLayout = new QBoxLayout(this, QBoxLayout::TopToBottom);
    mainLayout->setSpacing(0);
    mainLayout->setMargin(0);
    mainLayout->addWidget(m_button, 0, Qt::AlignCenter);

    QBoxLayout *layout = new QBoxLayout(mainLayout, QBoxLayout::TopToBottom);
    layout->setSpacing(0);
    layout->setMargin(0);
    layout->addWidget(m_lblTemp);
    layout->addWidget(m_lblWind);
    layout->addWidget(m_lblPres);

    updateFont();

    QTimer::singleShot( 0, this, SLOT( showWeather() ) );
}

/** The icon size for a panel @p thickness pixels thick: the largest
    standard size up to 70 % of it, so that the icon is drawn at its own
    size (crisp) with a margin around it. */
int dockwidget::iconSizeFor(int thickness)
{
    static const int sizes[] = { 128, 96, 64, 48, 32, 22, 16, 0 };
    int want = thickness * 7 / 10;
    for (int i = 0; sizes[i]; ++i)
        if (sizes[i] <= want)
            return sizes[i];
    return 16;
}

/** The panel font, smaller when @p lines lines of it do not fit into
    @p height pixels or when @p text is wider than @p width (0: any). */
void dockwidget::fitFont(int lines, int height, int width, const QString &text)
{
    m_font = KGlobalSettings::generalFont();
    QFontMetrics general(m_font);
    // a line is higher than the pixel size: shrink until it all fits
    for (int px = general.height(); ; --px) {
        if (px < general.height())
            m_font.setPixelSize(px);
        QFontMetrics fm(m_font);
        bool fits = (height <= 0 || lines * fm.height() <= height)
                    && (width <= 0 || text.isEmpty() || fm.width(text) <= width);
        if (fits || px <= 8)
            break;
    }
    updateFont();
}

int dockwidget::textLines() const
{
    return m_mode == ShowAll ? 3 : m_mode == ShowTempOnly ? 1 : 0;
}

int dockwidget::textWidth() const
{
    QFontMetrics fm(m_font);
    int w = 0;
    if (m_mode == ShowTempOnly || m_mode == ShowAll)
        w = fm.width(m_lblTemp->text());
    if (m_mode == ShowAll)
        w = QMAX(w, QMAX(fm.width(m_lblWind->text()), fm.width(m_lblPres->text())));
    return w;
}

void dockwidget::setIconSize(int size)
{
    if (size == m_iconSize)
        return;
    m_iconSize = size;
    m_button->setPixmap(WeatherIcons::load(m_iconName, m_iconSize));
}

/** resize the view **/
void dockwidget::resizeView( const QSize &size )
{
    kdDebug(12004) << "Changing to size " << size << endl;
    resize(size);
    QBoxLayout *box = static_cast<QBoxLayout*>(layout());
    int align = 0;

    if ( m_orientation == Horizontal )
    {
        // the icon, then the text beside it
        int h = size.height();
        setIconSize(iconSizeFor(h));
        box->setDirection(QBoxLayout::LeftToRight);
        box->setSpacing(textLines() ? 3 : 0);
        m_button->setFixedSize(m_iconSize + 2 * buttonMargin, h);
        align = Qt::AlignAuto | Qt::AlignVCenter;
    }
    else
    {
        // the icon, then the text below it
        int w = size.width();
        setIconSize(iconSizeFor(w));
        box->setDirection(QBoxLayout::TopToBottom);
        box->setSpacing(0);
        m_button->setFixedSize(w, m_iconSize + 2 * buttonMargin);
        align = Qt::AlignCenter;
    }
    m_lblTemp->setAlignment(align);
    m_lblWind->setAlignment(align);
    m_lblPres->setAlignment(align);
}

int dockwidget::widthForHeight(int h)
{
    int icon = iconSizeFor(h) + 2 * buttonMargin;
    if (!textLines())
        return icon;
    fitFont(textLines(), h, 0, QString::null);
    return icon + 3 + textWidth() + 2;
}

int dockwidget::heightForWidth( int w )
{
    int icon = iconSizeFor(w) + 2 * buttonMargin;
    if (!textLines())
        return icon;
    QString widest = m_lblTemp->text();
    if (m_mode == ShowAll) {
        QFontMetrics fm(KGlobalSettings::generalFont());
        if (fm.width(m_lblWind->text()) > fm.width(widest))
            widest = m_lblWind->text();
        if (fm.width(m_lblPres->text()) > fm.width(widest))
            widest = m_lblPres->text();
    }
    fitFont(textLines(), 0, w - 2, widest);
    return icon + textLines() * QFontMetrics(m_font).height() + 2;
}

void dockwidget::updateFont()
{
    m_lblTemp->setFont(m_font);
    m_lblWind->setFont(m_font);
    m_lblPres->setFont(m_font);
}

#include "dockwidget.moc"

// vim:ts=4:sw=4:et
