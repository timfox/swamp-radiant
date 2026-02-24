#include "video_workbench.h"

#include "mainframe.h"
#include "debugging/debugging.h"
#include "stream/stringstream.h"

#include <QMainWindow>
#include <QDockWidget>
#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QSlider>
#include <QCheckBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QSettings>
#include <QMenu>
#include <QMessageBox>

#include <limits>

#if __has_include( <QMediaContent> ) && __has_include( <QMediaPlayer> ) && __has_include( <QMediaPlaylist> )
	#include <QMediaContent>
	#include <QMediaPlayer>
	#include <QMediaPlaylist>
	#define RADIANT_QT_MULTIMEDIA_AVAILABLE 1
#else
	#define RADIANT_QT_MULTIMEDIA_AVAILABLE 0
#endif

#if __has_include( <QVideoWidget> )
	#include <QVideoWidget>
	#define RADIANT_QT_VIDEO_WIDGET_AVAILABLE 1
#else
	#define RADIANT_QT_VIDEO_WIDGET_AVAILABLE 0
#endif

namespace
{

#if RADIANT_QT_MULTIMEDIA_AVAILABLE && RADIANT_QT_VIDEO_WIDGET_AVAILABLE
QDockWidget* g_videoDock{};
QMediaPlayer* g_videoPlayer{};
QVideoWidget* g_videoWidget{};
QSlider* g_videoSeekSlider{};
QSlider* g_videoVolumeSlider{};
QLabel* g_videoPathLabel{};
QLabel* g_videoTimeLabel{};
QCheckBox* g_videoLoopCheck{};

const char* const c_videoSettingsPrefix = "VideoWorkbench/";

QSettings& VideoWorkbench_settings(){
	static QSettings settings;
	return settings;
}

QString VideoWorkbench_setting( const char* key, const QString& fallback = {} ){
	return VideoWorkbench_settings().value( StringStream( c_videoSettingsPrefix, key ).c_str(), fallback ).toString();
}

bool VideoWorkbench_settingBool( const char* key, bool fallback ){
	return VideoWorkbench_settings().value( StringStream( c_videoSettingsPrefix, key ).c_str(), fallback ).toBool();
}

int VideoWorkbench_settingInt( const char* key, int fallback ){
	return VideoWorkbench_settings().value( StringStream( c_videoSettingsPrefix, key ).c_str(), fallback ).toInt();
}

void VideoWorkbench_setSetting( const char* key, const QVariant& value ){
	VideoWorkbench_settings().setValue( StringStream( c_videoSettingsPrefix, key ).c_str(), value );
}

QString VideoWorkbench_formatDuration( qint64 milliseconds ){
	if ( milliseconds < 0 ) {
		milliseconds = 0;
	}
	const int secondsTotal = int( milliseconds / 1000 );
	const int seconds = secondsTotal % 60;
	const int minutesTotal = secondsTotal / 60;
	const int minutes = minutesTotal % 60;
	const int hours = minutesTotal / 60;
	return hours > 0
		? QString( "%1:%2:%3" ).arg( hours ).arg( minutes, 2, 10, QLatin1Char( '0' ) ).arg( seconds, 2, 10, QLatin1Char( '0' ) )
		: QString( "%1:%2" ).arg( minutes ).arg( seconds, 2, 10, QLatin1Char( '0' ) );
}

QString VideoWorkbench_defaultDirectory(){
	const QString last = VideoWorkbench_setting( "LastDirectory" );
	if ( !last.isEmpty() ) {
		return last;
	}
	return QString::fromLatin1( EnginePath_get() );
}

void VideoWorkbench_setLastDirectory( const QString& path ){
	if ( path.isEmpty() ) {
		return;
	}
	const QFileInfo info( path );
	const QString directory = info.isDir() ? info.absoluteFilePath() : info.absolutePath();
	if ( !directory.isEmpty() ) {
		VideoWorkbench_setSetting( "LastDirectory", directory );
	}
}

void VideoWorkbench_updateTimeLabel(){
	if ( g_videoPlayer == nullptr || g_videoTimeLabel == nullptr ) {
		return;
	}
	g_videoTimeLabel->setText(
	    StringStream(
	        VideoWorkbench_formatDuration( g_videoPlayer->position() ).toUtf8().constData(),
	        " / ",
	        VideoWorkbench_formatDuration( g_videoPlayer->duration() ).toUtf8().constData()
	    ).c_str()
	);
}

void VideoWorkbench_openFile( const QString& path, bool autoplay = true ){
	if ( g_videoPlayer == nullptr || path.isEmpty() ) {
		return;
	}
	const auto absolute = QFileInfo( path ).absoluteFilePath();
	g_videoPlayer->setMedia( QUrl::fromLocalFile( absolute ) );
	if ( g_videoPathLabel != nullptr ) {
		g_videoPathLabel->setText( StringStream( "File: ", absolute.toUtf8().constData() ).c_str() );
	}
	VideoWorkbench_setSetting( "LastFile", absolute );
	VideoWorkbench_setLastDirectory( absolute );
	if ( autoplay ) {
		g_videoPlayer->play();
	}
}

void VideoWorkbench_showContextMenu( const QPoint& pos ){
	if ( g_videoWidget == nullptr ) {
		return;
	}
	QMenu menu( g_videoWidget );
	auto* openAction = menu.addAction( "Open Video..." );
	menu.addSeparator();
	auto* playAction = menu.addAction( "Play" );
	auto* pauseAction = menu.addAction( "Pause" );
	auto* stopAction = menu.addAction( "Stop" );
	menu.addSeparator();
	auto* loopAction = menu.addAction( "Loop" );
	loopAction->setCheckable( true );
	loopAction->setChecked( g_videoLoopCheck != nullptr && g_videoLoopCheck->isChecked() );

	if ( QAction* chosen = menu.exec( g_videoWidget->mapToGlobal( pos ) ) ) {
		if ( chosen == openAction ) {
			const auto path = QFileDialog::getOpenFileName(
			    MainFrame_getWindow(),
			    "Open Cinematic Video",
			    VideoWorkbench_defaultDirectory(),
			    "Video Files (*.mp4 *.mkv *.webm *.avi *.mov *.ogv *.m4v);;All Files (*)"
			);
			VideoWorkbench_openFile( path );
		}
		else if ( chosen == playAction && g_videoPlayer != nullptr ) {
			g_videoPlayer->play();
		}
		else if ( chosen == pauseAction && g_videoPlayer != nullptr ) {
			g_videoPlayer->pause();
		}
		else if ( chosen == stopAction && g_videoPlayer != nullptr ) {
			g_videoPlayer->stop();
		}
		else if ( chosen == loopAction && g_videoLoopCheck != nullptr ) {
			g_videoLoopCheck->setChecked( !g_videoLoopCheck->isChecked() );
		}
	}
}

#endif

}

