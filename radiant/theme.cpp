/*
   Copyright (C) 2001-2006, William Joseph.
   All Rights Reserved.

   This file is part of GtkRadiant.

   GtkRadiant is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   GtkRadiant is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with GtkRadiant; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include <QStyle>
#include <QApplication>
#include <QMenu>
#include <QActionGroup>
#include <QFile>

#include "preferences.h"
#include "mainframe.h"
#include "preferencesystem.h"
#include "stringio.h"
#include "stream/stringstream.h"
#include "gtkutil/image.h"


enum class ETheme{
	Default = 0,
	Fusion,
	Light,
	Dark,
	Darker,
	OneDark,
	Steam,
	Vaporwave,
	OneGrayDarker,
	OneGray,
	Lighter,
};

static ETheme s_theme = ETheme::Dark;

QString load_qss( const char *filename ){
	if( QFile file( QString( AppPath_get() ) + "themes/" + filename ); file.open( QIODevice::OpenModeFlag::ReadOnly ) )
		return file.readAll();
	return {};
}

void set_icon_theme( bool light ){
	static auto init = ( Bitmaps_generateLight( AppPath_get(), SettingsPath_get() ),
	                     QIcon::setThemeSearchPaths( QIcon::themeSearchPaths() << AppPath_get() << SettingsPath_get() ), 1 );
	(void)init;

	BitmapsPath_set( light? StringStream( SettingsPath_get(), "bitmaps_light/" )
	                      : StringStream( AppPath_get(), "bitmaps/" ) );
	QIcon::setThemeName( light? "bitmaps_light" : "bitmaps" );
}

void theme_set( ETheme theme ){
	s_theme = theme;
#ifdef WIN32
//	QSettings settings( "HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize", QSettings::NativeFormat );
//	if( settings.value( "AppsUseLightTheme" ) == 0 )
#endif
	static struct
	{
		bool is1stThemeApplication = true; // guard to not apply possibly wrong defaults while app is started with Default theme
		const QPalette palette = qApp->palette();
		const QString style = qApp->style()->objectName();
	}
	defaults;

	if( theme == ETheme::Default ){
		set_icon_theme( true );
		if( !defaults.is1stThemeApplication ){
			qApp->setPalette( defaults.palette );
			qApp->setStyleSheet( QString() );
			qApp->setStyle( defaults.style );
		}
	}
	else if( theme == ETheme::Fusion ){
		set_icon_theme( true );
		qApp->setPalette( defaults.palette );
		qApp->setStyleSheet( load_qss( "fusion.qss" ) ); //missing, stub to load custom qss
		qApp->setStyle( "Fusion" );
	}
	else if( theme == ETheme::Light ){
		set_icon_theme( true );
		qApp->setStyle( "Fusion" );
		QPalette lightPalette;
		const auto rgb = []( double r, double g, double b ){
			return QColor::fromRgbF( r, g, b );
		};
		const QColor background = rgb( 0.760417, 0.760417, 0.760417 );
		const QColor panel = rgb( 0.541667, 0.541667, 0.541667 );
		const QColor dropdown = QColor::fromRgbF( 1.0, 1.0, 1.0 );
		const QColor secondary = rgb( 0.921875, 0.921875, 0.921875 );
		const QColor midColor = rgb( 0.557292, 0.557292, 0.557292 );
		const QColor border = rgb( 0.458333, 0.458333, 0.458333 );
		const QColor text = rgb( 0.015625, 0.015625, 0.015625 );
		const QColor disabledText = rgb( 0.406250, 0.406250, 0.406250 );
		const QColor highlight = rgb( 0.0, 0.162029, 0.745404 );
		const QColor highlightText = rgb( 1.0, 1.0, 1.0 );
		const QColor selectInactive = rgb( 0.051215, 0.155931, 0.307292 );
		const QColor warning = rgb( 1.0, 0.479320, 0.0 );
		const QColor accentBlue = rgb( 0.019382, 0.496933, 1.0 );
		lightPalette.setColor( QPalette::Window, background );
		lightPalette.setColor( QPalette::WindowText, text );
		lightPalette.setColor( QPalette::Base, dropdown );
		lightPalette.setColor( QPalette::AlternateBase, panel );
		lightPalette.setColor( QPalette::ToolTipBase, QColor::fromRgbF( 1.0, 1.0, 1.0 ) );
		lightPalette.setColor( QPalette::ToolTipText, text );
		lightPalette.setColor( QPalette::Text, text );
		lightPalette.setColor( QPalette::Button, secondary );
		lightPalette.setColor( QPalette::ButtonText, text );
		lightPalette.setColor( QPalette::BrightText, warning );
		lightPalette.setColor( QPalette::Link, accentBlue );
		lightPalette.setColor( QPalette::Highlight, highlight );
		lightPalette.setColor( QPalette::HighlightedText, highlightText );
		lightPalette.setColor( QPalette::Light, panel );
		lightPalette.setColor( QPalette::Mid, border );
		lightPalette.setColor( QPalette::Midlight, midColor );
		lightPalette.setColor( QPalette::Shadow, border );
		lightPalette.setColor( QPalette::Disabled, QPalette::WindowText, disabledText );
		lightPalette.setColor( QPalette::Disabled, QPalette::Text, disabledText );
		lightPalette.setColor( QPalette::Disabled, QPalette::ButtonText, disabledText );
		lightPalette.setColor( QPalette::Disabled, QPalette::Highlight, selectInactive );
		lightPalette.setColor( QPalette::Inactive, QPalette::Highlight, selectInactive );
		lightPalette.setColor( QPalette::Inactive, QPalette::HighlightedText, highlightText );
		qApp->setPalette( lightPalette );
		qApp->setStyleSheet( load_qss( "light.qss" ) );
	}
	else if( theme == ETheme::OneDark ){
		set_icon_theme( false );
		qApp->setStyle( "Fusion" );
		QPalette oneDarkPalette;
		const auto rgb = []( double r, double g, double b ){
			return QColor::fromRgbF( r, g, b );
		};
		const QColor background = rgb( 0.010960, 0.012983, 0.016807 );
		const QColor panel = rgb( 0.021219, 0.025187, 0.034340 );
		const QColor inputOutline = rgb( 0.033105, 0.040915, 0.054480 );
		const QColor hover = rgb( 0.057805, 0.074214, 0.099899 );
		const QColor hover2 = rgb( 0.149960, 0.191202, 0.266356 );
		const QColor text = rgb( 0.527115, 0.527115, 0.527115 );
		const QColor disabledText = rgb( 0.033105, 0.040915, 0.054480 );
		const QColor highlight = rgb( 0.527115, 1.000000, 0.304987 );
		const QColor highlightText = rgb( 1.000000, 1.000000, 1.000000 );
		const QColor selectInactive = rgb( 0.146666, 0.200000, 0.120000 );
		const QColor accentBlue = rgb( 0.019382, 0.496933, 1.000000 );
		oneDarkPalette.setColor( QPalette::Window, background );
		oneDarkPalette.setColor( QPalette::WindowText, text );
		oneDarkPalette.setColor( QPalette::Base, background );
		oneDarkPalette.setColor( QPalette::AlternateBase, panel );
		oneDarkPalette.setColor( QPalette::ToolTipBase, highlightText );
		oneDarkPalette.setColor( QPalette::ToolTipText, text );
		oneDarkPalette.setColor( QPalette::Text, text );
		oneDarkPalette.setColor( QPalette::Button, panel );
		oneDarkPalette.setColor( QPalette::ButtonText, text );
		oneDarkPalette.setColor( QPalette::BrightText, highlightText );
		oneDarkPalette.setColor( QPalette::Link, accentBlue );
		oneDarkPalette.setColor( QPalette::Highlight, highlight );
		oneDarkPalette.setColor( QPalette::HighlightedText, highlightText );
		oneDarkPalette.setColor( QPalette::Light, inputOutline );
		oneDarkPalette.setColor( QPalette::Mid, hover );
		oneDarkPalette.setColor( QPalette::Midlight, hover2 );
		oneDarkPalette.setColor( QPalette::Shadow, QColor::fromRgbF( 0.0, 0.0, 0.0 ) );
		oneDarkPalette.setColor( QPalette::Disabled, QPalette::WindowText, disabledText );
		oneDarkPalette.setColor( QPalette::Disabled, QPalette::Text, disabledText );
		oneDarkPalette.setColor( QPalette::Disabled, QPalette::ButtonText, disabledText );
		oneDarkPalette.setColor( QPalette::Disabled, QPalette::Highlight, selectInactive );
		oneDarkPalette.setColor( QPalette::Inactive, QPalette::Highlight, selectInactive );
		oneDarkPalette.setColor( QPalette::Inactive, QPalette::HighlightedText, highlightText );
		qApp->setPalette( oneDarkPalette );
		qApp->setStyleSheet( load_qss( "onedark.qss" ) );
	}
	else if( theme == ETheme::Steam ){
		set_icon_theme( false );
		qApp->setStyle( "Fusion" );
		QPalette steamPalette;
		const auto rgb = []( double r, double g, double b ){
			return QColor::fromRgbF( r, g, b );
		};
		const QColor background = rgb( 0.048172, 0.061246, 0.038204 );
		const QColor panel = rgb( 0.048172, 0.061246, 0.038204 );
		const QColor header = rgb( 0.072272, 0.097587, 0.057805 );
		const QColor secondary = rgb( 0.156250, 0.130678, 0.022386 );
		const QColor border = rgb( 0.201556, 0.201556, 0.198069 );
		const QColor inputOutline = rgb( 0.021219, 0.027321, 0.015996 );
		const QColor hover2 = rgb( 0.234375, 0.196017, 0.033579 );
		const QColor text = rgb( 0.527115, 0.527115, 0.527115 );
		const QColor disabledText = rgb( 0.083333, 0.069695, 0.011939 );
		const QColor highlight = rgb( 0.276042, 0.230865, 0.039549 );
		const QColor highlightText = QColor::fromRgbF( 1.0, 1.0, 1.0 );
		const QColor accentBlue = rgb( 0.019382, 0.496933, 1.000000 );
		const QColor warning = rgb( 1.0, 0.479320, 0.0 );
		const QColor error = rgb( 0.863157, 0.035601, 0.035601 );
		steamPalette.setColor( QPalette::Window, background );
		steamPalette.setColor( QPalette::WindowText, text );
		steamPalette.setColor( QPalette::Base, background );
		steamPalette.setColor( QPalette::AlternateBase, secondary );
		steamPalette.setColor( QPalette::ToolTipBase, highlightText );
		steamPalette.setColor( QPalette::ToolTipText, text );
		steamPalette.setColor( QPalette::Text, text );
		steamPalette.setColor( QPalette::Button, panel );
		steamPalette.setColor( QPalette::ButtonText, text );
		steamPalette.setColor( QPalette::BrightText, highlightText );
		steamPalette.setColor( QPalette::Link, accentBlue );
		steamPalette.setColor( QPalette::Highlight, highlight );
		steamPalette.setColor( QPalette::HighlightedText, highlightText );
		steamPalette.setColor( QPalette::Light, header );
		steamPalette.setColor( QPalette::Mid, border );
		steamPalette.setColor( QPalette::Midlight, hover2 );
		steamPalette.setColor( QPalette::Shadow, inputOutline );
		steamPalette.setColor( QPalette::Disabled, QPalette::WindowText, disabledText );
		steamPalette.setColor( QPalette::Disabled, QPalette::Text, disabledText );
		steamPalette.setColor( QPalette::Disabled, QPalette::ButtonText, disabledText );
		steamPalette.setColor( QPalette::Disabled, QPalette::Highlight, secondary );
		steamPalette.setColor( QPalette::Inactive, QPalette::Highlight, secondary );
		steamPalette.setColor( QPalette::Inactive, QPalette::HighlightedText, highlightText );
		steamPalette.setColor( QPalette::Disabled, QPalette::BrightText, warning );
		steamPalette.setColor( QPalette::Disabled, QPalette::Link, accentBlue );
		steamPalette.setColor( QPalette::Disabled, QPalette::Mid, error );
		qApp->setPalette( steamPalette );
		qApp->setStyleSheet( load_qss( "steam.qss" ) );
	}
	else if( theme == ETheme::Vaporwave ){
		set_icon_theme( false );
		qApp->setStyle( "Fusion" );
		QPalette vaporPalette;
		const auto rgb = []( double r, double g, double b ){
			return QColor::fromRgbF( r, g, b );
		};
		const QColor background = rgb( 0.012440, 0.013748, 0.031250 );
		const QColor panel = rgb( 0.012441, 0.013748, 0.031250 );
		const QColor dropdown = rgb( 0.039394, 0.043535, 0.098958 );
		const QColor header = rgb( 0.008293, 0.009165, 0.020833 );
		const QColor hover = rgb( 0.041467, 0.045826, 0.104167 );
		const QColor hover2 = rgb( 0.215861, 0.215861, 0.215861 );
		const QColor highlight = rgb( 0.000000, 0.162029, 0.745404 );
		const QColor text = rgb( 0.619792, 0.619792, 0.619792 );
		const QColor selectParent = rgb( 0.135916, 0.051270, 0.140625 );
		const QColor selectHover = rgb( 0.260867, 0.081814, 0.270833 );
		const QColor accentBlue = rgb( 0.019382, 0.496933, 1.000000 );
		const QColor accentYellow = rgb( 1.000000, 0.715693, 0.010330 );
		const QColor disabledText = rgb( 0.210082, 0.147922, 0.213542 );
		vaporPalette.setColor( QPalette::Window, background );
		vaporPalette.setColor( QPalette::WindowText, text );
		vaporPalette.setColor( QPalette::Base, dropdown );
		vaporPalette.setColor( QPalette::AlternateBase, panel );
		vaporPalette.setColor( QPalette::ToolTipBase, accentYellow );
		vaporPalette.setColor( QPalette::ToolTipText, text );
		vaporPalette.setColor( QPalette::Text, text );
		vaporPalette.setColor( QPalette::Button, panel );
		vaporPalette.setColor( QPalette::ButtonText, text );
		vaporPalette.setColor( QPalette::BrightText, accentYellow );
		vaporPalette.setColor( QPalette::Link, accentBlue );
		vaporPalette.setColor( QPalette::Highlight, highlight );
		vaporPalette.setColor( QPalette::HighlightedText, QColor::fromRgbF( 1.0, 1.0, 1.0 ) );
		vaporPalette.setColor( QPalette::Mid, hover2 );
		vaporPalette.setColor( QPalette::Midlight, hover );
		vaporPalette.setColor( QPalette::Light, header );
		vaporPalette.setColor( QPalette::Shadow, selectParent );
		vaporPalette.setColor( QPalette::ToolTipBase, accentYellow );
		vaporPalette.setColor( QPalette::Disabled, QPalette::WindowText, disabledText );
		vaporPalette.setColor( QPalette::Disabled, QPalette::Text, disabledText );
		vaporPalette.setColor( QPalette::Disabled, QPalette::ButtonText, disabledText );
		vaporPalette.setColor( QPalette::Disabled, QPalette::Highlight, selectHover );
		vaporPalette.setColor( QPalette::Inactive, QPalette::Highlight, selectHover );
		vaporPalette.setColor( QPalette::Inactive, QPalette::HighlightedText, QColor::fromRgbF( 1.0, 1.0, 1.0 ) );
		qApp->setPalette( vaporPalette );
		qApp->setStyleSheet( load_qss( "vaporwave.qss" ) );
	}
	else if( theme == ETheme::OneGrayDarker ){
		set_icon_theme( false );
		qApp->setStyle( "Fusion" );
		QPalette greyPalette;
		const auto rgb = []( double r, double g, double b ){
			return QColor::fromRgbF( r, g, b );
		};
		const QColor background = rgb( 0.012000, 0.012000, 0.012000 );
		const QColor panel = rgb( 0.026000, 0.026000, 0.026000 );
		const QColor dropdown = rgb( 0.012000, 0.012000, 0.012000 );
		const QColor header = rgb( 0.040000, 0.040000, 0.040000 );
		const QColor hover = rgb( 0.099899, 0.099899, 0.099899 );
		const QColor hover2 = rgb( 0.266356, 0.266356, 0.266356 );
		const QColor selectParent = rgb( 0.050000, 0.050000, 0.050000 );
		const QColor text = rgb( 0.527115, 0.527115, 0.527115 );
		const QColor select = rgb( 0.219999, 0.300000, 0.180000 );
		const QColor selectInactive = rgb( 0.146666, 0.200000, 0.120000 );
		const QColor highlight = rgb( 0.527115, 1.000000, 0.304987 );
		const QColor accentBlue = rgb( 0.019382, 0.496933, 1.000000 );
		const QColor accentYellow = rgb( 1.000000, 0.715693, 0.010330 );
		const QColor disabledText = rgb( 0.050000, 0.050000, 0.050000 );
		greyPalette.setColor( QPalette::Window, background );
		greyPalette.setColor( QPalette::WindowText, text );
		greyPalette.setColor( QPalette::Base, dropdown );
		greyPalette.setColor( QPalette::AlternateBase, panel );
		greyPalette.setColor( QPalette::ToolTipBase, accentYellow );
		greyPalette.setColor( QPalette::ToolTipText, text );
		greyPalette.setColor( QPalette::Text, text );
		greyPalette.setColor( QPalette::Button, panel );
		greyPalette.setColor( QPalette::ButtonText, text );
		greyPalette.setColor( QPalette::BrightText, accentYellow );
		greyPalette.setColor( QPalette::Link, accentBlue );
		greyPalette.setColor( QPalette::Highlight, highlight );
		greyPalette.setColor( QPalette::HighlightedText, QColor::fromRgbF( 1.0, 1.0, 1.0 ) );
		greyPalette.setColor( QPalette::Mid, hover2 );
		greyPalette.setColor( QPalette::Midlight, hover );
		greyPalette.setColor( QPalette::Light, header );
		greyPalette.setColor( QPalette::Shadow, select );
		greyPalette.setColor( QPalette::Disabled, QPalette::WindowText, disabledText );
		greyPalette.setColor( QPalette::Disabled, QPalette::Text, disabledText );
		greyPalette.setColor( QPalette::Disabled, QPalette::ButtonText, disabledText );
		greyPalette.setColor( QPalette::Disabled, QPalette::Highlight, selectInactive );
		greyPalette.setColor( QPalette::Disabled, QPalette::Mid, selectParent );
		greyPalette.setColor( QPalette::Inactive, QPalette::Highlight, selectInactive );
		greyPalette.setColor( QPalette::Inactive, QPalette::HighlightedText, QColor::fromRgbF( 1.0, 1.0, 1.0 ) );
		qApp->setPalette( greyPalette );
		qApp->setStyleSheet( load_qss( "onegraydarker.qss" ) );
	}
	else if( theme == ETheme::OneGray ){
		set_icon_theme( false );
		qApp->setStyle( "Fusion" );
		QPalette greyPalette;
		const auto rgb = []( double r, double g, double b ){
			return QColor::fromRgbF( r, g, b );
		};
		const QColor background = rgb( 0.016807, 0.016807, 0.016807 );
		const QColor panel = rgb( 0.034340, 0.034340, 0.034340 );
		const QColor dropdown = rgb( 0.016807, 0.016807, 0.016807 );
		const QColor header = rgb( 0.054480, 0.054480, 0.054480 );
		const QColor hover = rgb( 0.099899, 0.099899, 0.099899 );
		const QColor hover2 = rgb( 0.266356, 0.266356, 0.266356 );
		const QColor text = rgb( 0.527115, 0.527115, 0.527115 );
		const QColor selectInactive = rgb( 0.146666, 0.200000, 0.120000 );
		const QColor selectParent = rgb( 0.054480, 0.054480, 0.054480 );
		const QColor highlight = rgb( 0.527115, 1.000000, 0.304987 );
		const QColor accentBlue = rgb( 0.019382, 0.496933, 1.000000 );
		const QColor accentYellow = rgb( 1.000000, 0.715693, 0.010330 );
		const QColor disabledText = rgb( 0.050000, 0.050000, 0.050000 );
		greyPalette.setColor( QPalette::Window, background );
		greyPalette.setColor( QPalette::WindowText, text );
		greyPalette.setColor( QPalette::Base, dropdown );
		greyPalette.setColor( QPalette::AlternateBase, panel );
		greyPalette.setColor( QPalette::ToolTipBase, accentYellow );
		greyPalette.setColor( QPalette::ToolTipText, text );
		greyPalette.setColor( QPalette::Text, text );
		greyPalette.setColor( QPalette::Button, panel );
		greyPalette.setColor( QPalette::ButtonText, text );
		greyPalette.setColor( QPalette::BrightText, accentYellow );
		greyPalette.setColor( QPalette::Link, accentBlue );
		greyPalette.setColor( QPalette::Highlight, highlight );
		greyPalette.setColor( QPalette::HighlightedText, QColor::fromRgbF( 1.0, 1.0, 1.0 ) );
		greyPalette.setColor( QPalette::Mid, hover2 );
		greyPalette.setColor( QPalette::Midlight, hover );
		greyPalette.setColor( QPalette::Light, header );
		greyPalette.setColor( QPalette::Shadow, selectParent );
		greyPalette.setColor( QPalette::Disabled, QPalette::WindowText, disabledText );
		greyPalette.setColor( QPalette::Disabled, QPalette::Text, disabledText );
		greyPalette.setColor( QPalette::Disabled, QPalette::ButtonText, disabledText );
		greyPalette.setColor( QPalette::Disabled, QPalette::Highlight, selectInactive );
		greyPalette.setColor( QPalette::Inactive, QPalette::Highlight, selectInactive );
		greyPalette.setColor( QPalette::Inactive, QPalette::HighlightedText, QColor::fromRgbF( 1.0, 1.0, 1.0 ) );
		qApp->setPalette( greyPalette );
		qApp->setStyleSheet( load_qss( "onegray.qss" ) );
	}
	else if( theme == ETheme::Lighter ){
		set_icon_theme( false );
		qApp->setStyle( "Fusion" );
		QPalette lighterPalette;
		const auto rgb = []( double r, double g, double b ){
			return QColor::fromRgbF( r, g, b );
		};
		const QColor background = rgb( 0.062500, 0.062500, 0.062500 );
		const QColor panel = rgb( 0.166667, 0.166667, 0.166667 );
		const QColor dropdown = rgb( 0.165132, 0.165132, 0.165132 );
		const QColor hover = rgb( 0.432292, 0.432292, 0.432292 );
		const QColor hover2 = rgb( 0.557292, 0.557292, 0.557292 );
		const QColor text = QColor::fromRgbF( 1.0, 1.0, 1.0 );
		const QColor select = rgb( 0.000000, 0.162029, 0.745404 );
		const QColor selectInactive = rgb( 0.051269, 0.095307, 0.158961 );
		const QColor highlight = rgb( 0.000000, 0.162029, 0.745404 );
		const QColor accentBlue = rgb( 0.019382, 0.496933, 1.000000 );
		const QColor warning = rgb( 1.000000, 0.479320, 0.000000 );
		lighterPalette.setColor( QPalette::Window, background );
		lighterPalette.setColor( QPalette::WindowText, text );
		lighterPalette.setColor( QPalette::Base, dropdown );
		lighterPalette.setColor( QPalette::AlternateBase, panel );
		lighterPalette.setColor( QPalette::ToolTipBase, QColor::fromRgbF( 1.0, 1.0, 1.0 ) );
		lighterPalette.setColor( QPalette::ToolTipText, text );
		lighterPalette.setColor( QPalette::Text, text );
		lighterPalette.setColor( QPalette::Button, panel );
		lighterPalette.setColor( QPalette::ButtonText, text );
		lighterPalette.setColor( QPalette::BrightText, warning );
		lighterPalette.setColor( QPalette::Link, accentBlue );
		lighterPalette.setColor( QPalette::Highlight, highlight );
		lighterPalette.setColor( QPalette::HighlightedText, text );
		lighterPalette.setColor( QPalette::Mid, hover );
		lighterPalette.setColor( QPalette::Midlight, hover2 );
		lighterPalette.setColor( QPalette::Light, rgb( 0.125000, 0.125000, 0.125000 ) );
		lighterPalette.setColor( QPalette::Shadow, select );
		lighterPalette.setColor( QPalette::Disabled, QPalette::WindowText, selectInactive );
		lighterPalette.setColor( QPalette::Disabled, QPalette::Text, selectInactive );
		lighterPalette.setColor( QPalette::Disabled, QPalette::ButtonText, selectInactive );
		lighterPalette.setColor( QPalette::Disabled, QPalette::Highlight, selectInactive );
		lighterPalette.setColor( QPalette::Inactive, QPalette::Highlight, selectInactive );
		lighterPalette.setColor( QPalette::Inactive, QPalette::HighlightedText, text );
		qApp->setPalette( lighterPalette );
		qApp->setStyleSheet( load_qss( "lighter.qss" ) );
	}
	else if( theme == ETheme::Dark ){
		set_icon_theme( false );
		qApp->setStyle( "Fusion" );
		QPalette darkPalette;
		const QColor darkColor = QColor( 83, 84, 81 );
		const QColor disabledColor = QColor( 127, 127, 127 );
		const QColor baseColor( 46, 52, 54 );
		darkPalette.setColor( QPalette::Window, darkColor );
		darkPalette.setColor( QPalette::WindowText, Qt::white );
		darkPalette.setColor( QPalette::Disabled, QPalette::WindowText, disabledColor );
		darkPalette.setColor( QPalette::Base, baseColor );
		darkPalette.setColor( QPalette::AlternateBase, baseColor.darker( 130 ) );
		darkPalette.setColor( QPalette::ToolTipBase, Qt::white );
		darkPalette.setColor( QPalette::ToolTipText, Qt::white );
		darkPalette.setColor( QPalette::Text, Qt::white );
		darkPalette.setColor( QPalette::Disabled, QPalette::Text, disabledColor );
		darkPalette.setColor( QPalette::Disabled, QPalette::Light, disabledColor ); // disabled menu text shadow
		darkPalette.setColor( QPalette::Button, darkColor.lighter( 130 ) ); //<>
		darkPalette.setColor( QPalette::ButtonText, Qt::white );
		darkPalette.setColor( QPalette::Disabled, QPalette::ButtonText, disabledColor.lighter( 130 ) ); //<>
		darkPalette.setColor( QPalette::BrightText, Qt::red );
		darkPalette.setColor( QPalette::Link, QColor( 42, 130, 218 ) );

		darkPalette.setColor( QPalette::Highlight, QColor( 250, 203, 129 ) ); //<>
		darkPalette.setColor( QPalette::Inactive, QPalette::Highlight, disabledColor );
		darkPalette.setColor( QPalette::HighlightedText, Qt::black );
		darkPalette.setColor( QPalette::Disabled, QPalette::HighlightedText, disabledColor );

		qApp->setPalette( darkPalette );

		qApp->setStyleSheet( load_qss( "dark.qss" ) );
	}
	else if( theme == ETheme::Darker ){
		set_icon_theme( false );
		qApp->setStyle( "Fusion" );
		QPalette darkPalette;
		const QColor darkColor = QColor( 45, 45, 45 );
		const QColor disabledColor = QColor( 127, 127, 127 );
		const QColor baseColor( 18, 18, 18 );
		darkPalette.setColor( QPalette::Window, darkColor );
		darkPalette.setColor( QPalette::WindowText, Qt::white );
		darkPalette.setColor( QPalette::Disabled, QPalette::WindowText, disabledColor );
		darkPalette.setColor( QPalette::Base, baseColor );
		darkPalette.setColor( QPalette::AlternateBase, baseColor.darker( 130 ) );
		darkPalette.setColor( QPalette::ToolTipBase, Qt::white );
		darkPalette.setColor( QPalette::ToolTipText, Qt::white );
		darkPalette.setColor( QPalette::Text, Qt::white );
		darkPalette.setColor( QPalette::Disabled, QPalette::Text, disabledColor );
		darkPalette.setColor( QPalette::Disabled, QPalette::Light, disabledColor ); // disabled menu text shadow
		darkPalette.setColor( QPalette::Button, darkColor );
		darkPalette.setColor( QPalette::ButtonText, Qt::white );
		darkPalette.setColor( QPalette::Disabled, QPalette::ButtonText, disabledColor );
		darkPalette.setColor( QPalette::BrightText, Qt::red );
		darkPalette.setColor( QPalette::Link, QColor( 42, 130, 218 ) );

		darkPalette.setColor( QPalette::Highlight, QColor( 42, 130, 218 ) );
		darkPalette.setColor( QPalette::Inactive, QPalette::Highlight, disabledColor );
		darkPalette.setColor( QPalette::HighlightedText, Qt::black );
		darkPalette.setColor( QPalette::Disabled, QPalette::HighlightedText, disabledColor );

		qApp->setPalette( darkPalette );

		qApp->setStyleSheet( load_qss( "darker.qss" ) );
	}

	defaults.is1stThemeApplication = false;
}

void theme_construct_menu( class QMenu *menu ){
	auto *m = menu->addMenu( "GUI Theme" );
	m->setTearOffEnabled( g_Layout_enableDetachableMenus.m_value );
	auto *group = new QActionGroup( m );

	for( const auto *name : { "Default", "Fusion", "Light", "Dark", "Darker", "OneDark", "Steam", "Vaporwave", "OneGrayDarker", "OneGray", "Lighter" } )
	{
		auto *a = m->addAction( name );
		a->setCheckable( true );
		group->addAction( a );
	}
	// init radio
	if( const int value = static_cast<int>( s_theme ); 0 <= value && value < group->actions().size() )
		group->actions().at( value )->setChecked( true );

	QObject::connect( group, &QActionGroup::triggered, []( QAction *action ){
		theme_set( static_cast<ETheme>( action->actionGroup()->actions().indexOf( action ) ) );
	} );
}

void ThemeImport( int value ){
	s_theme = static_cast<ETheme>( value );
}
typedef FreeCaller<void(int), ThemeImport> ThemeImportCaller;

void ThemeExport( const IntImportCallback& importer ){
	importer( static_cast<int>( s_theme ) );
}
typedef FreeCaller<void(const IntImportCallback&), ThemeExport> ThemeExportCaller;


void theme_construct(){
	theme_set( s_theme ); // set theme here, not in importer, so it's set on the very 1st start too (when there is no preference to load)
}

void theme_registerGlobalPreference( class PreferenceSystem& preferences ){
	preferences.registerPreference( "GUITheme", makeIntStringImportCallback( ThemeImportCaller() ), makeIntStringExportCallback( ThemeExportCaller() ) );
}
