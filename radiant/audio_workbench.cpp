#include "audio_workbench.h"

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
#include <QLineEdit>
#include <QListWidget>
#include <QSlider>
#include <QCheckBox>
#include <QComboBox>
#include <QVariant>
#include <QFileDialog>
#include <QFile>
#include <QFileInfo>
#include <QTextStream>
#include <QDir>
#include <QDirIterator>
#include <QMessageBox>
#include <QSettings>
#include <QMenu>
#include <QGuiApplication>

#include <algorithm>
#include <limits>
#include <set>
#include <QHash>

#if __has_include( <QMediaContent> ) && __has_include( <QMediaPlayer> ) && __has_include( <QMediaPlaylist> )
	#include <QMediaContent>
	#include <QMediaPlayer>
	#include <QMediaPlaylist>
	#define RADIANT_QT_MULTIMEDIA_AVAILABLE 1
#else
	#define RADIANT_QT_MULTIMEDIA_AVAILABLE 0
#endif

namespace
{

#if RADIANT_QT_MULTIMEDIA_AVAILABLE
QDockWidget* g_audioDock{};
QMediaPlayer* g_audioPlayer{};
QMediaPlaylist* g_audioPlaylist{};
QListWidget* g_audioPlaylistView{};
QSlider* g_audioSeekSlider{};
QSlider* g_audioVolumeSlider{};
QLabel* g_audioNowPlayingLabel{};
QLabel* g_audioTimeLabel{};
QLineEdit* g_audioSearchEdit{};
QLabel* g_audioPlaylistStatsLabel{};
	QCheckBox* g_audioShuffleCheck{};
	QCheckBox* g_audioLoopCheck{};
	QComboBox* g_audioCategoryFilter{};
	QPushButton* g_audioScanContentButton{};

const char* const c_audioSettingsPrefix = "AudioWorkbench/";

QString AudioWorkbench_audioFilter(){
	return "Audio Files (*.mp3 *.ogg *.wav *.flac *.opus *.aac *.m4a);;All Files (*)";
}

QString AudioWorkbench_formatDuration( qint64 milliseconds ){
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

QSettings& AudioWorkbench_settings(){
	static QSettings settings;
	return settings;
}

QString AudioWorkbench_setting( const char* key, const QString& fallback = {} ){
	return AudioWorkbench_settings().value( StringStream( c_audioSettingsPrefix, key ).c_str(), fallback ).toString();
}

bool AudioWorkbench_settingBool( const char* key, bool fallback ){
	return AudioWorkbench_settings().value( StringStream( c_audioSettingsPrefix, key ).c_str(), fallback ).toBool();
}

int AudioWorkbench_settingInt( const char* key, int fallback ){
	return AudioWorkbench_settings().value( StringStream( c_audioSettingsPrefix, key ).c_str(), fallback ).toInt();
}

void AudioWorkbench_setSetting( const char* key, const QVariant& value ){
	AudioWorkbench_settings().setValue( StringStream( c_audioSettingsPrefix, key ).c_str(), value );
}

QString AudioWorkbench_autosavePath(){
	return QDir( QString::fromLatin1( SettingsPath_get() ) ).filePath( "audio_workbench_autosave.m3u" );
}

void AudioWorkbench_setLastDirectory( const QString& path ){
	if ( path.isEmpty() ) {
		return;
	}
	const QFileInfo info( path );
	const QString directory = info.isDir() ? info.absoluteFilePath() : info.absolutePath();
	if ( !directory.isEmpty() ) {
		AudioWorkbench_setSetting( "LastDirectory", directory );
	}
}

enum class AudioCategory
{
	Unknown = 0,
	SoundEffect,
	Music,
	Playlist
};

QHash<QString, AudioCategory> g_audioCategories;

AudioCategory AudioWorkbench_detectCategory( const QString& path ){
	const QString normalized = QFileInfo( path ).fileName().toLower();
	const QString lowerPath = path.toLower();
	static const QStringList playlistExtensions = { ".m3u", ".m3u8", ".pls" };
	for ( const auto& ext : playlistExtensions )
	{
		if ( lowerPath.endsWith( ext ) ) {
			return AudioCategory::Playlist;
		}
	}

	if ( lowerPath.contains( "/music/" ) || lowerPath.contains( "\\music\\" ) || normalized.startsWith( "music_" ) ) {
		return AudioCategory::Music;
	}
	if ( lowerPath.contains( "/sfx/" ) || lowerPath.contains( "\\sfx\\" ) || normalized.startsWith( "sfx_" ) ) {
		return AudioCategory::SoundEffect;
	}

	return AudioCategory::SoundEffect;
}

QString AudioWorkbench_categoryLabel( AudioCategory category ){
	switch ( category )
	{
	case AudioCategory::Music:
		return "Music";
	case AudioCategory::Playlist:
		return "Playlist";
	case AudioCategory::SoundEffect:
		return "SFX";
	default:
		return "Unknown";
	}
}

QString AudioWorkbench_contentFolder(){
	const QString override = AudioWorkbench_setting( "ContentFolder" );
	if ( !override.isEmpty() ) {
		return override;
	}
	return QString::fromLatin1( EnginePath_get() ) + "/content";
}

void AudioWorkbench_setContentFolder( const QString& path ){
	if ( path.isEmpty() ) {
		return;
	}
	AudioWorkbench_setSetting( "ContentFolder", path );
}

QString AudioWorkbench_categoryFilterLabel( AudioCategory category ){
	if ( category == AudioCategory::Unknown ) {
		return "All";
	}
	return AudioWorkbench_categoryLabel( category );
}

AudioCategory AudioWorkbench_filterCategorySetting(){
	return static_cast<AudioCategory>( AudioWorkbench_settingInt( "CategoryFilter", static_cast<int>( AudioCategory::Unknown ) ) );
}

void AudioWorkbench_setMediaCategory( const QString& path, AudioCategory category ){
	const QString absolute = QFileInfo( path ).absoluteFilePath();
	g_audioCategories.insert( absolute, category );
}

AudioCategory AudioWorkbench_categoryForPath( const QString& path ){
	const QString absolute = QFileInfo( path ).absoluteFilePath();
	if ( const auto it = g_audioCategories.find( absolute ); it != g_audioCategories.end() ) {
		return it.value();
	}
	const AudioCategory detected = AudioWorkbench_detectCategory( absolute );
	g_audioCategories.insert( absolute, detected );
	return detected;
}

QStringList AudioWorkbench_collectAudioFiles( const QString& directory, bool recursive ){
	static const QStringList filters = { "*.mp3", "*.ogg", "*.wav", "*.flac", "*.opus", "*.aac", "*.m4a", "*.m3u", "*.m3u8", "*.pls" };
	QStringList files;
	if ( recursive ) {
		QDirIterator iterator( directory, filters, QDir::Files, QDirIterator::Subdirectories );
		while ( iterator.hasNext() ) {
			files.push_back( iterator.next() );
		}
	}
	else{
		QDir dir( directory );
		for ( const auto& file : dir.entryList( filters, QDir::Files, QDir::Name ) ) {
			files.push_back( dir.absoluteFilePath( file ) );
		}
	}
	return files;
}

void AudioWorkbench_addFiles( const QStringList& files );
void AudioWorkbench_dedupe();
void AudioWorkbench_updateStatsLabel();

void AudioWorkbench_applyCategoryFilter();

void AudioWorkbench_scanContentFolder( bool recursive, bool showWarnings = true ){
	const QString folder = AudioWorkbench_contentFolder();
	if ( folder.isEmpty() ) {
		return;
	}
	QDir dir( folder );
	if ( !dir.exists() ) {
		if ( showWarnings ) {
			QMessageBox::warning( MainFrame_getWindow(), "Audio Scan", QString( "Content folder not found: %1" ).arg( folder ) );
		}
		return;
	}
	AudioWorkbench_setContentFolder( folder );
	const QStringList files = AudioWorkbench_collectAudioFiles( folder, recursive );
	if ( files.isEmpty() ) {
		if ( showWarnings ) {
			QMessageBox::information( MainFrame_getWindow(), "Audio Scan", "No audio files detected in the content folder." );
		}
		return;
	}
	AudioWorkbench_addFiles( files );
	AudioWorkbench_dedupe();
	AudioWorkbench_applyCategoryFilter();
}

void AudioWorkbench_applyCategoryFilter(){
	if ( g_audioCategoryFilter == nullptr || g_audioPlaylistView == nullptr ) {
		return;
	}
	const AudioCategory filterCategory = static_cast<AudioCategory>( g_audioCategoryFilter->currentData().toInt() );
	for ( int i = 0; i < g_audioPlaylistView->count(); ++i )
	{
		auto* item = g_audioPlaylistView->item( i );
		if ( item == nullptr ) {
			continue;
		}
		const AudioCategory category = static_cast<AudioCategory>( item->data( Qt::UserRole + 2 ).toInt() );
		const bool visible = filterCategory == AudioCategory::Unknown || category == filterCategory;
		item->setHidden( !visible );
	}
	AudioWorkbench_updateStatsLabel();
}


QString AudioWorkbench_defaultDirectory(){
	const QString last = AudioWorkbench_setting( "LastDirectory" );
	if ( !last.isEmpty() ) {
		return last;
	}
	return QString::fromLatin1( EnginePath_get() );
}

void AudioWorkbench_updateNowPlayingLabel(){
	if ( g_audioNowPlayingLabel == nullptr || g_audioPlaylist == nullptr ) {
		return;
	}
	const int index = g_audioPlaylist->currentIndex();
	if ( index < 0 || index >= g_audioPlaylist->mediaCount() ) {
		g_audioNowPlayingLabel->setText( "Now playing: (none)" );
		return;
	}

	const auto media = g_audioPlaylist->media( index );
	const auto filePath = media.canonicalUrl().isLocalFile() ? media.canonicalUrl().toLocalFile() : media.canonicalUrl().toString();
	const auto title = QFileInfo( filePath ).fileName();
	g_audioNowPlayingLabel->setText( StringStream( "Now playing: ", title.toUtf8().constData() ).c_str() );
}

void AudioWorkbench_updateStatsLabel(){
	if ( g_audioPlaylistStatsLabel == nullptr || g_audioPlaylist == nullptr ) {
		return;
	}
	int visible = 0;
	if ( g_audioPlaylistView != nullptr ) {
		for ( int i = 0; i < g_audioPlaylistView->count(); ++i )
		{
			if ( !g_audioPlaylistView->item( i )->isHidden() ) {
				++visible;
			}
		}
	}
	g_audioPlaylistStatsLabel->setText( StringStream( "Tracks: ", g_audioPlaylist->mediaCount(), " (visible: ", visible, ")" ).c_str() );
}

void AudioWorkbench_applyFilter(){
	if ( g_audioPlaylistView == nullptr || g_audioSearchEdit == nullptr ) {
		return;
	}
	const auto needle = g_audioSearchEdit->text().trimmed();
	for ( int i = 0; i < g_audioPlaylistView->count(); ++i )
	{
		auto* item = g_audioPlaylistView->item( i );
		const QString haystack = StringStream( item->text().toUtf8().constData(), " ", item->toolTip().toUtf8().constData() ).c_str();
		item->setHidden( !needle.isEmpty() && !haystack.contains( needle, Qt::CaseInsensitive ) );
	}
	AudioWorkbench_updateStatsLabel();
}

void AudioWorkbench_saveAutosavePlaylist(){
	if ( g_audioPlaylist == nullptr ) {
		return;
	}

	QFile file( AudioWorkbench_autosavePath() );
	if ( !file.open( QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate ) ) {
		return;
	}

	QTextStream out( &file );
	out << "#EXTM3U\n";
	for ( int i = 0; i < g_audioPlaylist->mediaCount(); ++i )
	{
		const auto url = g_audioPlaylist->media( i ).canonicalUrl();
		out << ( url.isLocalFile() ? url.toLocalFile() : url.toString() ) << '\n';
	}
	AudioWorkbench_setSetting( "CurrentIndex", g_audioPlaylist->currentIndex() );
}

void AudioWorkbench_applyPlaybackMode(){
	if ( g_audioPlaylist == nullptr || g_audioShuffleCheck == nullptr || g_audioLoopCheck == nullptr ) {
		return;
	}
	if ( g_audioShuffleCheck->isChecked() ) {
		g_audioPlaylist->setPlaybackMode( QMediaPlaylist::Random );
	}
	else{
		g_audioPlaylist->setPlaybackMode( g_audioLoopCheck->isChecked() ? QMediaPlaylist::Loop : QMediaPlaylist::Sequential );
	}
}

void AudioWorkbench_syncPlaylistView(){
	if ( g_audioPlaylistView == nullptr || g_audioPlaylist == nullptr ) {
		return;
	}

	g_audioPlaylistView->blockSignals( true );
	g_audioPlaylistView->clear();
	for ( int i = 0; i < g_audioPlaylist->mediaCount(); ++i )
	{
		const auto media = g_audioPlaylist->media( i );
		const auto url = media.canonicalUrl();
		const auto path = url.isLocalFile() ? url.toLocalFile() : url.toString();
		const QString absolute = QFileInfo( path ).absoluteFilePath();
		const AudioCategory category = AudioWorkbench_categoryForPath( absolute );
		const QString title = QFileInfo( path ).fileName();
		const QString displayText = QString( "%1 [%2]" ).arg( title, AudioWorkbench_categoryLabel( category ) );
		auto* item = new QListWidgetItem( displayText, g_audioPlaylistView );
		const QString tooltip = QString( "%1 (%2)" ).arg( path, AudioWorkbench_categoryLabel( category ) );
		item->setToolTip( tooltip );
		item->setData( Qt::UserRole, absolute );
		item->setData( Qt::UserRole + 1, i );
		item->setData( Qt::UserRole + 2, static_cast<int>( category ) );
	}

	const int currentPlaylistIndex = g_audioPlaylist->currentIndex();
	for ( int i = 0; i < g_audioPlaylistView->count(); ++i )
	{
		if ( g_audioPlaylistView->item( i )->data( Qt::UserRole + 1 ).toInt() == currentPlaylistIndex ) {
			g_audioPlaylistView->setCurrentRow( i );
			break;
		}
	}
	g_audioPlaylistView->blockSignals( false );

	AudioWorkbench_updateNowPlayingLabel();
	AudioWorkbench_applyFilter();
	AudioWorkbench_applyCategoryFilter();
	AudioWorkbench_saveAutosavePlaylist();
}

void AudioWorkbench_updateTimeLabel(){
	if ( g_audioPlayer == nullptr || g_audioTimeLabel == nullptr ) {
		return;
	}
	g_audioTimeLabel->setText(
	    StringStream(
	        AudioWorkbench_formatDuration( g_audioPlayer->position() ).toUtf8().constData(),
	        " / ",
	        AudioWorkbench_formatDuration( g_audioPlayer->duration() ).toUtf8().constData()
	    ).c_str()
	);
}

void AudioWorkbench_addFiles( const QStringList& files ){
	if ( g_audioPlaylist == nullptr ) {
		return;
	}

	for ( const auto& file : files )
	{
		if ( file.isEmpty() ) {
			continue;
		}
		const QString absolute = QFileInfo( file ).absoluteFilePath();
		const AudioCategory category = AudioWorkbench_detectCategory( absolute );
		AudioWorkbench_setMediaCategory( absolute, category );
		g_audioPlaylist->addMedia( QUrl::fromLocalFile( absolute ) );
		AudioWorkbench_setLastDirectory( absolute );
	}

	if ( g_audioPlaylist->currentIndex() < 0 && g_audioPlaylist->mediaCount() > 0 ) {
		g_audioPlaylist->setCurrentIndex( 0 );
	}

	AudioWorkbench_syncPlaylistView();
}

QStringList AudioWorkbench_collectPlaylistPaths(){
	QStringList paths;
	if ( g_audioPlaylist == nullptr ) {
		return paths;
	}
	for ( int i = 0; i < g_audioPlaylist->mediaCount(); ++i )
	{
		const auto url = g_audioPlaylist->media( i ).canonicalUrl();
		if ( url.isLocalFile() ) {
			paths.push_back( QFileInfo( url.toLocalFile() ).absoluteFilePath() );
		}
	}
	return paths;
}

void AudioWorkbench_rebuildPlaylist( const QStringList& paths, const QString& selectedPath ){
	if ( g_audioPlaylist == nullptr ) {
		return;
	}
	g_audioPlaylist->clear();
	int selectedIndex = -1;
	for ( int i = 0; i < paths.size(); ++i )
	{
		const auto absolute = QFileInfo( paths[i] ).absoluteFilePath();
		const AudioCategory category = AudioWorkbench_detectCategory( absolute );
		AudioWorkbench_setMediaCategory( absolute, category );
		g_audioPlaylist->addMedia( QUrl::fromLocalFile( absolute ) );
		if ( !selectedPath.isEmpty() && absolute == selectedPath ) {
			selectedIndex = i;
		}
	}
	if ( selectedIndex >= 0 ) {
		g_audioPlaylist->setCurrentIndex( selectedIndex );
	}
	else if ( g_audioPlaylist->mediaCount() > 0 ) {
		g_audioPlaylist->setCurrentIndex( 0 );
	}
	AudioWorkbench_syncPlaylistView();
}

void AudioWorkbench_addFolder( const QString& directory, bool recursive ){
	if ( directory.isEmpty() ) {
		return;
	}
	AudioWorkbench_setLastDirectory( directory );
	QStringList files;
	if ( recursive ) {
		QDirIterator it( directory, { "*.mp3", "*.ogg", "*.wav", "*.flac", "*.opus", "*.aac", "*.m4a" }, QDir::Files, QDirIterator::Subdirectories );
		while ( it.hasNext() )
		{
			files.push_back( it.next() );
		}
	}
	else{
		QDir dir( directory );
		for ( const auto& file : dir.entryList( { "*.mp3", "*.ogg", "*.wav", "*.flac", "*.opus", "*.aac", "*.m4a" }, QDir::Files, QDir::Name ) )
		{
			files.push_back( dir.absoluteFilePath( file ) );
		}
	}
	AudioWorkbench_addFiles( files );
}

bool AudioWorkbench_loadPlaylistFromFile( const QString& path, bool showErrors ){
	if ( g_audioPlaylist == nullptr ) {
		return false;
	}
	QFile file( path );
	if ( !file.open( QIODevice::ReadOnly | QIODevice::Text ) ) {
		if ( showErrors ) {
			QMessageBox::warning( MainFrame_getWindow(), "Load playlist", "Failed to open playlist file." );
		}
		return false;
	}

	g_audioPlaylist->clear();
	QTextStream stream( &file );
	QDir baseDir = QFileInfo( path ).absoluteDir();
	while ( !stream.atEnd() )
	{
		const auto raw = stream.readLine().trimmed();
		if ( raw.isEmpty() || raw.startsWith( '#' ) ) {
			continue;
		}
		const auto resolved = QFileInfo( raw ).isAbsolute() ? raw : baseDir.absoluteFilePath( raw );
		const QString absolute = QFileInfo( resolved ).absoluteFilePath();
		const AudioCategory category = AudioWorkbench_detectCategory( absolute );
		AudioWorkbench_setMediaCategory( absolute, category );
		g_audioPlaylist->addMedia( QUrl::fromLocalFile( absolute ) );
	}
	if ( g_audioPlaylist->mediaCount() > 0 ) {
		const int requestedIndex = AudioWorkbench_settingInt( "CurrentIndex", 0 );
		g_audioPlaylist->setCurrentIndex( qBound( 0, requestedIndex, g_audioPlaylist->mediaCount() - 1 ) );
	}

	AudioWorkbench_setSetting( "LastPlaylist", path );
	AudioWorkbench_setLastDirectory( path );
	AudioWorkbench_syncPlaylistView();
	return true;
}

int AudioWorkbench_currentPlaylistIndex(){
	if ( g_audioPlaylistView == nullptr || g_audioPlaylistView->currentItem() == nullptr ) {
		return -1;
	}
	return g_audioPlaylistView->currentItem()->data( Qt::UserRole + 1 ).toInt();
}

void AudioWorkbench_removeSelected(){
	if ( g_audioPlaylist == nullptr || g_audioPlaylistView == nullptr ) {
		return;
	}

	QList<int> playlistIndexes;
	for ( const auto& modelIndex : g_audioPlaylistView->selectionModel()->selectedRows() )
	{
		if ( auto* item = g_audioPlaylistView->item( modelIndex.row() ) ) {
			playlistIndexes.push_back( item->data( Qt::UserRole + 1 ).toInt() );
		}
	}
	std::sort( playlistIndexes.begin(), playlistIndexes.end(), std::greater<int>() );
	for ( int playlistIndex : playlistIndexes )
	{
		if ( playlistIndex >= 0 && playlistIndex < g_audioPlaylist->mediaCount() ) {
			g_audioPlaylist->removeMedia( playlistIndex );
		}
	}

	if ( g_audioPlaylist->mediaCount() == 0 ) {
		g_audioPlayer->stop();
	}
	else if ( g_audioPlaylist->currentIndex() < 0 ) {
		g_audioPlaylist->setCurrentIndex( 0 );
	}
	AudioWorkbench_syncPlaylistView();
}

void AudioWorkbench_moveCurrent( int delta ){
	if ( g_audioPlaylist == nullptr || g_audioPlaylistView == nullptr ) {
		return;
	}
	const int playlistIndex = AudioWorkbench_currentPlaylistIndex();
	if ( playlistIndex < 0 ) {
		return;
	}
	const int target = playlistIndex + delta;
	if ( target < 0 || target >= g_audioPlaylist->mediaCount() ) {
		return;
	}
	const auto media = g_audioPlaylist->media( playlistIndex );
	g_audioPlaylist->removeMedia( playlistIndex );
	g_audioPlaylist->insertMedia( target, media );
	g_audioPlaylist->setCurrentIndex( target );
	AudioWorkbench_syncPlaylistView();
}

void AudioWorkbench_dedupe(){
	const auto selected = g_audioPlaylistView && g_audioPlaylistView->currentItem() ? g_audioPlaylistView->currentItem()->data( Qt::UserRole ).toString() : "";
	QStringList paths = AudioWorkbench_collectPlaylistPaths();
	std::set<QString> seen;
	QStringList deduped;
	for ( const auto& path : paths )
	{
		if ( seen.insert( path ).second ) {
			deduped.push_back( path );
		}
	}
	AudioWorkbench_rebuildPlaylist( deduped, selected );
}

void AudioWorkbench_removeMissing(){
	const auto selected = g_audioPlaylistView && g_audioPlaylistView->currentItem() ? g_audioPlaylistView->currentItem()->data( Qt::UserRole ).toString() : "";
	QStringList existing;
	for ( const auto& path : AudioWorkbench_collectPlaylistPaths() )
	{
		if ( QFileInfo::exists( path ) ) {
			existing.push_back( path );
		}
	}
	AudioWorkbench_rebuildPlaylist( existing, selected );
}

void AudioWorkbench_sortByFilename(){
	const auto selected = g_audioPlaylistView && g_audioPlaylistView->currentItem() ? g_audioPlaylistView->currentItem()->data( Qt::UserRole ).toString() : "";
	auto paths = AudioWorkbench_collectPlaylistPaths();
	std::sort( paths.begin(), paths.end(), []( const QString& a, const QString& b ){
		return QFileInfo( a ).fileName().compare( QFileInfo( b ).fileName(), Qt::CaseInsensitive ) < 0;
	} );
	AudioWorkbench_rebuildPlaylist( paths, selected );
}

void AudioWorkbench_playSelected(){
	if ( g_audioPlayer == nullptr || g_audioPlaylist == nullptr ) {
		return;
	}
	const int playlistIndex = AudioWorkbench_currentPlaylistIndex();
	if ( playlistIndex >= 0 ) {
		g_audioPlaylist->setCurrentIndex( playlistIndex );
	}
	if ( g_audioPlaylist->currentIndex() < 0 && g_audioPlaylist->mediaCount() > 0 ) {
		g_audioPlaylist->setCurrentIndex( 0 );
	}
	if ( g_audioPlaylist->currentIndex() >= 0 ) {
		g_audioPlayer->play();
	}
}

void AudioWorkbench_showPlaylistContextMenu( const QPoint& pos ){
	if ( g_audioPlaylistView == nullptr ) {
		return;
	}
	QMenu menu( g_audioPlaylistView );
	auto* playAction = menu.addAction( "Play" );
	auto* removeAction = menu.addAction( "Remove" );
	menu.addSeparator();
	auto* upAction = menu.addAction( "Move Up" );
	auto* downAction = menu.addAction( "Move Down" );
	menu.addSeparator();
	auto* dedupeAction = menu.addAction( "Dedupe" );
	auto* removeMissingAction = menu.addAction( "Remove Missing" );
	auto* sortAction = menu.addAction( "Sort" );

	playAction->setEnabled( g_audioPlaylistView->currentItem() != nullptr );
	removeAction->setEnabled( !g_audioPlaylistView->selectionModel()->selectedRows().isEmpty() );
	upAction->setEnabled( AudioWorkbench_currentPlaylistIndex() > 0 );
	downAction->setEnabled( AudioWorkbench_currentPlaylistIndex() >= 0
	                        && g_audioPlaylist != nullptr
	                        && AudioWorkbench_currentPlaylistIndex() + 1 < g_audioPlaylist->mediaCount() );

	if ( QAction* chosen = menu.exec( g_audioPlaylistView->viewport()->mapToGlobal( pos ) ) ) {
		if ( chosen == playAction ) {
			AudioWorkbench_playSelected();
		}
		else if ( chosen == removeAction ) {
			AudioWorkbench_removeSelected();
		}
		else if ( chosen == upAction ) {
			AudioWorkbench_moveCurrent( -1 );
		}
		else if ( chosen == downAction ) {
			AudioWorkbench_moveCurrent( +1 );
		}
		else if ( chosen == dedupeAction ) {
			AudioWorkbench_dedupe();
		}
		else if ( chosen == removeMissingAction ) {
			AudioWorkbench_removeMissing();
		}
		else if ( chosen == sortAction ) {
			AudioWorkbench_sortByFilename();
		}
	}
}

void AudioWorkbench_restoreAutosavedPlaylist(){
	const auto autosave = AudioWorkbench_autosavePath();
	if ( QFileInfo::exists( autosave ) ) {
		AudioWorkbench_loadPlaylistFromFile( autosave, false );
		return;
	}
	const auto lastPlaylist = AudioWorkbench_setting( "LastPlaylist" );
	if ( !lastPlaylist.isEmpty() && QFileInfo::exists( lastPlaylist ) ) {
		AudioWorkbench_loadPlaylistFromFile( lastPlaylist, false );
	}
}

#endif

}

#if RADIANT_QT_MULTIMEDIA_AVAILABLE
void AudioWorkbench_createDock( QMainWindow* window ){
	if ( window == nullptr || g_audioDock != nullptr ) {
		return;
	}

	g_audioDock = new QDockWidget( "Music / Playlists", window );
	g_audioDock->setObjectName( "dock_audio_workbench" );

	auto* root = new QWidget( g_audioDock );
	auto* layout = new QVBoxLayout( root );

	auto* topButtons = new QHBoxLayout();
	auto* addFilesButton = new QPushButton( "Add Files...", root );
	auto* addFolderButton = new QPushButton( "Add Folder...", root );
	auto* addFolderRecursiveButton = new QPushButton( "Add Folder Rec...", root );
	auto* loadPlaylistButton = new QPushButton( "Load Playlist...", root );
	auto* savePlaylistButton = new QPushButton( "Save Playlist...", root );
	auto* saveRelativeButton = new QPushButton( "Save Relative...", root );
	auto* clearPlaylistButton = new QPushButton( "Clear", root );
	topButtons->addWidget( addFilesButton );
	topButtons->addWidget( addFolderButton );
	topButtons->addWidget( addFolderRecursiveButton );
	topButtons->addWidget( loadPlaylistButton );
	topButtons->addWidget( savePlaylistButton );
	topButtons->addWidget( saveRelativeButton );
	topButtons->addWidget( clearPlaylistButton );
	auto* scanContentButton = new QPushButton( "Scan Content", root );
	topButtons->addWidget( scanContentButton );
	g_audioScanContentButton = scanContentButton;
	layout->addLayout( topButtons );

	auto* filterRow = new QHBoxLayout();
	filterRow->addWidget( new QLabel( "Filter", root ) );
	g_audioSearchEdit = new QLineEdit( root );
	g_audioSearchEdit->setPlaceholderText( "Type to filter playlist..." );
	filterRow->addWidget( g_audioSearchEdit, 1 );
	filterRow->addWidget( new QLabel( "Category", root ) );
	g_audioCategoryFilter = new QComboBox( root );
	g_audioCategoryFilter->addItem( AudioWorkbench_categoryFilterLabel( AudioCategory::Unknown ), static_cast<int>( AudioCategory::Unknown ) );
	g_audioCategoryFilter->addItem( AudioWorkbench_categoryFilterLabel( AudioCategory::Music ), static_cast<int>( AudioCategory::Music ) );
	g_audioCategoryFilter->addItem( AudioWorkbench_categoryFilterLabel( AudioCategory::SoundEffect ), static_cast<int>( AudioCategory::SoundEffect ) );
	g_audioCategoryFilter->addItem( AudioWorkbench_categoryFilterLabel( AudioCategory::Playlist ), static_cast<int>( AudioCategory::Playlist ) );
	const AudioCategory savedFilter = AudioWorkbench_filterCategorySetting();
	const int savedIndex = g_audioCategoryFilter->findData( static_cast<int>( savedFilter ) );
	if ( savedIndex >= 0 ) {
		g_audioCategoryFilter->setCurrentIndex( savedIndex );
	}
	filterRow->addWidget( g_audioCategoryFilter );
	layout->addLayout( filterRow );

	g_audioPlaylistView = new QListWidget( root );
	g_audioPlaylistView->setSelectionMode( QAbstractItemView::ExtendedSelection );
	g_audioPlaylistView->setAlternatingRowColors( true );
	g_audioPlaylistView->setContextMenuPolicy( Qt::CustomContextMenu );
	layout->addWidget( g_audioPlaylistView, 1 );

	auto* editButtons = new QHBoxLayout();
	auto* removeButton = new QPushButton( "Remove", root );
	auto* moveUpButton = new QPushButton( "Move Up", root );
	auto* moveDownButton = new QPushButton( "Move Down", root );
	auto* dedupeButton = new QPushButton( "Dedupe", root );
	auto* removeMissingButton = new QPushButton( "Remove Missing", root );
	auto* sortButton = new QPushButton( "Sort", root );
	editButtons->addWidget( removeButton );
	editButtons->addWidget( moveUpButton );
	editButtons->addWidget( moveDownButton );
	editButtons->addWidget( dedupeButton );
	editButtons->addWidget( removeMissingButton );
	editButtons->addWidget( sortButton );
	layout->addLayout( editButtons );

	auto* transport = new QHBoxLayout();
	auto* playButton = new QPushButton( "Play", root );
	auto* pauseButton = new QPushButton( "Pause", root );
	auto* stopButton = new QPushButton( "Stop", root );
	auto* previousButton = new QPushButton( "Prev", root );
	auto* nextButton = new QPushButton( "Next", root );
	transport->addWidget( previousButton );
	transport->addWidget( playButton );
	transport->addWidget( pauseButton );
	transport->addWidget( stopButton );
	transport->addWidget( nextButton );
	layout->addLayout( transport );

	g_audioSeekSlider = new QSlider( Qt::Horizontal, root );
	g_audioSeekSlider->setRange( 0, 0 );
	layout->addWidget( g_audioSeekSlider );

	auto* nowPlayingRow = new QHBoxLayout();
	g_audioNowPlayingLabel = new QLabel( "Now playing: (none)", root );
	g_audioNowPlayingLabel->setTextInteractionFlags( Qt::TextSelectableByMouse );
	g_audioTimeLabel = new QLabel( "0:00 / 0:00", root );
	nowPlayingRow->addWidget( g_audioNowPlayingLabel, 1 );
	nowPlayingRow->addWidget( g_audioTimeLabel );
	layout->addLayout( nowPlayingRow );

	auto* volumeRow = new QHBoxLayout();
	volumeRow->addWidget( new QLabel( "Volume", root ) );
	g_audioVolumeSlider = new QSlider( Qt::Horizontal, root );
	g_audioVolumeSlider->setRange( 0, 100 );
	g_audioVolumeSlider->setValue( qBound( 0, AudioWorkbench_settingInt( "Volume", 80 ), 100 ) );
	g_audioShuffleCheck = new QCheckBox( "Shuffle", root );
	g_audioLoopCheck = new QCheckBox( "Loop", root );
	g_audioShuffleCheck->setChecked( AudioWorkbench_settingBool( "Shuffle", false ) );
	g_audioLoopCheck->setChecked( AudioWorkbench_settingBool( "Loop", false ) );
	volumeRow->addWidget( g_audioVolumeSlider, 1 );
	volumeRow->addWidget( g_audioShuffleCheck );
	volumeRow->addWidget( g_audioLoopCheck );
	layout->addLayout( volumeRow );

	g_audioPlaylistStatsLabel = new QLabel( "Tracks: 0", root );
	layout->addWidget( g_audioPlaylistStatsLabel );

	g_audioDock->setWidget( root );
	window->addDockWidget( Qt::BottomDockWidgetArea, g_audioDock );
	g_audioDock->hide();

	g_audioPlaylist = new QMediaPlaylist( g_audioDock );
	g_audioPlayer = new QMediaPlayer( g_audioDock );
	g_audioPlayer->setPlaylist( g_audioPlaylist );
	g_audioPlayer->setVolume( g_audioVolumeSlider->value() );
	AudioWorkbench_applyPlaybackMode();

	QObject::connect( addFilesButton, &QPushButton::clicked, [](){
		const auto files = QFileDialog::getOpenFileNames( MainFrame_getWindow(), "Add Audio Files", AudioWorkbench_defaultDirectory(), AudioWorkbench_audioFilter() );
		AudioWorkbench_addFiles( files );
	} );
	QObject::connect( addFolderButton, &QPushButton::clicked, [](){
		const auto directory = QFileDialog::getExistingDirectory( MainFrame_getWindow(), "Add Audio Folder", AudioWorkbench_defaultDirectory() );
		AudioWorkbench_addFolder( directory, false );
	} );
	QObject::connect( addFolderRecursiveButton, &QPushButton::clicked, [](){
		const auto directory = QFileDialog::getExistingDirectory( MainFrame_getWindow(), "Add Audio Folder Recursively", AudioWorkbench_defaultDirectory() );
		AudioWorkbench_addFolder( directory, true );
	} );
	QObject::connect( loadPlaylistButton, &QPushButton::clicked, [](){
		const auto path = QFileDialog::getOpenFileName( MainFrame_getWindow(), "Load Playlist", AudioWorkbench_defaultDirectory(), "Playlists (*.m3u *.m3u8 *.txt);;All Files (*)" );
		if ( path.isEmpty() ) {
			return;
		}
		AudioWorkbench_loadPlaylistFromFile( path, true );
	} );
	QObject::connect( savePlaylistButton, &QPushButton::clicked, [](){
		if ( g_audioPlaylist == nullptr ) {
			return;
		}
		const auto path = QFileDialog::getSaveFileName( MainFrame_getWindow(), "Save Playlist", AudioWorkbench_defaultDirectory(), "M3U Playlist (*.m3u)" );
		if ( path.isEmpty() ) {
			return;
		}

		QFile file( path );
		if ( !file.open( QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate ) ) {
			QMessageBox::warning( MainFrame_getWindow(), "Save playlist", "Failed to write playlist file." );
			return;
		}

		QTextStream out( &file );
		out << "#EXTM3U\n";
		for ( int i = 0; i < g_audioPlaylist->mediaCount(); ++i )
		{
			const auto url = g_audioPlaylist->media( i ).canonicalUrl();
			out << ( url.isLocalFile() ? url.toLocalFile() : url.toString() ) << '\n';
		}
		AudioWorkbench_setSetting( "LastPlaylist", path );
		AudioWorkbench_setLastDirectory( path );
	} );
	QObject::connect( saveRelativeButton, &QPushButton::clicked, [](){
		if ( g_audioPlaylist == nullptr ) {
			return;
		}
		const auto path = QFileDialog::getSaveFileName( MainFrame_getWindow(), "Save Relative Playlist", AudioWorkbench_defaultDirectory(), "M3U Playlist (*.m3u)" );
		if ( path.isEmpty() ) {
			return;
		}
		QFile file( path );
		if ( !file.open( QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate ) ) {
			QMessageBox::warning( MainFrame_getWindow(), "Save playlist", "Failed to write playlist file." );
			return;
		}
		QDir base = QFileInfo( path ).absoluteDir();
		QTextStream out( &file );
		out << "#EXTM3U\n";
		for ( int i = 0; i < g_audioPlaylist->mediaCount(); ++i )
		{
			const auto url = g_audioPlaylist->media( i ).canonicalUrl();
			if ( url.isLocalFile() ) {
				out << base.relativeFilePath( url.toLocalFile() ) << '\n';
			}
			else{
				out << url.toString() << '\n';
			}
		}
		AudioWorkbench_setSetting( "LastPlaylist", path );
		AudioWorkbench_setLastDirectory( path );
	} );
	QObject::connect( clearPlaylistButton, &QPushButton::clicked, [](){
		if ( g_audioPlaylist != nullptr ) {
			g_audioPlayer->stop();
			g_audioPlaylist->clear();
			AudioWorkbench_syncPlaylistView();
			AudioWorkbench_updateTimeLabel();
		}
	} );
	QObject::connect( removeButton, &QPushButton::clicked, [](){ AudioWorkbench_removeSelected(); } );
	QObject::connect( dedupeButton, &QPushButton::clicked, [](){ AudioWorkbench_dedupe(); } );
	QObject::connect( removeMissingButton, &QPushButton::clicked, [](){ AudioWorkbench_removeMissing(); } );
	QObject::connect( sortButton, &QPushButton::clicked, [](){ AudioWorkbench_sortByFilename(); } );
	QObject::connect( moveUpButton, &QPushButton::clicked, [](){ AudioWorkbench_moveCurrent( -1 ); } );
	QObject::connect( moveDownButton, &QPushButton::clicked, [](){ AudioWorkbench_moveCurrent( +1 ); } );

	QObject::connect( g_audioPlaylistView, &QListWidget::itemDoubleClicked, []( QListWidgetItem* item ){
		if ( item == nullptr || g_audioPlaylist == nullptr || g_audioPlayer == nullptr ) {
			return;
		}
		const int playlistIndex = item->data( Qt::UserRole + 1 ).toInt();
		if ( playlistIndex >= 0 ) {
			g_audioPlaylist->setCurrentIndex( playlistIndex );
			g_audioPlayer->play();
		}
	} );
	QObject::connect( g_audioPlaylistView, &QListWidget::currentRowChanged, []( int ){
		if ( g_audioPlaylistView != nullptr && g_audioPlaylist != nullptr && g_audioPlaylistView->currentItem() != nullptr ) {
			const int playlistIndex = g_audioPlaylistView->currentItem()->data( Qt::UserRole + 1 ).toInt();
			if ( playlistIndex >= 0 && playlistIndex < g_audioPlaylist->mediaCount() ) {
				g_audioPlaylist->setCurrentIndex( playlistIndex );
			}
		}
	} );
	QObject::connect( g_audioPlaylistView, &QWidget::customContextMenuRequested, []( const QPoint& pos ){
		AudioWorkbench_showPlaylistContextMenu( pos );
	} );

	QObject::connect( playButton, &QPushButton::clicked, [](){ AudioWorkbench_playSelected(); } );
	QObject::connect( pauseButton, &QPushButton::clicked, [](){
		if ( g_audioPlayer != nullptr ) {
			g_audioPlayer->pause();
		}
	} );
	QObject::connect( stopButton, &QPushButton::clicked, [](){
		if ( g_audioPlayer != nullptr ) {
			g_audioPlayer->stop();
		}
	} );
	QObject::connect( previousButton, &QPushButton::clicked, [](){
		if ( g_audioPlaylist != nullptr ) {
			g_audioPlaylist->previous();
		}
	} );
	QObject::connect( nextButton, &QPushButton::clicked, [](){
		if ( g_audioPlaylist != nullptr ) {
			g_audioPlaylist->next();
		}
	} );

	QObject::connect( g_audioSeekSlider, &QSlider::sliderMoved, []( int value ){
		if ( g_audioPlayer != nullptr ) {
			g_audioPlayer->setPosition( value );
		}
	} );
	QObject::connect( g_audioSeekSlider, &QSlider::sliderReleased, [](){
		if ( g_audioPlayer != nullptr && g_audioSeekSlider != nullptr ) {
			g_audioPlayer->setPosition( g_audioSeekSlider->value() );
		}
	} );
	QObject::connect( g_audioVolumeSlider, &QSlider::valueChanged, []( int value ){
		if ( g_audioPlayer != nullptr ) {
			g_audioPlayer->setVolume( value );
		}
		AudioWorkbench_setSetting( "Volume", value );
	} );
	QObject::connect( g_audioShuffleCheck, &QCheckBox::toggled, []( bool shuffleEnabled ){
		AudioWorkbench_setSetting( "Shuffle", shuffleEnabled );
		AudioWorkbench_applyPlaybackMode();
	} );
	QObject::connect( g_audioLoopCheck, &QCheckBox::toggled, []( bool loopEnabled ){
		AudioWorkbench_setSetting( "Loop", loopEnabled );
		AudioWorkbench_applyPlaybackMode();
	} );

	QObject::connect( g_audioPlayer, &QMediaPlayer::positionChanged, []( qint64 position ){
		if ( g_audioSeekSlider != nullptr && !g_audioSeekSlider->isSliderDown() ) {
			g_audioSeekSlider->setValue( int( position ) );
		}
		AudioWorkbench_updateTimeLabel();
	} );
	QObject::connect( g_audioPlayer, &QMediaPlayer::durationChanged, []( qint64 duration ){
		if ( g_audioSeekSlider != nullptr ) {
			g_audioSeekSlider->setRange( 0, int( qBound<qint64>( 0, duration, std::numeric_limits<int>::max() ) ) );
		}
		AudioWorkbench_updateTimeLabel();
	} );
	QObject::connect( g_audioPlaylist, &QMediaPlaylist::currentIndexChanged, []( int currentIndex ){
		if ( g_audioPlaylistView != nullptr && currentIndex >= 0 && currentIndex < g_audioPlaylistView->count() ) {
			for ( int i = 0; i < g_audioPlaylistView->count(); ++i )
			{
				if ( g_audioPlaylistView->item( i )->data( Qt::UserRole + 1 ).toInt() == currentIndex ) {
					g_audioPlaylistView->setCurrentRow( i );
					break;
				}
			}
		}
		AudioWorkbench_updateNowPlayingLabel();
		AudioWorkbench_setSetting( "CurrentIndex", currentIndex );
	} );
	QObject::connect( g_audioSearchEdit, &QLineEdit::textChanged, []( const QString& ){ AudioWorkbench_applyFilter(); } );
	if ( g_audioCategoryFilter != nullptr ) {
		QObject::connect( g_audioCategoryFilter, QOverload<int>::of( &QComboBox::currentIndexChanged ), []( int index ){
			if ( g_audioCategoryFilter == nullptr ) {
				return;
			}
			const AudioCategory category = static_cast<AudioCategory>( g_audioCategoryFilter->itemData( index ).toInt() );
			AudioWorkbench_setSetting( "CategoryFilter", static_cast<int>( category ) );
			AudioWorkbench_applyCategoryFilter();
		} );
	}
	if ( g_audioScanContentButton != nullptr ) {
		QObject::connect( g_audioScanContentButton, &QPushButton::clicked, [](){
			AudioWorkbench_scanContentFolder( true );
		} );
	}
	QObject::connect( g_audioPlayer, QOverload<QMediaPlayer::Error>::of( &QMediaPlayer::error ), []( QMediaPlayer::Error error ){
		if ( error == QMediaPlayer::NoError || g_audioPlayer == nullptr ) {
			return;
		}
		globalWarningStream() << "audio playback error: " << g_audioPlayer->errorString().toUtf8().constData() << '\n';
	} );

	AudioWorkbench_restoreAutosavedPlaylist();
	AudioWorkbench_syncPlaylistView();
	AudioWorkbench_updateTimeLabel();
	if ( g_audioPlaylist != nullptr && g_audioPlaylist->mediaCount() == 0 ) {
		AudioWorkbench_scanContentFolder( true, false );
	}
}

void AudioWorkbench_open(){
	if ( g_audioDock == nullptr ) {
		return;
	}
	g_audioDock->show();
	g_audioDock->raise();
}

void AudioWorkbench_stopAndRelease(){
	if ( g_audioPlayer != nullptr ) {
		g_audioPlayer->stop();
	}
	AudioWorkbench_saveAutosavePlaylist();
	g_audioDock = nullptr;
	g_audioPlayer = nullptr;
	g_audioPlaylist = nullptr;
	g_audioPlaylistView = nullptr;
	g_audioSeekSlider = nullptr;
	g_audioVolumeSlider = nullptr;
	g_audioNowPlayingLabel = nullptr;
	g_audioTimeLabel = nullptr;
	g_audioSearchEdit = nullptr;
	g_audioPlaylistStatsLabel = nullptr;
	g_audioShuffleCheck = nullptr;
	g_audioLoopCheck = nullptr;
	g_audioCategoryFilter = nullptr;
	g_audioScanContentButton = nullptr;
}
#else
void AudioWorkbench_createDock( QMainWindow* ){
}

void AudioWorkbench_open(){
	QMessageBox::warning(
	    MainFrame_getWindow(),
	    "Music Player / Playlist Editor",
	    "This build does not include Qt Multimedia.\nInstall Qt5Multimedia development files and rebuild."
	);
}

void AudioWorkbench_stopAndRelease(){
}
#endif