#if RADIANT_QT_MULTIMEDIA_AVAILABLE && RADIANT_QT_VIDEO_WIDGET_AVAILABLE
void VideoWorkbench_open(){
	if ( g_videoDock == nullptr ) {
		return;
	}
	g_videoDock->show();
	g_videoDock->raise();
}

void VideoWorkbench_createDock( QMainWindow* window ){
	if ( window == nullptr || g_videoDock != nullptr ) {
		return;
	}

	g_videoDock = new QDockWidget( "Cinematic Player", window );
	g_videoDock->setObjectName( "dock_cinematic_player" );

	auto* root = new QWidget( g_videoDock );
	auto* layout = new QVBoxLayout( root );

	auto* topButtons = new QHBoxLayout();
	auto* openButton = new QPushButton( "Open Video...", root );
	auto* playButton = new QPushButton( "Play", root );
	auto* pauseButton = new QPushButton( "Pause", root );
	auto* stopButton = new QPushButton( "Stop", root );
	topButtons->addWidget( openButton );
	topButtons->addWidget( playButton );
	topButtons->addWidget( pauseButton );
	topButtons->addWidget( stopButton );
	layout->addLayout( topButtons );

	g_videoWidget = new QVideoWidget( root );
	g_videoWidget->setMinimumHeight( 220 );
	g_videoWidget->setContextMenuPolicy( Qt::CustomContextMenu );
	layout->addWidget( g_videoWidget, 1 );

	g_videoSeekSlider = new QSlider( Qt::Horizontal, root );
	g_videoSeekSlider->setRange( 0, 0 );
	layout->addWidget( g_videoSeekSlider );

	auto* statusRow = new QHBoxLayout();
	g_videoPathLabel = new QLabel( "File: (none)", root );
	g_videoPathLabel->setTextInteractionFlags( Qt::TextSelectableByMouse );
	g_videoTimeLabel = new QLabel( "0:00 / 0:00", root );
	statusRow->addWidget( g_videoPathLabel, 1 );
	statusRow->addWidget( g_videoTimeLabel );
	layout->addLayout( statusRow );

	auto* controlsRow = new QHBoxLayout();
	controlsRow->addWidget( new QLabel( "Volume", root ) );
	g_videoVolumeSlider = new QSlider( Qt::Horizontal, root );
	g_videoVolumeSlider->setRange( 0, 100 );
	g_videoVolumeSlider->setValue( qBound( 0, VideoWorkbench_settingInt( "Volume", 80 ), 100 ) );
	g_videoLoopCheck = new QCheckBox( "Loop", root );
	g_videoLoopCheck->setChecked( VideoWorkbench_settingBool( "Loop", false ) );
	controlsRow->addWidget( g_videoVolumeSlider, 1 );
	controlsRow->addWidget( g_videoLoopCheck );
	layout->addLayout( controlsRow );

	g_videoDock->setWidget( root );
	window->addDockWidget( Qt::BottomDockWidgetArea, g_videoDock );
	g_videoDock->hide();

	g_videoPlayer = new QMediaPlayer( g_videoDock );
	g_videoPlayer->setVideoOutput( g_videoWidget );
	g_videoPlayer->setVolume( g_videoVolumeSlider->value() );

	QObject::connect( openButton, &QPushButton::clicked, [](){
		const auto path = QFileDialog::getOpenFileName(
		    MainFrame_getWindow(),
		    "Open Cinematic Video",
		    VideoWorkbench_defaultDirectory(),
		    "Video Files (*.mp4 *.mkv *.webm *.avi *.mov *.ogv *.m4v);;All Files (*)"
		);
		VideoWorkbench_openFile( path );
	} );
	QObject::connect( playButton, &QPushButton::clicked, [](){
		if ( g_videoPlayer != nullptr ) {
			g_videoPlayer->play();
		}
	} );
	QObject::connect( pauseButton, &QPushButton::clicked, [](){
		if ( g_videoPlayer != nullptr ) {
			g_videoPlayer->pause();
		}
	} );
	QObject::connect( stopButton, &QPushButton::clicked, [](){
		if ( g_videoPlayer != nullptr ) {
			g_videoPlayer->stop();
		}
	} );
	QObject::connect( g_videoSeekSlider, &QSlider::sliderMoved, []( int value ){
		if ( g_videoPlayer != nullptr ) {
			g_videoPlayer->setPosition( value );
		}
	} );
	QObject::connect( g_videoSeekSlider, &QSlider::sliderReleased, [](){
		if ( g_videoPlayer != nullptr && g_videoSeekSlider != nullptr ) {
			g_videoPlayer->setPosition( g_videoSeekSlider->value() );
		}
	} );
	QObject::connect( g_videoVolumeSlider, &QSlider::valueChanged, []( int value ){
		if ( g_videoPlayer != nullptr ) {
			g_videoPlayer->setVolume( value );
		}
		VideoWorkbench_setSetting( "Volume", value );
	} );
	QObject::connect( g_videoLoopCheck, &QCheckBox::toggled, []( bool loopEnabled ){
		VideoWorkbench_setSetting( "Loop", loopEnabled );
	} );
	QObject::connect( g_videoWidget, &QWidget::customContextMenuRequested, []( const QPoint& pos ){
		VideoWorkbench_showContextMenu( pos );
	} );
	QObject::connect( g_videoPlayer, &QMediaPlayer::positionChanged, []( qint64 position ){
		if ( g_videoSeekSlider != nullptr && !g_videoSeekSlider->isSliderDown() ) {
			g_videoSeekSlider->setValue( int( position ) );
		}
		VideoWorkbench_updateTimeLabel();
	} );
	QObject::connect( g_videoPlayer, &QMediaPlayer::durationChanged, []( qint64 duration ){
		if ( g_videoSeekSlider != nullptr ) {
			g_videoSeekSlider->setRange( 0, int( qBound<qint64>( 0, duration, std::numeric_limits<int>::max() ) ) );
		}
		VideoWorkbench_updateTimeLabel();
	} );
	QObject::connect( g_videoPlayer, QOverload<QMediaPlayer::Error>::of( &QMediaPlayer::error ), []( QMediaPlayer::Error error ){
		if ( error == QMediaPlayer::NoError || g_videoPlayer == nullptr ) {
			return;
		}
		globalWarningStream() << "video playback error: " << g_videoPlayer->errorString().toUtf8().constData() << '\n';
	} );
	QObject::connect( g_videoPlayer, &QMediaPlayer::mediaStatusChanged, []( QMediaPlayer::MediaStatus status ){
		if ( g_videoPlayer == nullptr || g_videoLoopCheck == nullptr ) {
			return;
		}
		if ( status == QMediaPlayer::EndOfMedia && g_videoLoopCheck->isChecked() ) {
			g_videoPlayer->setPosition( 0 );
			g_videoPlayer->play();
		}
	} );

	const auto lastFile = VideoWorkbench_setting( "LastFile" );
	if ( !lastFile.isEmpty() && QFileInfo::exists( lastFile ) ) {
		VideoWorkbench_openFile( lastFile, false );
	}
	VideoWorkbench_updateTimeLabel();
}

void VideoWorkbench_stopAndRelease(){
	if ( g_videoPlayer != nullptr ) {
		g_videoPlayer->stop();
	}
	g_videoDock = nullptr;
	g_videoPlayer = nullptr;
	g_videoWidget = nullptr;
	g_videoSeekSlider = nullptr;
	g_videoVolumeSlider = nullptr;
	g_videoPathLabel = nullptr;
	g_videoTimeLabel = nullptr;
	g_videoLoopCheck = nullptr;
}
#else
void VideoWorkbench_createDock( QMainWindow* ){
}

void VideoWorkbench_open(){
	QMessageBox::warning(
	    MainFrame_getWindow(),
	    "Cinematic Player",
	    "This build does not include Qt Multimedia video widget support.\nInstall Qt5MultimediaWidgets development files and rebuild."
	);
}

void VideoWorkbench_stopAndRelease(){
}
#endif
