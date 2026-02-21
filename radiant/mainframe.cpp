/*
   Copyright (C) 1999-2006 Id Software, Inc. and contributors.
   For a list of contributors, see the accompanying CONTRIBUTORS file.

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

//
// Main Window for Q3Radiant
//
// Leonardo Zide (leo@lokigames.com)
//

#include "mainframe.h"
#include "generic/callback.h"

#include "debugging/debugging.h"
#include "version.h"

#include "ifilesystem.h"
#include "ientity.h"
#include "ishaders.h"
#include "ieclass.h"
#include "irender.h"
#include "igl.h"
#include "moduleobserver.h"

#include <ctime>

#include <QWidget>
#include <QSplashScreen>
#include <QCoreApplication>
#include <QMainWindow>
#include <QLabel>
#include <QSplitter>
#include <QMenuBar>
#include <QApplication>
#include <QToolBar>
#include <QStatusBar>
#include <QBoxLayout>
#include <QDialog>
#include <QCloseEvent>
#include <QSettings>
#include <QDockWidget>
#include <QGroupBox>
#include <QDialogButtonBox>
#include <QPushButton>
#include <QMessageBox>
#include <QFormLayout>
#include <QLineEdit>
#include <QListWidget>
#include <QListWidgetItem>
#include <QTabWidget>
#include <QTextBrowser>
#include <QSlider>
#include <QTreeWidget>
#include <QFileDialog>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QProcess>
#include <QStandardPaths>
#include <QClipboard>
#include <QTextStream>
#include <QRegularExpression>
#include <QOpenGLWidget>
#include <QOpenGLFunctions>
#include <QList>

#include "commandlib.h"
#include "scenelib.h"
#include "stream/stringstream.h"
#include "signal/isignal.h"
#include "os/path.h"
#include "os/file.h"
#include <glib.h>
#include "moduleobservers.h"

#include "gtkutil/glfont.h"
#include "gtkutil/glwidget.h"
#include "gtkutil/image.h"
#include "gtkutil/menu.h"
#include "gtkutil/guisettings.h"

#include "autosave.h"
#include "build.h"
#include "brushmanip.h"
#include "camwindow.h"
#include "csg.h"
#include "commands.h"
#include "console.h"
#include "entity.h"
#include "eclasslib.h"
#include "entityinspector.h"
#include "entitylist.h"
#include "filters.h"
#include "findtexturedialog.h"
#include "grid.h"
#include "groupdialog.h"
#include "gtkdlgs.h"
#include "gtkmisc.h"
#include "help.h"
#include "map.h"
#include "mru.h"
#include "patchmanip.h"
#include "plugin.h"
#include "pluginmanager.h"
#include "pluginmenu.h"
#include "plugintoolbar.h"
#include "preferences.h"
#include "qe3.h"
#include "qgl.h"
#include "select.h"
#include "selection.h"
#include "server.h"
#include "surfacedialog.h"
#include "textures.h"
#include "texwindow.h"
#include "modelwindow.h"
#include "layerswindow.h"
#include "url.h"
#include "xywindow.h"
#include "windowobservers.h"
#include "renderstate.h"
#include "feedback.h"
#include "referencecache.h"
#include "iundo.h"

#include "colors.h"
#include "tools.h"
#include "filterbar.h"

void GamePacksPath_importString( const char* value );
void GamePacksPath_exportString( const StringImportCallback& importer );


// VFS
class VFSModuleObserver : public ModuleObserver
{
	std::size_t m_unrealised;
public:
	VFSModuleObserver() : m_unrealised( 1 ){
	}
	void realise() override {
		if ( --m_unrealised == 0 ) {
			QE_InitVFS();
			GlobalFileSystem().initialise();
		}
	}
	void unrealise() override {
		if ( ++m_unrealised == 1 ) {
			GlobalFileSystem().shutdown();
		}
	}
};

VFSModuleObserver g_VFSModuleObserver;

void VFS_Construct(){
	Radiant_attachHomePathsObserver( g_VFSModuleObserver );
}
void VFS_Destroy(){
	Radiant_detachHomePathsObserver( g_VFSModuleObserver );
}

// Home Paths

#ifdef WIN32
#include <shlobj.h>
#include <objbase.h>
const GUID qFOLDERID_SavedGames = {0x4C5C32FF, 0xBB9D, 0x43b0, {0xB5, 0xB4, 0x2D, 0x72, 0xE5, 0x4E, 0xAA, 0xA4}};
#define qREFKNOWNFOLDERID GUID
#define qKF_FLAG_CREATE 0x8000
#define qKF_FLAG_NO_ALIAS 0x1000
typedef HRESULT ( WINAPI qSHGetKnownFolderPath_t )( qREFKNOWNFOLDERID rfid, DWORD dwFlags, HANDLE hToken, PWSTR *ppszPath );
static qSHGetKnownFolderPath_t *qSHGetKnownFolderPath;
#endif
void HomePaths_Realise(){
	do
	{
		const char* prefix = g_pGameDescription->getKeyValue( "prefix" );
		if ( !string_empty( prefix ) ) {
			StringOutputStream path( 256 );

#if defined( __APPLE__ )
			path( DirectoryCleaned( g_get_home_dir() ), "Library/Application Support", ( prefix + 1 ), '/' );
			if ( file_is_directory( path ) ) {
				g_qeglobals.m_userEnginePath = path;
				break;
			}
			path( DirectoryCleaned( g_get_home_dir() ), prefix, '/' );
#endif

#if defined( WIN32 )
			TCHAR mydocsdir[MAX_PATH + 1];
			wchar_t *mydocsdirw;
			HMODULE shfolder = LoadLibrary( "shfolder.dll" );
			if ( shfolder ) {
				qSHGetKnownFolderPath = (qSHGetKnownFolderPath_t *) GetProcAddress( shfolder, "SHGetKnownFolderPath" );
			}
			else{
				qSHGetKnownFolderPath = nullptr;
			}
			CoInitializeEx( nullptr, COINIT_APARTMENTTHREADED );
			if ( qSHGetKnownFolderPath && qSHGetKnownFolderPath( qFOLDERID_SavedGames, qKF_FLAG_CREATE | qKF_FLAG_NO_ALIAS, nullptr, &mydocsdirw ) == S_OK ) {
				memset( mydocsdir, 0, sizeof( mydocsdir ) );
				wcstombs( mydocsdir, mydocsdirw, sizeof( mydocsdir ) - 1 );
				CoTaskMemFree( mydocsdirw );
				path( DirectoryCleaned( mydocsdir ), ( prefix + 1 ), '/' );
				if ( file_is_directory( path ) ) {
					g_qeglobals.m_userEnginePath = path;
					CoUninitialize();
					FreeLibrary( shfolder );
					break;
				}
			}
			CoUninitialize();
			if ( shfolder ) {
				FreeLibrary( shfolder );
			}
			if ( SUCCEEDED( SHGetFolderPath( nullptr, CSIDL_PERSONAL, nullptr, 0, mydocsdir ) ) ) {
				path( DirectoryCleaned( mydocsdir ), "My Games/", ( prefix + 1 ), '/' );
				// win32: only add it if it already exists
				if ( file_is_directory( path ) ) {
					g_qeglobals.m_userEnginePath = path;
					break;
				}
			}
#endif

#if defined( POSIX )
			path( DirectoryCleaned( g_get_home_dir() ), prefix, '/' );
			g_qeglobals.m_userEnginePath = path;
			break;
#endif
		}

		g_qeglobals.m_userEnginePath = EnginePath_get();
	}
	while ( false );

	Q_mkdir( g_qeglobals.m_userEnginePath.c_str() );

	g_qeglobals.m_userGamePath = StringStream( g_qeglobals.m_userEnginePath, gamename_get(), '/' );
	ASSERT_MESSAGE( !g_qeglobals.m_userGamePath.empty(), "HomePaths_Realise: user-game-path is empty" );
	Q_mkdir( g_qeglobals.m_userGamePath.c_str() );
}

ModuleObservers g_homePathObservers;

void Radiant_attachHomePathsObserver( ModuleObserver& observer ){
	g_homePathObservers.attach( observer );
}

void Radiant_detachHomePathsObserver( ModuleObserver& observer ){
	g_homePathObservers.detach( observer );
}

class HomePathsModuleObserver : public ModuleObserver
{
	std::size_t m_unrealised;
public:
	HomePathsModuleObserver() : m_unrealised( 1 ){
	}
	void realise() override {
		if ( --m_unrealised == 0 ) {
			HomePaths_Realise();
			g_homePathObservers.realise();
		}
	}
	void unrealise() override {
		if ( ++m_unrealised == 1 ) {
			g_homePathObservers.unrealise();
		}
	}
};

HomePathsModuleObserver g_HomePathsModuleObserver;

void HomePaths_Construct(){
	Radiant_attachEnginePathObserver( g_HomePathsModuleObserver );
}
void HomePaths_Destroy(){
	Radiant_detachEnginePathObserver( g_HomePathsModuleObserver );
}


// Engine Path

CopiedString g_strEnginePath;
ModuleObservers g_enginePathObservers;
std::size_t g_enginepath_unrealised = 1;

void Radiant_attachEnginePathObserver( ModuleObserver& observer ){
	g_enginePathObservers.attach( observer );
}

void Radiant_detachEnginePathObserver( ModuleObserver& observer ){
	g_enginePathObservers.detach( observer );
}


void EnginePath_Realise(){
	if ( --g_enginepath_unrealised == 0 ) {
		g_enginePathObservers.realise();
	}
}


const char* EnginePath_get(){
	ASSERT_MESSAGE( g_enginepath_unrealised == 0, "EnginePath_get: engine path not realised" );
	return g_strEnginePath.c_str();
}

void EnginePath_Unrealise(){
	if ( ++g_enginepath_unrealised == 1 ) {
		g_enginePathObservers.unrealise();
	}
}

static CopiedString g_installedDevFilesPath; // track last engine path, where dev files installation occured, to prompt again when changed

static void installDevFiles(){
	if( !path_equal( g_strEnginePath.c_str(), g_installedDevFilesPath.c_str() ) ){
		ASSERT_MESSAGE( g_enginepath_unrealised != 0, "installDevFiles: engine path realised" );
		DoInstallDevFilesDlg( g_strEnginePath.c_str() );
		g_installedDevFilesPath = g_strEnginePath;
	}
}

void setEnginePath( CopiedString& self, const char* value ){
	const auto buffer = StringStream( DirectoryCleaned( value ) );
	if ( !path_equal( buffer, self.c_str() ) ) {
#if 0
		while ( !ConfirmModified( "Paths Changed" ) )
		{
			if ( Map_Unnamed( g_map ) ) {
				Map_SaveAs();
			}
			else
			{
				Map_Save();
			}
		}
		Map_RegionOff();
#endif

		ScopeDisableScreenUpdates disableScreenUpdates( "Processing...", "Changing Engine Path" );

		EnginePath_Unrealise();

		self = buffer;

		installDevFiles();

		EnginePath_Realise();
	}
}
typedef ReferenceCaller<CopiedString, void(const char*), setEnginePath> EnginePathImportCaller;


// Extra Resource Path

std::array<CopiedString, 5> g_strExtraResourcePaths;

const std::array<CopiedString, 5>& ExtraResourcePaths_get(){
	return g_strExtraResourcePaths;
}


// App Path

CopiedString g_strAppPath;                 ///< holds the full path of the executable

const char* AppPath_get(){
	return g_strAppPath.c_str();
}

/// the path to the local rc-dir
const char* LocalRcPath_get(){
	static CopiedString rc_path;
	if ( rc_path.empty() ) {
		rc_path = StringStream( GlobalRadiant().getSettingsPath(), g_pGameDescription->mGameFile, '/' );
	}
	return rc_path.c_str();
}

/// directory for temp files
/// NOTE: on *nix this is were we check for .pid
CopiedString g_strSettingsPath;
const char* SettingsPath_get(){
	return g_strSettingsPath.c_str();
}

CopiedString g_strGamePacksPath;
const char* GamePacksPath_get(){
	return g_strGamePacksPath.c_str();
}

void GamePacksPath_setDefault(){
	g_strGamePacksPath = StringStream( g_strAppPath, "gamepacks/" );
}

void GamePacksPath_set( const char* path ){
	if ( string_empty( path ) ) {
		GamePacksPath_setDefault();
		return;
	}
	StringOutputStream stream( 256 );
	stream << DirectoryCleaned( path );
	g_strGamePacksPath = stream.c_str();
}


/*!
   points to the game tools directory, for instance
   C:/Program Files/Quake III Arena/GtkRadiant
   (or other games)
   this is one of the main variables that are configured by the game selection on startup
   [GameToolsPath]/plugins
   [GameToolsPath]/modules
   and also q3map, bspc
 */
CopiedString g_strGameToolsPath;           ///< this is set by g_GamesDialog

const char* GameToolsPath_get(){
	return g_strGameToolsPath.c_str();
}


void Paths_constructPreferences( PreferencesPage& page ){
	page.appendPathEntry( "Engine Path", true,
	                      StringImportCallback( EnginePathImportCaller( g_strEnginePath ) ),
	                      StringExportCallback( StringExportCaller( g_strEnginePath ) )
	                    );
	page.appendPathEntry( "Gamepacks Path", true,
	                      StringImportCallback( FreeCaller<void(const char*), GamePacksPath_importString>() ),
	                      StringExportCallback( FreeCaller<void(const StringImportCallback&), GamePacksPath_exportString>() )
	                    );
}
void Paths_constructPage( PreferenceGroup& group ){
	PreferencesPage page( group.createPage( "Paths", "Path Settings" ) );
	Paths_constructPreferences( page );
	for( auto& extraPath : g_strExtraResourcePaths )
		page.appendPathEntry( "Extra Resource Path", true,
		                      StringImportCallback( EnginePathImportCaller( extraPath ) ),
		                      StringExportCallback( StringExportCaller( extraPath ) )
		                    );
}
void Paths_registerPreferencesPage(){
	PreferencesDialog_addGamePage( makeCallbackF( Paths_constructPage ) );
}


class PathsDialog : public Dialog
{
public:
	void BuildDialog() override {
		GetWidget()->setWindowTitle( "Engine Path Configuration" );

		auto *vbox = new QVBoxLayout( GetWidget() );
		{
			auto *frame = new QGroupBox( "Path settings" );
			vbox->addWidget( frame );

			auto *grid = new QGridLayout( frame );
			grid->setAlignment( Qt::AlignmentFlag::AlignTop );
			grid->setColumnStretch( 0, 111 );
			grid->setColumnStretch( 1, 333 );
			{
				const char* engine;
#if defined( WIN32 )
				engine = g_pGameDescription->getRequiredKeyValue( "engine_win32" );
#elif defined( __linux__ ) || defined ( __FreeBSD__ )
				engine = g_pGameDescription->getRequiredKeyValue( "engine_linux" );
#elif defined( __APPLE__ )
				engine = g_pGameDescription->getRequiredKeyValue( "engine_macos" );
#else
#error "unsupported platform"
#endif
				const auto text = StringStream( "Select directory, where game executable sits (typically ", Quoted( engine ), ")\n" );
				grid->addWidget( new QLabel( text.c_str() ), 0, 0, 1, 2 );
			}
			{
				PreferencesPage preferencesPage( *this, grid );
				Paths_constructPreferences( preferencesPage );
			}
		}
		{
			auto *buttons = new QDialogButtonBox( QDialogButtonBox::StandardButton::Ok );
			vbox->addWidget( buttons );
			QObject::connect( buttons, &QDialogButtonBox::accepted, GetWidget(), &QDialog::accept );
		}
	}
};

PathsDialog g_PathsDialog;

static bool g_strEnginePath_was_empty_1st_start = false;

void EnginePath_verify(){
	if ( !file_exists( g_strEnginePath.c_str() ) || g_strEnginePath_was_empty_1st_start ) {
		g_installedDevFilesPath = ""; // trigger install for non existing engine path case
		g_PathsDialog.Create( nullptr );
		g_PathsDialog.DoModal();
		g_PathsDialog.Destroy();
	}
	installDevFiles(); // try this anytime, as engine path may be set via command line or -gamedetect
}

namespace
{
CopiedString g_gamename;
CopiedString g_gamemode;
ModuleObservers g_gameNameObservers;
ModuleObservers g_gameModeObservers;
}

void Radiant_attachGameNameObserver( ModuleObserver& observer ){
	g_gameNameObservers.attach( observer );
}

void Radiant_detachGameNameObserver( ModuleObserver& observer ){
	g_gameNameObservers.detach( observer );
}

const char* basegame_get(){
	return g_pGameDescription->getRequiredKeyValue( "basegame" );
}

const char* gamename_get(){
	if ( g_gamename.empty() ) {
		return basegame_get();
	}
	return g_gamename.c_str();
}

void gamename_set( const char* gamename ){
	if ( !string_equal( gamename, g_gamename.c_str() ) ) {
		g_gameNameObservers.unrealise();
		g_gamename = gamename;
		g_gameNameObservers.realise();
	}
}

void Radiant_attachGameModeObserver( ModuleObserver& observer ){
	g_gameModeObservers.attach( observer );
}

void Radiant_detachGameModeObserver( ModuleObserver& observer ){
	g_gameModeObservers.detach( observer );
}

const char* gamemode_get(){
	return g_gamemode.c_str();
}

void gamemode_set( const char* gamemode ){
	if ( !string_equal( gamemode, g_gamemode.c_str() ) ) {
		g_gameModeObservers.unrealise();
		g_gamemode = gamemode;
		g_gameModeObservers.realise();
	}
}

#include "os/dir.h"

const char* const c_library_extension =
#if defined( WIN32 )
    "dll"
#elif defined ( __APPLE__ )
    "dylib"
#elif defined( __linux__ ) || defined ( __FreeBSD__ )
    "so"
#endif
    ;

void Radiant_loadModules( const char* path ){
	Directory_forEach( path, matchFileExtension( c_library_extension, [&]( const char *name ){
		char fullname[1024];
		ASSERT_MESSAGE( strlen( path ) + strlen( name ) < 1024, "" );
		strcpy( fullname, path );
		strcat( fullname, name );
		globalOutputStream() << "Found " << SingleQuoted( fullname ) << '\n';
		GlobalModuleServer_loadModule( fullname );
	}));
}

void Radiant_loadModulesFromRoot( const char* directory ){
	Radiant_loadModules( StringStream( directory, g_pluginsDir ) );

	if ( !string_equal( g_pluginsDir, g_modulesDir ) ) {
		Radiant_loadModules( StringStream( directory, g_modulesDir ) );
	}
}


class WorldspawnColourEntityClassObserver : public ModuleObserver
{
	std::size_t m_unrealised;
public:
	WorldspawnColourEntityClassObserver() : m_unrealised( 1 ){
	}
	void realise() override {
		if ( --m_unrealised == 0 ) {
			SetWorldspawnColour( g_xywindow_globals.color_brushes );
		}
	}
	void unrealise() override {
		if ( ++m_unrealised == 1 ) {
		}
	}
};

WorldspawnColourEntityClassObserver g_WorldspawnColourEntityClassObserver;


ModuleObservers g_gameToolsPathObservers;

void Radiant_attachGameToolsPathObserver( ModuleObserver& observer ){
	g_gameToolsPathObservers.attach( observer );
}

void Radiant_detachGameToolsPathObserver( ModuleObserver& observer ){
	g_gameToolsPathObservers.detach( observer );
}

void Radiant_Initialise(){
	GlobalModuleServer_Initialise();

	Radiant_loadModulesFromRoot( AppPath_get() );

	Preferences_Load();

	bool success = Radiant_Construct( GlobalModuleServer_get() );
	ASSERT_MESSAGE( success, "module system failed to initialise - see radiant.log for error messages" );

	g_gameToolsPathObservers.realise();
	g_gameModeObservers.realise();
	g_gameNameObservers.realise();
}

void Radiant_Shutdown(){
	g_gameNameObservers.unrealise();
	g_gameModeObservers.unrealise();
	g_gameToolsPathObservers.unrealise();

	if ( !g_preferences_globals.disable_ini ) {
		globalOutputStream() << "Start writing prefs\n";
		Preferences_Save();
		globalOutputStream() << "Done prefs\n";
	}

	Radiant_Destroy();

	GlobalModuleServer_Shutdown();
}

void Exit(){
	if ( ConfirmModified( "Exit Radiant" ) ) {
		QCoreApplication::quit();
	}
}

#include "environment.h"

#ifdef WIN32
#include <process.h>
#else
#include <spawn.h>
/* According to the Single Unix Specification, environ is not
 * in any system header, although unistd.h often declares it.
 */
extern char **environ;
#endif
void Radiant_Restart(){
	if( ConfirmModified( "Restart Radiant" ) ){
		const auto mapname = StringStream( Quoted( Map_Name( g_map ) ) );

		char *argv[] = { string_clone( environment_get_app_filepath() ),
	                     Map_Unnamed( g_map )? nullptr : string_clone( mapname ),
	                     nullptr };
#ifdef WIN32
		const int status = !_spawnv( P_NOWAIT, argv[0], argv );
#else
		const int status = posix_spawn( nullptr, argv[0], nullptr, nullptr, argv, environ );
#endif

		// quit if radiant successfully started
		if ( status == 0 ) {
			QCoreApplication::quit();
		}
	}
}


void Restart(){
	PluginsMenu_clear();
	PluginToolbar_clear();

	Radiant_Shutdown();
	Radiant_Initialise();

	PluginsMenu_populate();

	PluginToolbar_populate();
}


void OpenUpdateURL(){
	OpenURL( "https://github.com/Garux/netradiant-custom/releases/latest" );
#if 0
	// build the URL
	StringOutputStream URL( 256 );
	URL << "http://www.icculus.org/netradiant/?cmd=update&data=dlupdate&query_dlup=1";
#ifdef WIN32
	URL << "&OS_dlup=1";
#elif defined( __APPLE__ )
	URL << "&OS_dlup=2";
#else
	URL << "&OS_dlup=3";
#endif
	URL << "&Version_dlup=" RADIANT_VERSION;
	g_GamesDialog.AddPacksURL( URL );
	OpenURL( URL );
#endif
}

// open the Q3Rad manual
void OpenHelpURL(){
	// at least on win32, AppPath + "docs/index.html"
	OpenURL( StringStream( AppPath_get(), "docs/index.html" ) );
}

void OpenBugReportURL(){
	// OpenURL( "http://www.icculus.org/netradiant/?cmd=bugs" );
	OpenURL( "https://github.com/Garux/netradiant-custom/issues" );
}


QWidget* g_page_console;

void Console_ToggleShow(){
	GroupDialog_showPage( g_page_console );
}

QWidget* g_page_entity;

void EntityInspector_ToggleShow(){
	GroupDialog_showPage( g_page_entity );
}

QWidget* g_page_models;

void ModelBrowser_ToggleShow(){
	GroupDialog_showPage( g_page_models );
}

QWidget* g_page_layers;

void LayersBrowser_ToggleShow(){
	GroupDialog_showPage( g_page_layers);
}


static class EverySecondTimer
{
	QTimer m_timer;
public:
	EverySecondTimer(){
		m_timer.setInterval( 1000 );
		m_timer.callOnTimeout( [](){
			if ( QGuiApplication::mouseButtons().testFlag( Qt::MouseButton::NoButton ) ) {
				QE_CheckAutoSave();
			}
		} );
	}
	void enable(){
		m_timer.start();
	}
	void disable(){
		m_timer.stop();
	}
}
s_qe_every_second_timer;


class WaitDialog
{
public:
	QWidget* m_window;
	QLabel* m_label;
};

WaitDialog create_wait_dialog( const char* title, const char* text ){
	/* Qt::Tool window type doesn't steal focus, which saves e.g. from losing freelook camera mode on autosave
	   or entity menu from hiding, while clicked with ctrl, by tex/model loading popup.
	   Qt::WidgetAttribute::WA_ShowWithoutActivating is implied, but lets have it set too. */
	auto *window = new QWidget( MainFrame_getWindow(), Qt::Tool | Qt::WindowTitleHint );
	window->setWindowTitle( title );
	window->setWindowModality( Qt::WindowModality::ApplicationModal );
	window->setAttribute( Qt::WidgetAttribute::WA_ShowWithoutActivating );

	auto *label = new QLabel( text );
	{
		auto *box = new QHBoxLayout( window );
		box->setSizeConstraint( QLayout::SizeConstraint::SetFixedSize );
		box->setContentsMargins( 20, 5, 20, 3 );
		box->addWidget( label );
		label->setMinimumWidth( 200 );
	}
	return WaitDialog{ .m_window = window, .m_label = label };
}

namespace
{
clock_t g_lastRedrawTime = 0;
const clock_t c_redrawInterval = clock_t( CLOCKS_PER_SEC / 10 );

bool redrawRequired(){
	clock_t currentTime = std::clock();
	if ( currentTime - g_lastRedrawTime >= c_redrawInterval ) {
		g_lastRedrawTime = currentTime;
		return true;
	}
	return false;
}
}

typedef std::list<CopiedString> StringStack;
StringStack g_wait_stack;
WaitDialog g_wait;

bool ScreenUpdates_Enabled(){
	return g_wait_stack.empty();
}

void ScreenUpdates_process(){
	if ( redrawRequired() ) {
		process_gui();
	}
}


void ScreenUpdates_Disable( const char* message, const char* title ){
	if ( g_wait_stack.empty() ) {
		s_qe_every_second_timer.disable();

		process_gui();

		g_wait = create_wait_dialog( title, message );

		g_wait.m_window->show();
		ScreenUpdates_process();
	}
	else {
		g_wait.m_window->setWindowTitle( title );
		g_wait.m_label->setText( message );
		ScreenUpdates_process();
	}
	g_wait_stack.push_back( message );
}

void ScreenUpdates_Enable(){
	ASSERT_MESSAGE( !ScreenUpdates_Enabled(), "screen updates already enabled" );
	g_wait_stack.pop_back();
	if ( g_wait_stack.empty() ) {
		s_qe_every_second_timer.enable();

		delete std::exchange( g_wait.m_window, nullptr );
	}
	else {
		g_wait.m_label->setText( g_wait_stack.back().c_str() );
		ScreenUpdates_process();
	}
}



void GlobalCamera_UpdateWindow(){
	if ( g_pParentWnd != 0 ) {
		CamWnd_Update( *g_pParentWnd->GetCamWnd() );
	}
}

void XY_UpdateAllWindows(){
	if ( g_pParentWnd != 0 ) {
		g_pParentWnd->forEachXYWnd( []( XYWnd* xywnd ){
			XYWnd_Update( *xywnd );
		} );
	}
}

void UpdateAllWindows(){
	GlobalCamera_UpdateWindow();
	XY_UpdateAllWindows();
}


LatchedInt g_Layout_viewStyle( 0, "Window Layout" );
LatchedBool g_Layout_enableDetachableMenus( true, "Detachable Menus" );
LatchedBool g_Layout_builtInGroupDialog( false, "Built-In Group Dialog" );
LatchedBool g_Layout_expiramentalFeatures( false, "Expiramental Features" );
bool Layout_expiramentalFeaturesEnabled(){
	return g_Layout_expiramentalFeatures.m_value;
}

namespace
{
QDockWidget* g_exp_propertiesDock{};
QDockWidget* g_exp_previewDock{};
QDockWidget* g_exp_assetsDock{};
QDockWidget* g_exp_historyDock{};
QDockWidget* g_exp_usdDock{};
QLabel* g_exp_selectedCountLabel{};
QLabel* g_exp_selectedComponentsLabel{};
QLineEdit* g_exp_shaderEdit{};
QListWidget* g_exp_assetsList{};
QListWidget* g_exp_historyList{};
QTreeWidget* g_exp_usdTree{};
std::size_t g_exp_historyCounter{};
bool g_exp_undoTrackerAttached{};

class ExperimentalPreviewWidget final : public QOpenGLWidget, protected QOpenGLFunctions
{
	void initializeGL() override {
		initializeOpenGLFunctions();
	}
	void paintGL() override {
		glClearColor( 0.14f, 0.14f, 0.16f, 1.0f );
		glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );
	}
};

class ExperimentalUndoTracker final : public UndoTracker
{
	void addEvent( const char* event ) const {
		if ( g_exp_historyList == nullptr ) {
			return;
		}
		g_exp_historyList->addItem( StringStream( '#', ++g_exp_historyCounter, ' ', event ).c_str() );
		g_exp_historyList->scrollToBottom();
	}
public:
	void clear() override {
		g_exp_historyCounter = 0;
		if ( g_exp_historyList != nullptr ) {
			g_exp_historyList->clear();
		}
	}
	void begin() override {
		addEvent( "Begin change" );
	}
	void undo() override {
		addEvent( "Undo" );
	}
	void redo() override {
		addEvent( "Redo" );
	}
};
ExperimentalUndoTracker g_experimentalUndoTracker;

void Experimental_setUndoTrackerAttached( bool attached ){
	if ( attached && !g_exp_undoTrackerAttached ) {
		GlobalUndoSystem().trackerAttach( g_experimentalUndoTracker );
		g_exp_undoTrackerAttached = true;
	}
	else if ( !attached && g_exp_undoTrackerAttached ) {
		GlobalUndoSystem().trackerDetach( g_experimentalUndoTracker );
		g_exp_undoTrackerAttached = false;
	}
}

void Experimental_refreshSelection(){
	if ( g_exp_selectedCountLabel != nullptr ) {
		g_exp_selectedCountLabel->setText( StringStream( GlobalSelectionSystem().countSelected() ).c_str() );
	}
	if ( g_exp_selectedComponentsLabel != nullptr ) {
		g_exp_selectedComponentsLabel->setText( StringStream( GlobalSelectionSystem().countSelectedComponents() ).c_str() );
	}
}

void Experimental_selectionChanged( const Selectable& ){
	Experimental_refreshSelection();
}

void Experimental_applySelectedShader(){
	if ( g_exp_shaderEdit == nullptr || g_exp_shaderEdit->text().isEmpty() ) {
		return;
	}

	const auto shader = g_exp_shaderEdit->text().trimmed().toLatin1();
	if ( shader.isEmpty() ) {
		return;
	}

	Select_SetShader_Undo( shader.constData() );
	UpdateAllWindows();
}

struct ExperimentalShaderNameVisitor
{
	void operator()( const char* name ) const {
		if ( g_exp_assetsList != nullptr ) {
			g_exp_assetsList->addItem( name );
		}
	}
};

void Experimental_refreshAssetLibrary(){
	if ( g_exp_assetsList == nullptr ) {
		return;
	}
	g_exp_assetsList->clear();
	GlobalShaderSystem().foreachShaderName( makeCallback( ExperimentalShaderNameVisitor() ) );
	g_exp_assetsList->sortItems();
}

void Experimental_toggleDock( QDockWidget* dock ){
	if ( dock != nullptr ) {
		dock->setVisible( !dock->isVisible() );
	}
}

void Experimental_togglePropertiesDock(){
	Experimental_toggleDock( g_exp_propertiesDock );
}
void Experimental_togglePreviewDock(){
	Experimental_toggleDock( g_exp_previewDock );
}
void Experimental_toggleAssetsDock(){
	Experimental_toggleDock( g_exp_assetsDock );
}
void Experimental_toggleHistoryDock(){
	Experimental_toggleDock( g_exp_historyDock );
}
void Experimental_toggleUSDDock(){
	Experimental_toggleDock( g_exp_usdDock );
}

void Experimental_styleDockTitle( QDockWidget* dock, const char* title ){
	if ( dock == nullptr ) {
		return;
	}
	const auto labelText = StringStream( "[NEW] ", title );
	dock->setWindowTitle( labelText.c_str() );
	auto* label = new QLabel( labelText.c_str(), dock );
	label->setStyleSheet( "QLabel { color: #58c95a; font-weight: 600; padding-left: 6px; }" );
	dock->setTitleBarWidget( label );
}

void Experimental_importUSDStructure(){
	if ( !g_Layout_expiramentalFeatures.m_value || g_exp_usdTree == nullptr ) {
		return;
	}

	const auto filename = QFileDialog::getOpenFileName( MainFrame_getWindow(), "Import USD Structure", "", "USD Files (*.usd *.usda *.usdc)" );
	if ( filename.isEmpty() ) {
		return;
	}

	QFile file( filename );
	if ( !file.open( QIODevice::ReadOnly | QIODevice::Text ) ) {
		globalErrorStream() << "failed to open USD file: " << filename.toLatin1().constData() << '\n';
		return;
	}

	g_exp_usdTree->clear();

	QTextStream stream( &file );
	QList<QTreeWidgetItem*> stack;
	const QRegularExpression defRegex( "^def\\s+\\w+\\s+\"([^\"]+)\"" );

	while ( !stream.atEnd() )
	{
		const auto line = stream.readLine().trimmed();

		if ( line.startsWith( '}' ) ) {
			if ( !stack.isEmpty() ) {
				stack.removeLast();
			}
			continue;
		}

		const auto match = defRegex.match( line );
		if ( !match.hasMatch() ) {
			continue;
		}

		auto* item = new QTreeWidgetItem( QStringList( match.captured( 1 ) ) );
		if ( stack.isEmpty() ) {
			g_exp_usdTree->addTopLevelItem( item );
		}
		else{
			stack.back()->addChild( item );
		}

		if ( line.contains( '{' ) ) {
			stack.push_back( item );
		}
	}

	g_exp_usdTree->expandAll();
	if ( g_exp_usdDock != nullptr ) {
		g_exp_usdDock->show();
	}
}

void Experimental_createDocks( QMainWindow* window ){
	if ( !g_Layout_expiramentalFeatures.m_value || window == nullptr ) {
		return;
	}

	Experimental_setUndoTrackerAttached( true );

	g_exp_propertiesDock = new QDockWidget( "Properties", window );
	Experimental_styleDockTitle( g_exp_propertiesDock, "Properties" );
	{
		auto* root = new QWidget( g_exp_propertiesDock );
		auto* form = new QFormLayout( root );
		g_exp_selectedCountLabel = new QLabel( "0", root );
		g_exp_selectedComponentsLabel = new QLabel( "0", root );
		g_exp_shaderEdit = new QLineEdit( root );
		auto* applyButton = new QPushButton( "Apply Shader", root );
		form->addRow( "Selected", g_exp_selectedCountLabel );
		form->addRow( "Selected Components", g_exp_selectedComponentsLabel );
		form->addRow( "Shader", g_exp_shaderEdit );
		form->addRow( "", applyButton );
		QObject::connect( applyButton, &QPushButton::clicked, [](){ Experimental_applySelectedShader(); } );
		g_exp_propertiesDock->setWidget( root );
	}
	window->addDockWidget( Qt::RightDockWidgetArea, g_exp_propertiesDock );

	g_exp_previewDock = new QDockWidget( "Preview", window );
	Experimental_styleDockTitle( g_exp_previewDock, "Preview" );
	g_exp_previewDock->setWidget( new ExperimentalPreviewWidget );
	window->addDockWidget( Qt::RightDockWidgetArea, g_exp_previewDock );

	g_exp_assetsDock = new QDockWidget( "Asset Library", window );
	Experimental_styleDockTitle( g_exp_assetsDock, "Asset Library" );
	{
		auto* root = new QWidget( g_exp_assetsDock );
		auto* vbox = new QVBoxLayout( root );
		g_exp_assetsList = new QListWidget( root );
		g_exp_assetsList->setViewMode( QListView::IconMode );
		g_exp_assetsList->setUniformItemSizes( true );
		g_exp_assetsList->setResizeMode( QListView::Adjust );
		g_exp_assetsList->setDragEnabled( true );
		auto* refreshButton = new QPushButton( "Refresh Assets", root );
		vbox->addWidget( g_exp_assetsList );
		vbox->addWidget( refreshButton );
		QObject::connect( refreshButton, &QPushButton::clicked, [](){ Experimental_refreshAssetLibrary(); } );
		QObject::connect( g_exp_assetsList, &QListWidget::itemDoubleClicked, []( QListWidgetItem* item ){
			if ( item != nullptr && g_exp_shaderEdit != nullptr ) {
				g_exp_shaderEdit->setText( item->text() );
				Experimental_applySelectedShader();
			}
		} );
		g_exp_assetsDock->setWidget( root );
	}
	window->addDockWidget( Qt::LeftDockWidgetArea, g_exp_assetsDock );

	g_exp_historyDock = new QDockWidget( "History", window );
	Experimental_styleDockTitle( g_exp_historyDock, "History" );
	{
		g_exp_historyList = new QListWidget( g_exp_historyDock );
		g_exp_historyDock->setWidget( g_exp_historyList );
	}
	window->addDockWidget( Qt::LeftDockWidgetArea, g_exp_historyDock );

	g_exp_usdDock = new QDockWidget( "USD Structure", window );
	Experimental_styleDockTitle( g_exp_usdDock, "USD Structure" );
	{
		auto* root = new QWidget( g_exp_usdDock );
		auto* vbox = new QVBoxLayout( root );
		auto* importButton = new QPushButton( "Import USD Structure...", root );
		g_exp_usdTree = new QTreeWidget( root );
		g_exp_usdTree->setHeaderLabels( QStringList( "Prim" ) );
		vbox->addWidget( importButton );
		vbox->addWidget( g_exp_usdTree );
		QObject::connect( importButton, &QPushButton::clicked, [](){ Experimental_importUSDStructure(); } );
		g_exp_usdDock->setWidget( root );
	}
	window->addDockWidget( Qt::LeftDockWidgetArea, g_exp_usdDock );

	window->tabifyDockWidget( g_exp_propertiesDock, g_exp_previewDock );
	window->tabifyDockWidget( g_exp_assetsDock, g_exp_historyDock );
	window->tabifyDockWidget( g_exp_historyDock, g_exp_usdDock );

	Experimental_refreshSelection();
	Experimental_refreshAssetLibrary();
}

void Experimental_destroyDocks(){
	Experimental_setUndoTrackerAttached( false );
	g_exp_propertiesDock = nullptr;
	g_exp_previewDock = nullptr;
	g_exp_assetsDock = nullptr;
	g_exp_historyDock = nullptr;
	g_exp_usdDock = nullptr;
	g_exp_selectedCountLabel = nullptr;
	g_exp_selectedComponentsLabel = nullptr;
	g_exp_shaderEdit = nullptr;
	g_exp_assetsList = nullptr;
	g_exp_historyList = nullptr;
	g_exp_usdTree = nullptr;
	g_exp_historyCounter = 0;
}
}



void create_file_menu( QMenuBar *menubar ){
	// File menu
	QMenu *menu = menubar->addMenu( "&File" );

	menu->setTearOffEnabled( g_Layout_enableDetachableMenus.m_value );

	create_menu_item_with_mnemonic( menu, "&New Map", "NewMap" );
	menu->addSeparator();

	create_menu_item_with_mnemonic( menu, "&Open...", "OpenMap" );
	create_menu_item_with_mnemonic( menu, "&Import...", "ImportMap" );
	menu->addSeparator();
	create_menu_item_with_mnemonic( menu, "&Save", "SaveMap" );
	create_menu_item_with_mnemonic( menu, "Save &as...", "SaveMapAs" );
	create_menu_item_with_mnemonic( menu, "Save s&elected...", "SaveSelected" );
	create_menu_item_with_mnemonic( menu, "Save re&gion...", "SaveRegion" );
	menu->addSeparator();
	create_menu_item_with_mnemonic( menu, "&Pointfile", "TogglePointfile" );
	menu->addSeparator();
	MRU_constructMenu( menu );
	menu->addSeparator();
	create_menu_item_with_mnemonic( menu, "E&xit", "Exit" );
}

void create_edit_menu( QMenuBar *menubar ){
	// Edit menu
	QMenu *menu = menubar->addMenu( "&Edit" );

	menu->setTearOffEnabled( g_Layout_enableDetachableMenus.m_value );

	create_menu_item_with_mnemonic( menu, "&Undo", "Undo" );
	create_menu_item_with_mnemonic( menu, "&Redo", "Redo" );
	menu->addSeparator();
	create_menu_item_with_mnemonic( menu, "&Copy", "Copy" );
	create_menu_item_with_mnemonic( menu, "&Paste", "Paste" );
	create_menu_item_with_mnemonic( menu, "P&aste To Camera", "PasteToCamera" );
	create_menu_item_with_mnemonic( menu, "Move To Camera", "MoveToCamera" );
	menu->addSeparator();
	create_menu_item_with_mnemonic( menu, "&Duplicate", "CloneSelection" );
	create_menu_item_with_mnemonic( menu, "Duplicate, make uni&que", "CloneSelectionAndMakeUnique" );
	create_menu_item_with_mnemonic( menu, "D&elete", "DeleteSelection" );
	//create_menu_item_with_mnemonic( menu, "Pa&rent", "ParentSelection" );
	menu->addSeparator();
	create_menu_item_with_mnemonic( menu, "C&lear Selection", "UnSelectSelection" );
	create_menu_item_with_mnemonic( menu, "&Invert Selection", "InvertSelection" );
	create_menu_item_with_mnemonic( menu, "Select i&nside", "SelectInside" );
	create_menu_item_with_mnemonic( menu, "Select &touching", "SelectTouching" );

	menu->addSeparator();

	create_menu_item_with_mnemonic( menu, "Select All Of Type", "SelectAllOfType" );
	create_menu_item_with_mnemonic( menu, "Select Textured", "SelectTextured" );
	create_menu_item_with_mnemonic( menu, "&Expand Selection To Primitives", "ExpandSelectionToPrimitives" );
	create_menu_item_with_mnemonic( menu, "&Expand Selection To Entities", "ExpandSelectionToEntities" );
	create_menu_item_with_mnemonic( menu, "&Expand Selection To Layers", "ExpandSelectionToLayers" );
	create_menu_item_with_mnemonic( menu, "Select Connected Entities", "SelectConnectedEntities" );

	menu->addSeparator();
	create_menu_item_with_mnemonic( menu, "&Shortcuts...", "Shortcuts" );
	create_menu_item_with_mnemonic( menu, "Pre&ferences...", "Preferences" );
}

namespace
{
Vector3 Add_entitySpawnOrigin(){
	if ( g_pParentWnd != nullptr && g_pParentWnd->GetCamWnd() != nullptr ) {
		return Camera_getOrigin( *g_pParentWnd->GetCamWnd() );
	}
	return g_vector3_identity;
}

void Add_createMiscModel();

void Add_createEntity( const char* classname ){
	if ( classname_equal( classname, "misc_model" ) ) {
		Add_createMiscModel();
		return;
	}
	Entity_createFromSelection( classname, Add_entitySpawnOrigin() );
}

void Add_createLight(){
	Add_createEntity( "light" );
}

void Add_createInfoPlayerStart(){
	Add_createEntity( "info_player_start" );
}

void Add_createInfoPlayerDeathmatch(){
	Add_createEntity( "info_player_deathmatch" );
}

void Add_createMiscModel(){
	const char* modelPath = misc_model_dialog( MainFrame_getWindow() );
	if ( modelPath == nullptr ) {
		return;
	}

	UndoableCommand undo( "insertModel" );
	EntityClass* entityClass = GlobalEntityClassManager().findOrInsert( "misc_model", false );
	NodeSmartReference node( GlobalEntityCreator().createEntity( entityClass ) );

	Node_getTraversable( GlobalSceneGraph().root() )->insert( node );

	scene::Path entitypath( makeReference( GlobalSceneGraph().root() ) );
	entitypath.push( makeReference( node.get() ) );
	scene::Instance& instance = findInstance( entitypath );

	if ( Transformable* transform = Instance_getTransformable( instance ) ) {
		transform->setType( TRANSFORM_PRIMITIVE );
		transform->setTranslation( Add_entitySpawnOrigin() );
		transform->freezeTransform();
	}

	GlobalSelectionSystem().setSelectedAll( false );
	Instance_setSelected( instance, true );

	Node_getEntity( node )->setKeyValue( entityClass->miscmodel_key(), modelPath );
}

class AddEntityClassCollector final : public EntityClassVisitor
{
	QList<QString>& m_names;
public:
	AddEntityClassCollector( QList<QString>& names ) : m_names( names ){
	}
	void visit( EntityClass* entityClass ) override {
		m_names.push_back( entityClass->name() );
	}
};

void Add_openEntityDialog(){
	QDialog dialog( MainFrame_getWindow() );
	dialog.setWindowTitle( "Add Entity" );
	dialog.setModal( true );
	dialog.resize( 560, 480 );

	auto* layout = new QVBoxLayout( &dialog );
	auto* filterEdit = new QLineEdit( &dialog );
	filterEdit->setPlaceholderText( "Filter entity classes..." );
	layout->addWidget( filterEdit );

	auto* list = new QListWidget( &dialog );
	list->setSelectionMode( QAbstractItemView::SelectionMode::SingleSelection );
	layout->addWidget( list, 1 );

	auto* buttons = new QDialogButtonBox( QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog );
	buttons->button( QDialogButtonBox::Ok )->setText( "Add" );
	layout->addWidget( buttons );

	QList<QString> names;
	{
		AddEntityClassCollector collector( names );
		GlobalEntityClassManager().forEach( collector );
	}
	names.sort( Qt::CaseInsensitive );

	const auto refill = [&](){
		const auto filter = filterEdit->text();
		list->clear();
		for ( const auto& name : names )
		{
			if ( filter.isEmpty() || name.contains( filter, Qt::CaseInsensitive ) ) {
				list->addItem( name );
			}
		}
		if ( list->count() > 0 ) {
			list->setCurrentRow( 0 );
		}
	};

	QObject::connect( filterEdit, &QLineEdit::textChanged, [&](){ refill(); } );
	QObject::connect( buttons, &QDialogButtonBox::accepted, [&](){
		if ( list->currentItem() != nullptr ) {
			dialog.accept();
		}
	} );
	QObject::connect( buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject );
	QObject::connect( list, &QListWidget::itemDoubleClicked, [&]( QListWidgetItem* item ){
		if ( item != nullptr ) {
			dialog.accept();
		}
	} );

	refill();
	filterEdit->setFocus();

	if ( dialog.exec() == QDialog::DialogCode::Accepted && list->currentItem() != nullptr ) {
		const auto classname = list->currentItem()->text().toLatin1();
		Add_createEntity( classname.constData() );
	}
}

Vector3 g_cameraBookmarks_origin[5];
Vector3 g_cameraBookmarks_angles[5];
bool g_cameraBookmarks_valid[5]{};

void CameraBookmark_store( std::size_t index ){
	if ( g_pParentWnd == nullptr || g_pParentWnd->GetCamWnd() == nullptr || index >= 5 ) {
		return;
	}
	g_cameraBookmarks_origin[index] = Camera_getOrigin( *g_pParentWnd->GetCamWnd() );
	g_cameraBookmarks_angles[index] = Camera_getAngles( *g_pParentWnd->GetCamWnd() );
	g_cameraBookmarks_valid[index] = true;
	Sys_Status( StringStream( "Stored camera bookmark ", index + 1 ).c_str() );
}

void CameraBookmark_recall( std::size_t index ){
	if ( g_pParentWnd == nullptr || g_pParentWnd->GetCamWnd() == nullptr || index >= 5 ) {
		return;
	}
	if ( !g_cameraBookmarks_valid[index] ) {
		Sys_Status( StringStream( "Camera bookmark ", index + 1, " is empty" ).c_str() );
		return;
	}
	Camera_setOrigin( *g_pParentWnd->GetCamWnd(), g_cameraBookmarks_origin[index] );
	Camera_setAngles( *g_pParentWnd->GetCamWnd(), g_cameraBookmarks_angles[index] );
	UpdateAllWindows();
	Sys_Status( StringStream( "Recalled camera bookmark ", index + 1 ).c_str() );
}

struct IdTech3ToolDef
{
	const char* name;
	const char* executable;
	const char* description;
};

IdTech3ToolDef g_idTech3Tools[] = {
	{ "Q3Map2++", "q3map2.x86_64", "Primary id Tech 3 map compiler (BSP/VIS/LIGHT stages)." },
	{ "QData3++", "qdata3.x86_64", "Asset compile pipeline for models/sprites and game data." },
	{ "Q2Map++", "q2map.x86_64", "Legacy Quake II style map compile utility." },
	{ "MBSPC++", "mbspc.x86_64", "Bot navigation compiler for BSP maps." },
};

QString IdTech3Tool_executablePath( const char* executable ){
	return QDir( QString::fromLatin1( AppPath_get() ) ).filePath( executable );
}

void IdTech3Tool_copyHelpCommand( const IdTech3ToolDef& tool ){
	const auto command = StringStream( '"', IdTech3Tool_executablePath( tool.executable ).toUtf8().constData(), "\" --help" );
	QGuiApplication::clipboard()->setText( command.c_str() );
	Sys_Status( StringStream( "Copied command: ", tool.name ).c_str() );
}

void IdTech3Tool_runHelp( const IdTech3ToolDef& tool ){
	const auto executable = IdTech3Tool_executablePath( tool.executable );
	if ( !QFileInfo::exists( executable ) ) {
		QMessageBox::warning( MainFrame_getWindow(), "Tool not found", StringStream( "Missing executable:\n", executable.toUtf8().constData() ).c_str() );
		return;
	}
	if ( !QProcess::startDetached( executable, { "--help" }, QFileInfo( executable ).absolutePath() ) ) {
		QMessageBox::warning( MainFrame_getWindow(), "Launch failed", StringStream( "Failed to start:\n", executable.toUtf8().constData() ).c_str() );
		return;
	}
	Sys_Status( StringStream( "Launched ", tool.name, " --help" ).c_str() );
}

void IdTech3Tool_openHubDialog(){
	QDialog dialog( MainFrame_getWindow() );
	dialog.setWindowTitle( "Id Tech 3 Tool Center" );
	dialog.setModal( true );
	dialog.resize( 860, 620 );

	auto* layout = new QVBoxLayout( &dialog );
	auto* tabs = new QTabWidget( &dialog );
	layout->addWidget( tabs, 1 );

	auto addTextTab = [tabs]( const char* title, const char* html ){
		auto* text = new QTextBrowser( tabs );
		text->setOpenExternalLinks( true );
		text->setHtml( html );
		tabs->addTab( text, title );
	};

	addTextTab( "Index", R"HTML(
<h2>Id Tech 3 Tool Center</h2>
<p>Hammer++-inspired quality-of-life hub for this Radiant fork.</p>
<p><b>Scope:</b> id Tech 3 and mod workflows (including custom PBR shader pipelines), not Source/VTF.</p>
<p><b>Sections:</b> Index, Features, Updates, Download, Credits, Tools.</p>
<p><b>Discussion:</b> add your team Discord/community link in this tab if you want quick access from the editor.</p>
<p><b>Note:</b> compiler and external-tool redistribution policy remains up to tool authors and your project licenses.</p>
)HTML" );
	addTextTab( "Features", R"HTML(
<h3>Implemented</h3>
<ul>
<li>Add menu with direct placement actions and searchable <b>Add Entity...</b> picker.</li>
<li>Hammer-style 4-pane layout preset (3D + Top + Front + Side).</li>
<li>Camera bookmarks: <b>Ctrl+1..5</b> store, <b>Shift+1..5</b> recall.</li>
<li>Id Tech 3 Tool Center with compiler quick actions.</li>
<li>Model add flow hardened to avoid misc_model graph corruption/asserts.</li>
</ul>
<h3>Planned Hammer++ Parity Track (id Tech 3 adaptation)</h3>
<ul>
<li>Realtime lighting/material preview modes (legacy + PBR visualization).</li>
<li>Instance workflow equivalent for prefab-like reuse and live preview context.</li>
<li>Improved color picker (RGB/HSV presets, per-map palette behavior).</li>
<li>Editor-object visibility toggles (helpers, tool textures, debug sprites).</li>
<li>Model/material hot-reload and richer particle preview controls.</li>
<li>Gizmo + pivot workflow upgrades and local/global transform toggles.</li>
<li>Face/UV tooling upgrades and clipping/vertex editing QoL refinements.</li>
<li>Advanced browsers (model/material/particle) with better filtering.</li>
</ul>
<h3>Out of Scope / Engine-Specific Mapping</h3>
<ul>
<li>Source-only items (VMF/VTF/VBSP/VVIS/VRAD semantics) are replaced with id Tech 3 equivalents.</li>
<li>Skybox/fog/render-effect previews are targeted to id Tech 3 entity/shader conventions.</li>
</ul>
)HTML" );
	addTextTab( "Updates", R"HTML(
<h3>Recent</h3>
<ul>
<li>Added Add menu, entity picker, and safe model insertion path.</li>
<li>Added Hammer-style 4-pane preset and bookmark camera workflow.</li>
<li>Added Tools menu + Id Tech 3 Tool Center.</li>
</ul>
<h3>Next Up</h3>
<ul>
<li>PBR shader workflow page and validation commands in this hub.</li>
<li>Toolbar toggles for helper visibility and preview channels.</li>
<li>First-pass lighting preview controls in 3D view.</li>
</ul>
)HTML" );
	addTextTab( "Download",
	            StringStream( "<p><b>Install path:</b></p><pre>", AppPath_get(), "</pre>"
	                          "<p>Expected binary/tool location for this editor build.</p>"
	                          "<p>Current bundled compilers are id Tech 3 oriented (q3map2/qdata3/mbspc/etc).</p>" ).c_str() );
	addTextTab( "Credits", R"HTML(
<p>Design direction inspired by Hammer++ quality-of-life evolution.</p>
<p>Implementation adapted for id Tech 3 editing and compile workflows.</p>
<p>Thanks to Radiant maintainers, gamepack maintainers, and community tool authors.</p>
)HTML" );

	auto* toolsTab = new QWidget( tabs );
	auto* toolsLayout = new QVBoxLayout( toolsTab );
	auto* toolsList = new QListWidget( toolsTab );
	toolsLayout->addWidget( toolsList, 1 );
	auto* buttonRow = new QHBoxLayout();
	auto* runHelpButton = new QPushButton( "Run --help", toolsTab );
	auto* copyButton = new QPushButton( "Copy Command", toolsTab );
	buttonRow->addWidget( runHelpButton );
	buttonRow->addWidget( copyButton );
	buttonRow->addStretch( 1 );
	toolsLayout->addLayout( buttonRow );
	for ( std::size_t index = 0; index < std::size( g_idTech3Tools ); ++index )
	{
		const auto& tool = g_idTech3Tools[index];
		auto* item = new QListWidgetItem( StringStream( tool.name, " — ", tool.description ).c_str(), toolsList );
		item->setData( Qt::UserRole, int( index ) );
	}
	toolsList->setCurrentRow( 0 );
	QObject::connect( runHelpButton, &QPushButton::clicked, [toolsList](){
		if ( auto* item = toolsList->currentItem() ) {
			IdTech3Tool_runHelp( g_idTech3Tools[item->data( Qt::UserRole ).toInt()] );
		}
	} );
	QObject::connect( copyButton, &QPushButton::clicked, [toolsList](){
		if ( auto* item = toolsList->currentItem() ) {
			IdTech3Tool_copyHelpCommand( g_idTech3Tools[item->data( Qt::UserRole ).toInt()] );
		}
	} );
	tabs->addTab( toolsTab, "Tools" );

	auto* closeButtons = new QDialogButtonBox( QDialogButtonBox::Close, &dialog );
	layout->addWidget( closeButtons );
	QObject::connect( closeButtons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject );

	dialog.exec();
}

QProcess* g_audioPreviewProcess{};
QString g_audioPreviewProgram;

QString AudioPreview_findPlayerProgram(){
	if ( !g_audioPreviewProgram.isEmpty() ) {
		return g_audioPreviewProgram;
	}
	for ( const auto& candidate : { "ffplay", "mpv", "vlc", "cvlc", "mpg123", "mplayer", "xdg-open" } )
	{
		const auto found = QStandardPaths::findExecutable( candidate );
		if ( !found.isEmpty() ) {
			g_audioPreviewProgram = found;
			break;
		}
	}
	return g_audioPreviewProgram;
}

void AudioPreview_stop(){
	if ( g_audioPreviewProcess != nullptr ) {
		g_audioPreviewProcess->kill();
		g_audioPreviewProcess->deleteLater();
		g_audioPreviewProcess = nullptr;
	}
}

bool AudioPreview_playFile( const QString& filePath, int volumePercent ){
	AudioPreview_stop();
	const auto program = AudioPreview_findPlayerProgram();
	if ( program.isEmpty() ) {
		return false;
	}
	QStringList args;
	const auto exeName = QFileInfo( program ).fileName();
	if ( exeName == "ffplay" ) {
		args << "-nodisp" << "-autoexit" << "-loglevel" << "quiet" << "-volume" << QString::number( qBound( 0, volumePercent, 100 ) ) << filePath;
	}
	else if ( exeName == "mpv" ) {
		args << "--no-video" << "--really-quiet" << "--volume" << QString::number( qBound( 0, volumePercent, 100 ) ) << filePath;
	}
	else if ( exeName == "vlc" || exeName == "cvlc" ) {
		args << "--play-and-exit" << "--intf" << "dummy" << filePath;
	}
	else if ( exeName == "xdg-open" ) {
		args << filePath;
	}
	else{
		args << filePath;
	}
	g_audioPreviewProcess = new QProcess( MainFrame_getWindow() );
	g_audioPreviewProcess->setProgram( program );
	g_audioPreviewProcess->setArguments( args );
	g_audioPreviewProcess->start();
	return g_audioPreviewProcess->waitForStarted( 2000 );
}

void AudioPreview_openDialog(){
	QDialog dialog( MainFrame_getWindow() );
	dialog.setWindowTitle( "Audio Preview (MP3 / OGG / WAV)" );
	dialog.setModal( true );
	dialog.resize( 700, 180 );

	auto* root = new QVBoxLayout( &dialog );
	auto* form = new QFormLayout();
	auto* pathRow = new QHBoxLayout();
	auto* pathEdit = new QLineEdit( &dialog );
	pathEdit->setPlaceholderText( "Choose a sound file..." );
	auto* browseButton = new QPushButton( "Browse...", &dialog );
	pathRow->addWidget( pathEdit, 1 );
	pathRow->addWidget( browseButton );
	auto* pathWidget = new QWidget( &dialog );
	pathWidget->setLayout( pathRow );
	form->addRow( "File", pathWidget );

	auto* volumeSlider = new QSlider( Qt::Horizontal, &dialog );
	volumeSlider->setRange( 0, 100 );
	volumeSlider->setValue( 80 );
	form->addRow( "Volume", volumeSlider );
	root->addLayout( form );

	auto* buttons = new QDialogButtonBox( &dialog );
	auto* playButton = buttons->addButton( "Play", QDialogButtonBox::ActionRole );
	auto* stopButton = buttons->addButton( "Stop", QDialogButtonBox::ActionRole );
	auto* closeButton = buttons->addButton( QDialogButtonBox::Close );
	root->addWidget( buttons );

	QObject::connect( browseButton, &QPushButton::clicked, [&](){
		const auto startPath = pathEdit->text().isEmpty() ? QString::fromLatin1( EnginePath_get() ) : QFileInfo( pathEdit->text() ).absolutePath();
		const auto selected = QFileDialog::getOpenFileName( &dialog, "Select Audio File", startPath, "Audio Files (*.mp3 *.ogg *.wav *.flac);;All Files (*)" );
		if ( !selected.isEmpty() ) {
			pathEdit->setText( selected );
		}
	} );
	QObject::connect( playButton, &QPushButton::clicked, [&](){
		const auto file = pathEdit->text().trimmed();
		if ( file.isEmpty() ) {
			return;
		}
		if ( !QFileInfo::exists( file ) ) {
			QMessageBox::warning( &dialog, "Missing file", "Selected file does not exist." );
			return;
		}
		if ( !AudioPreview_playFile( file, volumeSlider->value() ) ) {
			QMessageBox::warning( &dialog, "No player found", "Install ffplay/mpv/vlc (or set xdg-open) to preview audio." );
		}
	} );
	QObject::connect( stopButton, &QPushButton::clicked, &dialog, [](){ AudioPreview_stop(); } );
	QObject::connect( closeButton, &QPushButton::clicked, &dialog, &QDialog::reject );
	QObject::connect( &dialog, &QDialog::rejected, [](){ AudioPreview_stop(); } );

	dialog.exec();
}

void Layout_setStyleAndRequestRestart( MainFrame::EViewStyle style, const char* name ){
	if ( g_Layout_viewStyle.m_latched == style ) {
		return;
	}

	g_Layout_viewStyle.import( style );
	Preferences_Save();

	const auto message = StringStream( name, " layout will apply after restart.\n\nRestart now?" );
	if ( QMessageBox::question( MainFrame_getWindow(), "Restart is required", message.c_str(), QMessageBox::Yes | QMessageBox::No, QMessageBox::No ) == QMessageBox::Yes ) {
		Radiant_Restart();
	}
}

void Layout_setHammerFourPane(){
	Layout_setStyleAndRequestRestart( MainFrame::eSplit, "Hammer++ 4-pane" );
}
}

void create_add_menu( QMenuBar *menubar ){
	QMenu *menu = menubar->addMenu( "&Add" );

	menu->setTearOffEnabled( g_Layout_enableDetachableMenus.m_value );

	create_menu_item_with_mnemonic( menu, "Entity...", "AddEntityByName" );
	menu->addSeparator();
	create_menu_item_with_mnemonic( menu, "Light", "AddLight" );
	create_menu_item_with_mnemonic( menu, "Player Start", "AddInfoPlayerStart" );
	create_menu_item_with_mnemonic( menu, "Player Deathmatch", "AddInfoPlayerDeathmatch" );
	create_menu_item_with_mnemonic( menu, "Model...", "AddMiscModel" );
	menu->addSeparator();

	QMenu* brushMenu = menu->addMenu( "Brush Primitive" );
	brushMenu->setTearOffEnabled( g_Layout_enableDetachableMenus.m_value );
	create_menu_item_with_mnemonic( brushMenu, "Prism...", "BrushPrism" );
	create_menu_item_with_mnemonic( brushMenu, "Cone...", "BrushCone" );
	create_menu_item_with_mnemonic( brushMenu, "Sphere...", "BrushSphere" );
}

void create_view_menu( QMenuBar *menubar, MainFrame::EViewStyle style ){
	// View menu
	QMenu *menu = menubar->addMenu( "Vie&w" );

	menu->setTearOffEnabled( g_Layout_enableDetachableMenus.m_value );

	if ( style == MainFrame::eFloating ) {
		create_check_menu_item_with_mnemonic( menu, "Camera View", "ToggleCamera" );
		create_check_menu_item_with_mnemonic( menu, "XY (Top) View", "ToggleView" );
		create_check_menu_item_with_mnemonic( menu, "XZ (Front) View", "ToggleFrontView" );
		create_check_menu_item_with_mnemonic( menu, "YZ (Side) View", "ToggleSideView" );
	}
	if ( style != MainFrame::eRegular && style != MainFrame::eRegularLeft ) {
		create_menu_item_with_mnemonic( menu, "Console", "ToggleConsole" );
	}
	create_menu_item_with_mnemonic( menu, "Switch to Hammer++ 4-pane layout", "LayoutHammerFourPane" );
	if ( ( style != MainFrame::eRegular && style != MainFrame::eRegularLeft ) || g_Layout_builtInGroupDialog.m_value ) {
		create_menu_item_with_mnemonic( menu, "Texture Browser", "ToggleTextures" );
	}
	create_menu_item_with_mnemonic( menu, "Model Browser", "ToggleModelBrowser" );
	create_menu_item_with_mnemonic( menu, "Entity Inspector", "ToggleEntityInspector" );
	create_menu_item_with_mnemonic( menu, "Layers Browser", "ToggleLayersBrowser" );
	create_menu_item_with_mnemonic( menu, "&Surface Inspector", "SurfaceInspector" );
	create_menu_item_with_mnemonic( menu, "Entity List", "ToggleEntityList" );
	if ( g_Layout_expiramentalFeatures.m_value ) {
		menu->addSeparator();
		create_menu_item_with_mnemonic( menu, "[NEW] Properties", "ToggleExperimentalProperties" );
		create_menu_item_with_mnemonic( menu, "[NEW] Preview", "ToggleExperimentalPreview" );
		create_menu_item_with_mnemonic( menu, "[NEW] Asset Library", "ToggleExperimentalAssets" );
		create_menu_item_with_mnemonic( menu, "[NEW] History", "ToggleExperimentalHistory" );
		create_menu_item_with_mnemonic( menu, "[NEW] USD Structure", "ToggleExperimentalUSD" );
	}

	menu->addSeparator();
	{
		QMenu* submenu = menu->addMenu( "Camera" );

		submenu->setTearOffEnabled( g_Layout_enableDetachableMenus.m_value );

		create_menu_item_with_mnemonic( submenu, "Focus on Selected", "CameraFocusOnSelected" );
		create_menu_item_with_mnemonic( submenu, "&Center", "CenterView" );
		create_menu_item_with_mnemonic( submenu, "&Up Floor", "UpFloor" );
		create_menu_item_with_mnemonic( submenu, "&Down Floor", "DownFloor" );
		submenu->addSeparator();
		create_menu_item_with_mnemonic( submenu, "Far Clip Plane In", "CubicClipZoomIn" );
		create_menu_item_with_mnemonic( submenu, "Far Clip Plane Out", "CubicClipZoomOut" );
		submenu->addSeparator();
		create_menu_item_with_mnemonic( submenu, "Next leak spot", "NextLeakSpot" );
		create_menu_item_with_mnemonic( submenu, "Previous leak spot", "PrevLeakSpot" );
		submenu->addSeparator();
		create_menu_item_with_mnemonic( submenu, "Store Bookmark 1", "CameraStoreBookmark1" );
		create_menu_item_with_mnemonic( submenu, "Store Bookmark 2", "CameraStoreBookmark2" );
		create_menu_item_with_mnemonic( submenu, "Store Bookmark 3", "CameraStoreBookmark3" );
		create_menu_item_with_mnemonic( submenu, "Store Bookmark 4", "CameraStoreBookmark4" );
		create_menu_item_with_mnemonic( submenu, "Store Bookmark 5", "CameraStoreBookmark5" );
		submenu->addSeparator();
		create_menu_item_with_mnemonic( submenu, "Recall Bookmark 1", "CameraRecallBookmark1" );
		create_menu_item_with_mnemonic( submenu, "Recall Bookmark 2", "CameraRecallBookmark2" );
		create_menu_item_with_mnemonic( submenu, "Recall Bookmark 3", "CameraRecallBookmark3" );
		create_menu_item_with_mnemonic( submenu, "Recall Bookmark 4", "CameraRecallBookmark4" );
		create_menu_item_with_mnemonic( submenu, "Recall Bookmark 5", "CameraRecallBookmark5" );
		//cameramodel is not implemented in instances, thus useless
//		submenu->addSeparator();
//		create_menu_item_with_mnemonic( submenu, "Look Through Selected", "LookThroughSelected" );
//		create_menu_item_with_mnemonic( submenu, "Look Through Camera", "LookThroughCamera" );
	}
	menu->addSeparator();
	{
		QMenu* submenu = menu->addMenu( "Orthographic" );

		submenu->setTearOffEnabled( g_Layout_enableDetachableMenus.m_value );

		if ( style == MainFrame::eRegular || style == MainFrame::eRegularLeft || style == MainFrame::eFloating ) {
			create_menu_item_with_mnemonic( submenu, "&Next (XY, XZ, YZ)", "NextView" );
			create_menu_item_with_mnemonic( submenu, "XY (Top)", "ViewTop" );
			create_menu_item_with_mnemonic( submenu, "XZ (Front)", "ViewFront" );
			create_menu_item_with_mnemonic( submenu, "YZ (Side)", "ViewSide" );
			submenu->addSeparator();
		}
		else{
			create_menu_item_with_mnemonic( submenu, "Center on Selected", "NextView" );
		}

		create_menu_item_with_mnemonic( submenu, "Focus on Selected", "XYFocusOnSelected" );
		create_menu_item_with_mnemonic( submenu, "Center on Selected", "CenterXYView" );
		submenu->addSeparator();
		create_menu_item_with_mnemonic( submenu, "&XY 100%", "Zoom100" );
		create_menu_item_with_mnemonic( submenu, "XY Zoom &In", "ZoomIn" );
		create_menu_item_with_mnemonic( submenu, "XY Zoom &Out", "ZoomOut" );
	}

	menu->addSeparator();

	{
		QMenu* submenu = menu->addMenu( "Show" );

		submenu->setTearOffEnabled( g_Layout_enableDetachableMenus.m_value );

		create_check_menu_item_with_mnemonic( submenu, "Show Entity &Angles", "ShowAngles" );
		create_check_menu_item_with_mnemonic( submenu, "Show Entity &Names", "ShowNames" );
		create_check_menu_item_with_mnemonic( submenu, "Show Light Radiuses", "ShowLightRadiuses" );
		create_check_menu_item_with_mnemonic( submenu, "Show Entity Boxes", "ShowBboxes" );
		create_check_menu_item_with_mnemonic( submenu, "Show Entity Connections", "ShowConnections" );

		submenu->addSeparator();

		create_check_menu_item_with_mnemonic( submenu, "Show 2D Size Info", "ShowSize2d" );
		create_check_menu_item_with_mnemonic( submenu, "Show 3D Size Info", "ShowSize3d" );
		create_check_menu_item_with_mnemonic( submenu, "Show Crosshair", "ToggleCrosshairs" );
		create_check_menu_item_with_mnemonic( submenu, "Show Grid", "ToggleGrid" );
		create_check_menu_item_with_mnemonic( submenu, "Show Blocks", "ShowBlocks" );
		create_check_menu_item_with_mnemonic( submenu, "Show C&oordinates", "ShowCoordinates" );
		create_check_menu_item_with_mnemonic( submenu, "Show Window Outline", "ShowWindowOutline" );
		create_check_menu_item_with_mnemonic( submenu, "Show Axes", "ShowAxes" );
		create_check_menu_item_with_mnemonic( submenu, "Show 2D Workzone", "ShowWorkzone2d" );
		create_check_menu_item_with_mnemonic( submenu, "Show 3D Workzone", "ShowWorkzone3d" );
		create_check_menu_item_with_mnemonic( submenu, "Show Renderer Stats", "ShowStats" );
	}

	{
		QMenu* submenu = menu->addMenu( "Filter" );

		submenu->setTearOffEnabled( g_Layout_enableDetachableMenus.m_value );

		Filters_constructMenu( submenu );
	}
	menu->addSeparator();
	{
		create_check_menu_item_with_mnemonic( menu, "Hide Selected", "HideSelected" );
		create_menu_item_with_mnemonic( menu, "Show Hidden", "ShowHidden" );
	}
	menu->addSeparator();
	{
		QMenu* submenu = menu->addMenu( "Region" );

		submenu->setTearOffEnabled( g_Layout_enableDetachableMenus.m_value );

		create_menu_item_with_mnemonic( submenu, "&Off", "RegionOff" );
		create_menu_item_with_mnemonic( submenu, "&Set XY", "RegionSetXY" );
		create_menu_item_with_mnemonic( submenu, "Set &Brush", "RegionSetBrush" );
		create_check_menu_item_with_mnemonic( submenu, "Set Se&lection", "RegionSetSelection" );
	}

	//command_connect_accelerator( "CenterXYView" );
}

void create_selection_menu( QMenuBar *menubar ){
	// Selection menu
	QMenu *menu = menubar->addMenu( "M&odify" );

	menu->setTearOffEnabled( g_Layout_enableDetachableMenus.m_value );
	{
		QMenu* submenu = menu->addMenu( "Components" );

		submenu->setTearOffEnabled( g_Layout_enableDetachableMenus.m_value );

		create_check_menu_item_with_mnemonic( submenu, "&Edges", "DragEdges" );
		create_check_menu_item_with_mnemonic( submenu, "&Vertices", "DragVertices" );
		create_check_menu_item_with_mnemonic( submenu, "&Faces", "DragFaces" );
	}

	menu->addSeparator();
	create_menu_item_with_mnemonic( menu, "Snap To Grid", "SnapToGrid" );

	menu->addSeparator();

	{
		QMenu* submenu = menu->addMenu( "Nudge" );

		submenu->setTearOffEnabled( g_Layout_enableDetachableMenus.m_value );

		create_menu_item_with_mnemonic( submenu, "Nudge Left", "SelectNudgeLeft" );
		create_menu_item_with_mnemonic( submenu, "Nudge Right", "SelectNudgeRight" );
		create_menu_item_with_mnemonic( submenu, "Nudge Up", "SelectNudgeUp" );
		create_menu_item_with_mnemonic( submenu, "Nudge Down", "SelectNudgeDown" );
		submenu->addSeparator();
		create_menu_item_with_mnemonic( submenu, "Nudge +Z", "MoveSelectionUP" );
		create_menu_item_with_mnemonic( submenu, "Nudge -Z", "MoveSelectionDOWN" );
	}
	{
		QMenu* submenu = menu->addMenu( "Rotate" );

		submenu->setTearOffEnabled( g_Layout_enableDetachableMenus.m_value );

		create_menu_item_with_mnemonic( submenu, "Rotate X", "RotateSelectionX" );
		create_menu_item_with_mnemonic( submenu, "Rotate Y", "RotateSelectionY" );
		create_menu_item_with_mnemonic( submenu, "Rotate Z", "RotateSelectionZ" );
		submenu->addSeparator();
		create_menu_item_with_mnemonic( submenu, "Rotate Clockwise", "RotateSelectionClockwise" );
		create_menu_item_with_mnemonic( submenu, "Rotate Anticlockwise", "RotateSelectionAnticlockwise" );
	}
	{
		QMenu* submenu = menu->addMenu( "Flip" );

		submenu->setTearOffEnabled( g_Layout_enableDetachableMenus.m_value );

		create_menu_item_with_mnemonic( submenu, "Flip &X", "MirrorSelectionX" );
		create_menu_item_with_mnemonic( submenu, "Flip &Y", "MirrorSelectionY" );
		create_menu_item_with_mnemonic( submenu, "Flip &Z", "MirrorSelectionZ" );
		submenu->addSeparator();
		create_menu_item_with_mnemonic( submenu, "Flip Horizontally", "MirrorSelectionHorizontally" );
		create_menu_item_with_mnemonic( submenu, "Flip Vertically", "MirrorSelectionVertically" );
	}
	menu->addSeparator();
	create_menu_item_with_mnemonic( menu, "Arbitrary rotation...", "ArbitraryRotation" );
	create_menu_item_with_mnemonic( menu, "Arbitrary scale...", "ArbitraryScale" );
	menu->addSeparator();
	{
		QMenu* submenu = menu->addMenu( "Repeat" );

		submenu->setTearOffEnabled( g_Layout_enableDetachableMenus.m_value );

		create_menu_item_with_mnemonic( submenu, "Repeat Transforms", "RepeatTransforms" );

		using SetTextCB = PointerCaller<QAction, void(const char*), +[]( QAction *action, const char *text ){ action->setText( text ); }>;
		const auto addItem = [submenu]<SelectionSystem::EManipulatorMode mode>() -> SetTextCB {
			return SetTextCB( create_menu_item_with_mnemonic( submenu, "", makeCallbackF( +[](){ GlobalSelectionSystem().resetTransforms( mode ); } ) ) );
		};
		SelectionSystem_connectTransformsCallbacks( { addItem.operator()<SelectionSystem::eTranslate>(),
		                                              addItem.operator()<SelectionSystem::eRotate>(),
		                                              addItem.operator()<SelectionSystem::eScale>(),
		                                              addItem.operator()<SelectionSystem::eSkew>() } );
		GlobalSelectionSystem().resetTransforms(); // init texts immediately

		create_menu_item_with_mnemonic( submenu, "Reset Transforms", "ResetTransforms" );
	}
}

void create_bsp_menu( QMenuBar *menubar ){
	// BSP menu
	QMenu *menu = menubar->addMenu( "&Build" );

	menu->setTearOffEnabled( g_Layout_enableDetachableMenus.m_value );

	create_menu_item_with_mnemonic( menu, "Customize...", "BuildMenuCustomize" );
	create_menu_item_with_mnemonic( menu, "Run recent build", "Build_runRecentExecutedBuild" );

	menu->addSeparator();

	menu->setToolTipsVisible( true );
	Build_constructMenu( menu );

	g_bsp_menu = menu;
}

void create_grid_menu( QMenuBar *menubar ){
	// Grid menu
	QMenu *menu = menubar->addMenu( "&Grid" );

	menu->setTearOffEnabled( g_Layout_enableDetachableMenus.m_value );

	Grid_constructMenu( menu );
}

void create_misc_menu( QMenuBar *menubar ){
	// Misc menu
	QMenu *menu = menubar->addMenu( "M&isc" );

	menu->setTearOffEnabled( g_Layout_enableDetachableMenus.m_value );
#if 0
	create_menu_item_with_mnemonic( menu, "&Benchmark", makeCallbackF( GlobalCamera_Benchmark ) );
#endif
	create_colours_menu( menu );

	create_menu_item_with_mnemonic( menu, "Find brush...", "FindBrush" );
	create_menu_item_with_mnemonic( menu, "Map Info...", "MapInfo" );
	create_menu_item_with_mnemonic( menu, "&Refresh models", "RefreshReferences" );
	if ( g_Layout_expiramentalFeatures.m_value ) {
		create_menu_item_with_mnemonic( menu, "Import USD structure...", "ImportUSDStructure" );
	}
	create_menu_item_with_mnemonic( menu, "Set 2D &Background image...", makeCallbackF( WXY_SetBackgroundImage ) );
	create_menu_item_with_mnemonic( menu, "Fullscreen", "Fullscreen" );
	create_menu_item_with_mnemonic( menu, "Maximize view", "MaximizeView" );
}

void create_entity_menu( QMenuBar *menubar ){
	// Entity menu
	QMenu *menu = menubar->addMenu( "E&ntity" );

	menu->setTearOffEnabled( g_Layout_enableDetachableMenus.m_value );

	Entity_constructMenu( menu );
}

void create_brush_menu( QMenuBar *menubar ){
	// Brush menu
	QMenu *menu = menubar->addMenu( "Brush" );

	menu->setTearOffEnabled( g_Layout_enableDetachableMenus.m_value );

	Brush_constructMenu( menu );
}

void create_patch_menu( QMenuBar *menubar ){
	// Curve menu
	QMenu *menu = menubar->addMenu( "&Curve" );

	menu->setTearOffEnabled( g_Layout_enableDetachableMenus.m_value );

	Patch_constructMenu( menu );
}

void create_tools_menu( QMenuBar *menubar ){
	QMenu *menu = menubar->addMenu( "&Tools" );

	menu->setTearOffEnabled( g_Layout_enableDetachableMenus.m_value );

	create_menu_item_with_mnemonic( menu, "Id Tech 3 Tool Center...", "OpenIdTech3ToolCenter" );
	create_menu_item_with_mnemonic( menu, "Audio Preview (MP3/OGG/WAV)...", "OpenAudioPreview" );
	menu->addSeparator();
	create_menu_item_with_mnemonic( menu, "Q3Map2++ Help", "ToolQ3Map2Help" );
	create_menu_item_with_mnemonic( menu, "QData3++ Help", "ToolQData3Help" );
	create_menu_item_with_mnemonic( menu, "Q2Map++ Help", "ToolQ2MapHelp" );
	create_menu_item_with_mnemonic( menu, "MBSPC++ Help", "ToolMBSPCHelp" );
}

void create_help_menu( QMenuBar *menubar ){
	// Help menu
	QMenu *menu = menubar->addMenu( "&Help" );

	menu->setTearOffEnabled( g_Layout_enableDetachableMenus.m_value );

//	create_menu_item_with_mnemonic( menu, "Manual", "OpenManual" );

	// this creates all the per-game drop downs for the game pack helps
	// it will take care of hooking the Sys_OpenURL calls etc.
	create_game_help_menu( menu );

	create_menu_item_with_mnemonic( menu, "Bug report", makeCallbackF( OpenBugReportURL ) );
	create_menu_item_with_mnemonic( menu, "Check for Radiant update", "CheckForUpdate" ); // FIXME
	create_menu_item_with_mnemonic( menu, "&About", makeCallbackF( DoAbout ) );
}

void create_main_menu( QMenuBar *menubar, MainFrame::EViewStyle style ){
	create_file_menu( menubar );
 	create_edit_menu( menubar );
	create_add_menu( menubar );
	create_view_menu( menubar, style );
	create_selection_menu( menubar );
	create_bsp_menu( menubar );
	create_grid_menu( menubar );
	create_misc_menu( menubar );
	create_entity_menu( menubar );
	create_brush_menu( menubar );
	if ( !string_equal( g_pGameDescription->getKeyValue( "no_patch" ), "1" ) )
		create_patch_menu( menubar );
	create_tools_menu( menubar );
	create_plugins_menu( menubar );
	create_help_menu( menubar );
}


void Patch_registerShortcuts(){
	command_connect_accelerator( "InvertCurveTextureX" );
	command_connect_accelerator( "InvertCurveTextureY" );
	command_connect_accelerator( "PatchInsertInsertColumn" );
	command_connect_accelerator( "PatchInsertInsertRow" );
	command_connect_accelerator( "PatchDeleteLastColumn" );
	command_connect_accelerator( "PatchDeleteLastRow" );
	command_connect_accelerator( "NaturalizePatch" );
}

void Manipulators_registerShortcuts(){
	command_connect_accelerator( "MouseRotateOrScale" );
	command_connect_accelerator( "MouseDragOrTransform" );
}

void TexdefNudge_registerShortcuts(){
	command_connect_accelerator( "TexRotateClock" );
	command_connect_accelerator( "TexRotateCounter" );
	command_connect_accelerator( "TexScaleUp" );
	command_connect_accelerator( "TexScaleDown" );
	command_connect_accelerator( "TexScaleLeft" );
	command_connect_accelerator( "TexScaleRight" );
	command_connect_accelerator( "TexShiftUp" );
	command_connect_accelerator( "TexShiftDown" );
	command_connect_accelerator( "TexShiftLeft" );
	command_connect_accelerator( "TexShiftRight" );
}

void SelectNudge_registerShortcuts(){
	command_connect_accelerator( "MoveSelectionDOWN" );
	command_connect_accelerator( "MoveSelectionUP" );
	command_connect_accelerator( "SelectNudgeLeft" );
	command_connect_accelerator( "SelectNudgeRight" );
	command_connect_accelerator( "SelectNudgeUp" );
	command_connect_accelerator( "SelectNudgeDown" );
}

void SnapToGrid_registerShortcuts(){
	command_connect_accelerator( "SnapToGrid" );
}

void SelectByType_registerShortcuts(){
	command_connect_accelerator( "SelectAllOfType" );
}

void SurfaceInspector_registerShortcuts(){
	command_connect_accelerator( "FitTexture" );
	command_connect_accelerator( "FitTextureWidth" );
	command_connect_accelerator( "FitTextureHeight" );
	command_connect_accelerator( "FitTextureWidthOnly" );
	command_connect_accelerator( "FitTextureHeightOnly" );
	command_connect_accelerator( "TextureProjectAxial" );
	command_connect_accelerator( "TextureProjectOrtho" );
	command_connect_accelerator( "TextureProjectCam" );
}

void TexBro_registerShortcuts(){
	toggle_add_accelerator( "SearchFromStart" );
}

void Misc_registerShortcuts(){
	command_connect_accelerator( "Redo2" );
	command_connect_accelerator( "UnSelectSelection2" );
	command_connect_accelerator( "DeleteSelection2" );
	command_connect_accelerator( "DeleteSelection3" );
}


void register_shortcuts(){
//	Patch_registerShortcuts();
	Grid_registerShortcuts();
//	XYWnd_registerShortcuts();
	CamWnd_registerShortcuts();
	Manipulators_registerShortcuts();
	SurfaceInspector_registerShortcuts();
	TexdefNudge_registerShortcuts();
//	SelectNudge_registerShortcuts();
//	SnapToGrid_registerShortcuts();
//	SelectByType_registerShortcuts();
	TexBro_registerShortcuts();
	Misc_registerShortcuts();
	Entity_registerShortcuts();
	Layers_registerShortcuts();
}

void File_constructToolbar( QToolBar* toolbar ){
	toolbar_append_button( toolbar, "Open an existing map", "file_open.png", "OpenMap" );
	toolbar_append_button( toolbar, "Save the active map", "file_save.png", "SaveMap" );
}

void UndoRedo_constructToolbar( QToolBar* toolbar ){
	toolbar_append_button( toolbar, "Undo", "undo.png", "Undo" );
	toolbar_append_button( toolbar, "Redo", "redo.png", "Redo" );
}

void RotateFlip_constructToolbar( QToolBar* toolbar ){
//	toolbar_append_button( toolbar, "x-axis Flip", "brush_flipx.png", "MirrorSelectionX" );
//	toolbar_append_button( toolbar, "x-axis Rotate", "brush_rotatex.png", "RotateSelectionX" );
//	toolbar_append_button( toolbar, "y-axis Flip", "brush_flipy.png", "MirrorSelectionY" );
//	toolbar_append_button( toolbar, "y-axis Rotate", "brush_rotatey.png", "RotateSelectionY" );
//	toolbar_append_button( toolbar, "z-axis Flip", "brush_flipz.png", "MirrorSelectionZ" );
//	toolbar_append_button( toolbar, "z-axis Rotate", "brush_rotatez.png", "RotateSelectionZ" );
	toolbar_append_button( toolbar, "Flip Horizontally", "brush_flip_hor.png", "MirrorSelectionHorizontally" );
	toolbar_append_button( toolbar, "Flip Vertically", "brush_flip_vert.png", "MirrorSelectionVertically" );

	toolbar_append_button( toolbar, "Rotate Anticlockwise", "brush_rotate_anti.png", "RotateSelectionAnticlockwise" );
	toolbar_append_button( toolbar, "Rotate Clockwise", "brush_rotate_clock.png", "RotateSelectionClockwise" );
}

void Select_constructToolbar( QToolBar* toolbar ){
	toolbar_append_button( toolbar, "Select touching", "selection_selecttouching.png", "SelectTouching" );
	toolbar_append_button( toolbar, "Select inside", "selection_selectinside.png", "SelectInside" );
}

void CSG_constructToolbar( QToolBar* toolbar ){
	toolbar_append_button( toolbar, "CSG Subtract", "selection_csgsubtract.png", "CSGSubtract" );
	toolbar_append_button( toolbar, "CSG Wrap Merge", "selection_csgmerge.png", "CSGWrapMerge" );
	toolbar_append_button( toolbar, "Room", "selection_makeroom.png", "CSGroom" );
	toolbar_append_button( toolbar, "CSG Tool", "ellipsis.png", "CSGTool" );
}

void ComponentModes_constructToolbar( QToolBar* toolbar ){
	toolbar_append_toggle_button( toolbar, "Select Vertices", "modify_vertices.png", "DragVertices" );
	toolbar_append_toggle_button( toolbar, "Select Edges", "modify_edges.png", "DragEdges" );
	toolbar_append_toggle_button( toolbar, "Select Faces", "modify_faces.png", "DragFaces" );
}

void XYWnd_constructToolbar( QToolBar* toolbar ){
	toolbar_append_button( toolbar, "Change views", "view_change.png", "NextView" );
}

void Manipulators_constructToolbar( QToolBar* toolbar ){
	toolbar_append_toggle_button( toolbar, "Resize (Q)", "select_mouseresize.png", "MouseDrag" ); // hardcoded shortcut tip of "MouseDragOrTransform"...
	toolbar_append_toggle_button( toolbar, "Clipper", "select_clipper.png", "ToggleClipper" );
	toolbar_append_toggle_button( toolbar, "Translate", "select_mousetranslate.png", "MouseTranslate" );
	toolbar_append_toggle_button( toolbar, "Rotate", "select_mouserotate.png", "MouseRotate" );
	toolbar_append_toggle_button( toolbar, "Scale", "select_mousescale.png", "MouseScale" );
	toolbar_append_toggle_button( toolbar, "Transform (Q)", "select_mousetransform.png", "MouseTransform" ); // hardcoded shortcut tip of "MouseDragOrTransform"...
//	toolbar_append_toggle_button( toolbar, "Build", "select_mouserotate.png", "MouseBuild" );
	toolbar_append_toggle_button( toolbar, "UV Tool", "select_mouseuv.png", "MouseUV" );
}

extern CopiedString g_toolbarHiddenButtons;

#include <QSvgGenerator>
void create_main_toolbar( QToolBar *toolbar,  MainFrame::EViewStyle style ){
	QSvgGenerator dummy; // reference symbol, so that Qt5Svg.dll required dependency is explicit, also install-dlls-msys2-mingw.sh will find it

 	File_constructToolbar( toolbar );
	toolbar_append_separator( toolbar );

	UndoRedo_constructToolbar( toolbar );
	toolbar_append_separator( toolbar );

	RotateFlip_constructToolbar( toolbar );
	toolbar_append_separator( toolbar );

	Select_constructToolbar( toolbar );
	toolbar_append_separator( toolbar );

	CSG_constructToolbar( toolbar );
	toolbar_append_separator( toolbar );

	ComponentModes_constructToolbar( toolbar );
	toolbar_append_separator( toolbar );

	if ( style != MainFrame::eSplit ) {
		XYWnd_constructToolbar( toolbar );
		toolbar_append_separator( toolbar );
	}

	CamWnd_constructToolbar( toolbar );
	toolbar_append_separator( toolbar );

	Manipulators_constructToolbar( toolbar );
	toolbar_append_separator( toolbar );

	if ( !string_equal( g_pGameDescription->getKeyValue( "no_patch" ), "1" ) ) {
		Patch_constructToolbar( toolbar );
		toolbar_append_separator( toolbar );
	}

	toolbar_append_toggle_button( toolbar, "Texture Lock", "texture_lock.png", "TogTexLock" );
	toolbar_append_toggle_button( toolbar, "Texture Vertex Lock", "texture_vertexlock.png", "TogTexVertexLock" );
	toolbar_append_separator( toolbar );

	toolbar_append_button( toolbar, "Entities", "entities.png", "ToggleEntityInspector" );
	// disable the console and texture button in the regular layouts
	if ( style != MainFrame::eRegular && style != MainFrame::eRegularLeft ) {
		toolbar_append_button( toolbar, "Console", "console.png", "ToggleConsole" );
	}
	if ( ( style != MainFrame::eRegular && style != MainFrame::eRegularLeft ) || g_Layout_builtInGroupDialog.m_value ) {
		toolbar_append_button( toolbar, "Texture Browser", "texture_browser.png", "ToggleTextures" );
	}

	// TODO: call light inspector
	//QAction* g_view_lightinspector_button = toolbar_append_button( toolbar, "Light Inspector", "lightinspector.png", "ToggleLightInspector" );

	toolbar_append_separator( toolbar );
	toolbar_append_button( toolbar, "Refresh Models", "refresh_models.png", "RefreshReferences" );
}


void create_main_statusbar( QStatusBar *statusbar, QLabel *pStatusLabel[c_status__count] ){
	statusbar->setSizeGripEnabled( false );
	{
		auto *label = new QLabel;
		statusbar->addPermanentWidget( label, 1 );
		pStatusLabel[c_status_command] = label;
	}

	for ( int i = 1; i < c_status__count; ++i )
	{
		if( i == c_status_brushcount ){
			auto *widget = new QWidget;
			auto *hbox = new QHBoxLayout( widget );
			hbox->setMargin( 0 );
			statusbar->addPermanentWidget( widget, 0 );
			const char* imgs[3] = { "status_brush.png", "status_patch.png", "status_entity.png" };
			for( ; i < c_status_brushcount + 3; ++i ){
				auto *label = new QLabel();
				auto pixmap = new_local_image( imgs[i - c_status_brushcount] );
				pixmap.setDevicePixelRatio( label->devicePixelRatio() );
				label->setPixmap( pixmap.scaledToHeight( 16 * label->devicePixelRatio() * label->logicalDpiX() / 96, Qt::TransformationMode::SmoothTransformation ) );
				hbox->addWidget( label );

				label = new QLabel();
				label->setMinimumWidth( label->fontMetrics().horizontalAdvance( "99999" ) );
				hbox->addWidget( label );
				pStatusLabel[i] = label;
			}
			--i;
		}
		else{
			auto *label = new QLabel;
			if( i == c_status_grid ){
				statusbar->addPermanentWidget( label, 0 );
				label->setToolTip( " <b>G</b>: <u>G</u>rid size<br> <b>F</b>: map <u>F</u>ormat<br> <b>C</b>: camera <u>C</u>lip distance <br> <b>L</b>: texture <u>L</u>ock" );
			}
			else
				statusbar->addPermanentWidget( label, 1 );
			pStatusLabel[i] = label;
		}
	}
}

SignalHandlerId XYWindowDestroyed_connect( const SignalHandler& handler ){
	return g_pParentWnd->GetXYWnd()->onDestroyed.connectFirst( handler );
}

void XYWindowDestroyed_disconnect( SignalHandlerId id ){
	g_pParentWnd->GetXYWnd()->onDestroyed.disconnect( id );
}

MouseEventHandlerId XYWindowMouseDown_connect( const MouseEventHandler& handler ){
	return g_pParentWnd->GetXYWnd()->onMouseDown.connectFirst( handler );
}

void XYWindowMouseDown_disconnect( MouseEventHandlerId id ){
	g_pParentWnd->GetXYWnd()->onMouseDown.disconnect( id );
}

// =============================================================================
// MainFrame class

MainFrame* g_pParentWnd = 0;

QWidget* MainFrame_getWindow(){
	return g_pParentWnd == 0? 0 : g_pParentWnd->m_window;
}

MainFrame::MainFrame() : m_idleRedrawStatusText( RedrawStatusTextCaller( *this ) ){
	Create();
}

MainFrame::~MainFrame(){
	SaveGuiState();

	m_window->hide(); // hide to avoid resize events during content deletion

	Shutdown();

	delete m_window;
}

void MainFrame::SetActiveXY( XYWnd* p ){
	if ( m_pActiveXY ) {
		m_pActiveXY->SetActive( false );
	}

	m_pActiveXY = p;

	if ( m_pActiveXY ) {
		m_pActiveXY->SetActive( true );
	}
}

#ifdef WIN32
#include <QtPlatformHeaders/QWindowsWindowFunctions>
#endif
void MainFrame_toggleFullscreen(){
	QWidget *w = MainFrame_getWindow();
#ifdef WIN32 // https://doc.qt.io/qt-5.15/windows-issues.html#fullscreen-opengl-based-windows
	QWindowsWindowFunctions::setHasBorderInFullScreen( w->windowHandle(), true );
#endif
	w->setWindowState( w->windowState() ^ Qt::WindowState::WindowFullScreen );
}

class MaximizeView
{
	bool m_maximized{};
	QList<int> m_vSplitSizes;
	QList<int> m_vSplit2Sizes;
	QList<int> m_hSplitSizes;

	void maximize(){
		m_maximized = true;
		m_vSplitSizes = g_pParentWnd->m_vSplit->sizes();
		m_vSplit2Sizes = g_pParentWnd->m_vSplit2->sizes();
		m_hSplitSizes = g_pParentWnd->m_hSplit->sizes();

		const QPoint cursor = g_pParentWnd->m_hSplit->mapFromGlobal( QCursor::pos() );

		if( cursor.y() < m_vSplitSizes[0] )
			g_pParentWnd->m_vSplit->setSizes( { 9999, 0 } );
		else
			g_pParentWnd->m_vSplit->setSizes( { 0, 9999 } );

		if( cursor.y() < m_vSplit2Sizes[0] )
			g_pParentWnd->m_vSplit2->setSizes( { 9999, 0 } );
		else
			g_pParentWnd->m_vSplit2->setSizes( { 0, 9999 } );

		if( cursor.x() < m_hSplitSizes[0] )
			g_pParentWnd->m_hSplit->setSizes( { 9999, 0 } );
		else
			g_pParentWnd->m_hSplit->setSizes( { 0, 9999 } );
	}
public:
	void unmaximize(){
		if( m_maximized ){
			m_maximized = false;
			g_pParentWnd->m_vSplit->setSizes( m_vSplitSizes );
			g_pParentWnd->m_vSplit2->setSizes( m_vSplit2Sizes );
			g_pParentWnd->m_hSplit->setSizes( m_hSplitSizes );
		}
	}
	void toggle(){
		m_maximized ? unmaximize() : maximize();
	}
};

MaximizeView g_maximizeview;

void Maximize_View(){
	if( g_pParentWnd != 0 && g_pParentWnd->m_vSplit != 0 && g_pParentWnd->m_vSplit2 != 0 && g_pParentWnd->m_hSplit != 0 )
		g_maximizeview.toggle();
}



class RadiantQMainWindow : public QMainWindow
{
protected:
	void closeEvent( QCloseEvent *event ) override {
		event->ignore();
		Exit();
	}
	bool event( QEvent *event ) override {
		if( event->type() == QEvent::ShortcutOverride && !QGuiApplication::mouseButtons().testFlag( Qt::MouseButton::NoButton ) ){
			event->accept(); // block shortcuts while mouse buttons are pressed
		}
		return QMainWindow::event( event );
	}
public:
	QMenu* createPopupMenu() override {
		auto *menu = QMainWindow::createPopupMenu();
		if( menu == nullptr )
			menu = new QMenu;
		else
			menu->addSeparator();
		toolbar_construct_control_menu( menu );
		return menu;
	}
};


QSplashScreen *create_splash(){
	auto *splash = new QSplashScreen( new_local_image( "splash.png" ) );
	splash->show();
	return splash;
}

static QSplashScreen *splash_screen = 0;

void show_splash(){
	splash_screen = create_splash();

	process_gui();
}

void hide_splash(){
//.	splash_screen->finish();
	delete splash_screen;
}


void user_shortcuts_init(){
	const auto path = StringStream( SettingsPath_get(), g_pGameDescription->mGameFile, '/' );
	LoadCommandMap( path );
	SaveCommandMap( path );
}

void user_shortcuts_save(){
	const auto path = StringStream( SettingsPath_get(), g_pGameDescription->mGameFile, '/' );
	SaveCommandMap( path );
}


void MainFrame::Create(){
	QMainWindow *window = m_window = new RadiantQMainWindow();

	GlobalWindowObservers_connectTopLevel( window );

	/* GlobalCommands_insert plugins commands */
	GetPlugInMgr().Init( window );
	/* then load shortcuts cfg */
	user_shortcuts_init();

	GlobalPressedKeys_connect( window );
	GlobalShortcuts_setWidget( window );
	register_shortcuts();

	m_nCurrentStyle = (EViewStyle)g_Layout_viewStyle.m_value;

	create_main_menu( window->menuBar(), CurrentStyle() );

	{
		{
			auto *toolbar = new QToolBar( "Main Toolbar" );
			toolbar->setObjectName( "Main_Toolbar" ); // required for proper state save/restore
			window->addToolBar( Qt::ToolBarArea::TopToolBarArea, toolbar );
			create_main_toolbar( toolbar, CurrentStyle() );
		}
		{
			auto *toolbar = new QToolBar( "Filter Toolbar" );
			toolbar->setObjectName( "Filter_Toolbar" ); // required for proper state save/restore
			window->addToolBar( Qt::ToolBarArea::RightToolBarArea, toolbar );
			create_filter_toolbar( toolbar );
		}
		{
			auto *toolbar = new QToolBar( "Plugin Toolbar" );
			toolbar->setObjectName( "Plugin_Toolbar" ); // required for proper state save/restore
			window->addToolBar( Qt::ToolBarArea::RightToolBarArea, toolbar );
			create_plugin_toolbar( toolbar );
		}
	}

	create_main_statusbar( window->statusBar(), m_statusLabel );

	GroupDialog_constructWindow( window );

	g_page_entity = GroupDialog_addPage( "Entities", EntityInspector_constructWindow( GroupDialog_getWindow() ), RawStringExportCaller( "Entities" ) );

	if ( FloatingGroupDialog() ) {
		g_page_console = GroupDialog_addPage( "Console", Console_constructWindow(), RawStringExportCaller( "Console" ) );
		g_page_textures = GroupDialog_addPage( "Textures", TextureBrowser_constructWindow( GroupDialog_getWindow() ), TextureBrowserExportTitleCaller() );
	}

	g_page_models = GroupDialog_addPage( "Models", ModelBrowser_constructWindow( GroupDialog_getWindow() ), RawStringExportCaller( "Models" ) );

	g_page_layers = GroupDialog_addPage( "Layers", LayersBrowser_constructWindow( GroupDialog_getWindow() ), RawStringExportCaller( "Layers" ) );

	window->show();

	if ( CurrentStyle() == eRegular || CurrentStyle() == eRegularLeft ) {
		window->setCentralWidget( m_hSplit = new QSplitter() );
		{
			m_vSplit = new QSplitter( Qt::Vertical );
			m_vSplit2 = new QSplitter( Qt::Vertical );
			if ( CurrentStyle() == eRegular ){
				m_hSplit->addWidget( m_vSplit );
				m_hSplit->addWidget( m_vSplit2 );
			}
			else{
				m_hSplit->addWidget( m_vSplit2 );
				m_hSplit->addWidget( m_vSplit );
			}
			// console
			m_vSplit->addWidget( Console_constructWindow() );

			// xy
			m_pXYWnd = new XYWnd();
			m_pXYWnd->SetViewType( XY );
			m_vSplit->insertWidget( 0, m_pXYWnd->GetWidget() );
			{
				// camera
				m_pCamWnd = NewCamWnd();
				GlobalCamera_setCamWnd( *m_pCamWnd );
				CamWnd_setParent( *m_pCamWnd, window );
				m_vSplit2->addWidget( CamWnd_getWidget( *m_pCamWnd ) );

				// textures
				if( g_Layout_builtInGroupDialog.m_value )
					g_page_textures = GroupDialog_addPage( "Textures", TextureBrowser_constructWindow( GroupDialog_getWindow() ), TextureBrowserExportTitleCaller() );
				else
					m_vSplit2->addWidget( TextureBrowser_constructWindow( window ) );
			}
		}
	}
	else if ( CurrentStyle() == eFloating ) {
		{
			auto *window = new QWidget( m_window, Qt::Dialog | Qt::WindowMinimizeButtonHint | Qt::WindowMaximizeButtonHint | Qt::WindowCloseButtonHint );
			window->setWindowTitle( "Camera" );
			g_guiSettings.addWindow( window, "floating/cam", 400, 300, 50, 100 );

			m_pCamWnd = NewCamWnd();
			GlobalCamera_setCamWnd( *m_pCamWnd );

			{
				auto *box = new QHBoxLayout( window );
				box->setContentsMargins( 1, 1, 1, 1 );
				box->addWidget( CamWnd_getWidget( *m_pCamWnd ) );
			}

			CamWnd_setParent( *m_pCamWnd, window );
			GlobalPressedKeys_connect( window );
			GlobalWindowObservers_connectTopLevel( window );
			CamWnd_Shown_Construct( window );
		}

		{
			auto *window = new QWidget( m_window, Qt::Dialog | Qt::WindowMinimizeButtonHint | Qt::WindowMaximizeButtonHint | Qt::WindowCloseButtonHint );
			g_guiSettings.addWindow( window, "floating/xy", 400, 300, 500, 100 );

			m_pXYWnd = new XYWnd();
			m_pXYWnd->m_parent = window;
			m_pXYWnd->SetViewType( XY );

			{
				auto *box = new QHBoxLayout( window );
				box->setContentsMargins( 1, 1, 1, 1 );
				box->addWidget( m_pXYWnd->GetWidget() );
			}

			GlobalWindowObservers_connectTopLevel( window );
			XY_Top_Shown_Construct( window );
		}

		{
			auto *window = new QWidget( m_window, Qt::Dialog | Qt::WindowMinimizeButtonHint | Qt::WindowMaximizeButtonHint | Qt::WindowCloseButtonHint );
			g_guiSettings.addWindow( window, "floating/xz", 400, 300, 500, 450 );

			m_pXZWnd = new XYWnd();
			m_pXZWnd->m_parent = window;
			m_pXZWnd->SetViewType( XZ );

			{
				auto *box = new QHBoxLayout( window );
				box->setContentsMargins( 1, 1, 1, 1 );
				box->addWidget( m_pXZWnd->GetWidget() );
			}

			GlobalWindowObservers_connectTopLevel( window );
			XZ_Front_Shown_Construct( window );
		}

		{
			auto *window = new QWidget( m_window, Qt::Dialog | Qt::WindowMinimizeButtonHint | Qt::WindowMaximizeButtonHint | Qt::WindowCloseButtonHint );
			g_guiSettings.addWindow( window, "floating/yz", 400, 300, 50, 450 );

			m_pYZWnd = new XYWnd();
			m_pYZWnd->m_parent = window;
			m_pYZWnd->SetViewType( YZ );

			{
				auto *box = new QHBoxLayout( window );
				box->setContentsMargins( 1, 1, 1, 1 );
				box->addWidget( m_pYZWnd->GetWidget() );
			}

			GlobalWindowObservers_connectTopLevel( window );
			YZ_Side_Shown_Construct( window );
		}

		GroupDialog_show();
	}
	else // 4 way
	{
		window->setCentralWidget( m_hSplit = new QSplitter() );
		m_hSplit->addWidget( m_vSplit = new QSplitter( Qt::Vertical ) );
		m_hSplit->addWidget( m_vSplit2 = new QSplitter( Qt::Vertical ) );

		m_pCamWnd = NewCamWnd();
		GlobalCamera_setCamWnd( *m_pCamWnd );
		CamWnd_setParent( *m_pCamWnd, window );

		m_vSplit->addWidget( CamWnd_getWidget( *m_pCamWnd ) );

		m_pXZWnd = new XYWnd();
		m_pXZWnd->SetViewType( XZ );

		m_vSplit->addWidget( m_pXZWnd->GetWidget() );

		m_pXYWnd = new XYWnd();
		m_pXYWnd->SetViewType( XY );

		m_vSplit2->addWidget( m_pXYWnd->GetWidget() );

		m_pYZWnd = new XYWnd();
		m_pYZWnd->SetViewType( YZ );

		m_vSplit2->addWidget( m_pYZWnd->GetWidget() );
	}

	if( g_Layout_builtInGroupDialog.m_value && CurrentStyle() != eFloating ){
		m_hSplit->addWidget( GroupDialog_getWindow() );
		m_hSplit->setStretchFactor( 0, 2222 ); // set relative splitter sizes for eSplit (no sizes are restored)
		m_hSplit->setStretchFactor( 1, 2222 );
		m_hSplit->setStretchFactor( 2, 0 );
	}
	else{ // floating group dialog
		GlobalWindowObservers_connectTopLevel( GroupDialog_getWindow() ); // for layers browser icons toggle
	}

	EntityList_constructWindow( window );
	PreferencesDialog_constructWindow( window );
	FindTextureDialog_constructWindow( window );
	SurfaceInspector_constructWindow( window );

	SetActiveXY( m_pXYWnd );

	AddGridChangeCallback( SetGridStatusCaller( *this ) );
	AddGridChangeCallback( FreeCaller<void(), XY_UpdateAllWindows>() );

	Experimental_createDocks( window );

	s_qe_every_second_timer.enable();

	toolbar_importState( g_toolbarHiddenButtons.c_str() );
	RestoreGuiState();

	//GlobalShortcuts_reportUnregistered();
}

void MainFrame::SaveGuiState(){
	//restore good state first
	g_maximizeview.unmaximize();

	g_guiSettings.save();
}

void MainFrame::RestoreGuiState(){
	g_guiSettings.addWindow( m_window, "MainFrame/geometry", 962, 480 );
	g_guiSettings.addMainWindow( m_window, "MainFrame/state" );

	if( !FloatingGroupDialog() && m_hSplit != nullptr && m_vSplit != nullptr && m_vSplit2 != nullptr ){
		g_guiSettings.addSplitter( m_hSplit, "MainFrame/m_hSplit", { 384, 576 } );
		g_guiSettings.addSplitter( m_vSplit, "MainFrame/m_vSplit", CurrentStyle() == eSplit ? QList<int>{ 250, 250 } : QList<int>{ 377, 20 } );
		g_guiSettings.addSplitter( m_vSplit2, "MainFrame/m_vSplit2", CurrentStyle() == eSplit ? QList<int>{ 250, 250 } : QList<int>{ 250, 150 } );
	}
}

void MainFrame::Shutdown(){
	s_qe_every_second_timer.disable();

	EntityList_destroyWindow();

	delete std::exchange( m_pXYWnd, nullptr );
	delete std::exchange( m_pYZWnd, nullptr );
	delete std::exchange( m_pXZWnd, nullptr );

	ModelBrowser_destroyWindow();
	LayersBrowser_destroyWindow();
	TextureBrowser_destroyWindow();

	DeleteCamWnd( m_pCamWnd );
	m_pCamWnd = 0;

	PreferencesDialog_destroyWindow();
	SurfaceInspector_destroyWindow();
	FindTextureDialog_destroyWindow();

	g_DbgDlg.destroyWindow();

	// destroying group-dialog last because it may contain texture-browser
	GroupDialog_destroyWindow();

	Experimental_destroyDocks();
	AudioPreview_stop();

	user_shortcuts_save();
}

void MainFrame::RedrawStatusText(){
	for( int i = 0; i < c_status__count; ++i )
		m_statusLabel[i]->setText( m_status[i].c_str() );
}

void MainFrame::UpdateStatusText(){
	m_idleRedrawStatusText.queueDraw();
}

void MainFrame::SetStatusText( int status_n, const char* status ){
	m_status[status_n] = status;
	UpdateStatusText();
}

void Sys_Status( const char* status ){
	if ( g_pParentWnd )
		g_pParentWnd->SetStatusText( c_status_command, status );
}

void brushCountChanged( const Selectable& selectable ){
	QE_brushCountChanged();
}

//int getRotateIncrement(){
//	return static_cast<int>( g_si_globals.rotate );
//}

int getFarClipDistance(){
	return g_camwindow_globals.m_nCubicScale;
}

float ( *GridStatus_getGridSize )() = GetGridSize;
//int ( *GridStatus_getRotateIncrement )() = getRotateIncrement;
int ( *GridStatus_getFarClipDistance )() = getFarClipDistance;
bool ( *GridStatus_getTextureLockEnabled )();
const char* ( *GridStatus_getTexdefTypeIdLabel )();

void MainFrame::SetGridStatus(){
	StringOutputStream status( 64 );
	const char* lock = ( GridStatus_getTextureLockEnabled() ) ? "ON   " : "OFF  ";
	status << ( GetSnapGridSize() > 0 ? "G:" : "g:" ) << GridStatus_getGridSize()
	       << "  F:" << GridStatus_getTexdefTypeIdLabel()
	       << "  C:" << GridStatus_getFarClipDistance()
	       << "  L:" << lock;
	SetStatusText( c_status_grid, status );
}

void GridStatus_changed(){
	if ( g_pParentWnd != 0 ) {
		g_pParentWnd->SetGridStatus();
	}
}

CopiedString g_OpenGLFont( "Myriad Pro" );
int g_OpenGLFontSize = 8;

void OpenGLFont_select(){
	CopiedString newfont;
	int newsize;
	if( OpenGLFont_dialog( MainFrame_getWindow(), g_OpenGLFont.c_str(), g_OpenGLFontSize, newfont, newsize ) ){
		{
			ScopeDisableScreenUpdates disableScreenUpdates( "Processing...", "Changing OpenGL Font" );
			delete GlobalOpenGL().m_font;
			g_OpenGLFont = newfont;
			g_OpenGLFontSize = newsize;
			GlobalOpenGL().m_font = glfont_create( g_OpenGLFont.c_str(), g_OpenGLFontSize, g_strAppPath.c_str() );
		}
		UpdateAllWindows();
	}
}


void GlobalGL_sharedContextCreated(){
	// report OpenGL information
	globalOutputStream() << "GL_VENDOR: " << reinterpret_cast<const char*>( gl().glGetString( GL_VENDOR ) ) << '\n';
	globalOutputStream() << "GL_RENDERER: " << reinterpret_cast<const char*>( gl().glGetString( GL_RENDERER ) ) << '\n';
	globalOutputStream() << "GL_VERSION: " << reinterpret_cast<const char*>( gl().glGetString( GL_VERSION ) ) << '\n';
	globalOutputStream() << "GL_EXTENSIONS: " << reinterpret_cast<const char*>( gl().glGetString( GL_EXTENSIONS ) ) << '\n';

	QGL_sharedContextCreated( GlobalOpenGL() );

	ShaderCache_extensionsInitialised();

	GlobalShaderCache().realise();
	Textures_Realise();

	GlobalOpenGL().m_font = glfont_create( g_OpenGLFont.c_str(), g_OpenGLFontSize, g_strAppPath.c_str() );
}

void GlobalGL_sharedContextDestroyed(){
	Textures_Unrealise();
	GlobalShaderCache().unrealise();

	QGL_sharedContextDestroyed( GlobalOpenGL() );
}


void Layout_constructPreferences( PreferencesPage& page ){
	{
		const char* layouts[] = { "window1.png", "window2.png", "window3.png", "window4.png" };
		page.appendRadioIcons(
		    "Window Layout",
		    StringArrayRange( layouts ),
		    LatchedImportCaller( g_Layout_viewStyle ),
		    IntExportCaller( g_Layout_viewStyle.m_latched )
		);
	}
	page.appendCheckBox(
	    "", "Detachable Menus",
	    LatchedImportCaller( g_Layout_enableDetachableMenus ),
	    BoolExportCaller( g_Layout_enableDetachableMenus.m_latched )
	);
	page.appendCheckBox(
	    "", "Built-In Group Dialog",
	    LatchedImportCaller( g_Layout_builtInGroupDialog ),
	    BoolExportCaller( g_Layout_builtInGroupDialog.m_latched )
	);
	page.appendCheckBox(
	    "", "Expiramental Features",
	    LatchedImportCaller( g_Layout_expiramentalFeatures ),
	    BoolExportCaller( g_Layout_expiramentalFeatures.m_latched )
	);
}

void Layout_constructPage( PreferenceGroup& group ){
	PreferencesPage page( group.createPage( "Layout", "Layout Preferences" ) );
	Layout_constructPreferences( page );
}

void Layout_registerPreferencesPage(){
	PreferencesDialog_addInterfacePage( makeCallbackF( Layout_constructPage ) );
}


void FocusAllViews(){
	XY_Centralize(); //using centralizing here, not focusing function
	GlobalCamera_FocusOnSelected();
}

#include "preferencesystem.h"
#include "stringio.h"

void MainFrame_Construct(){
	GlobalCommands_insert( "OpenManual", makeCallbackF( OpenHelpURL ), QKeySequence( "F1" ) );

	GlobalCommands_insert( "RefreshReferences", makeCallbackF( RefreshReferences ) );
	GlobalCommands_insert( "CheckForUpdate", makeCallbackF( OpenUpdateURL ) );
	GlobalCommands_insert( "Exit", makeCallbackF( Exit ) );
	GlobalCommands_insert( "AddEntityByName", makeCallbackF( Add_openEntityDialog ) );
	GlobalCommands_insert( "AddLight", makeCallbackF( Add_createLight ) );
	GlobalCommands_insert( "AddInfoPlayerStart", makeCallbackF( Add_createInfoPlayerStart ) );
	GlobalCommands_insert( "AddInfoPlayerDeathmatch", makeCallbackF( Add_createInfoPlayerDeathmatch ) );
	GlobalCommands_insert( "AddMiscModel", makeCallbackF( Add_createMiscModel ) );
	GlobalCommands_insert( "LayoutHammerFourPane", makeCallbackF( Layout_setHammerFourPane ) );
	GlobalCommands_insert( "OpenIdTech3ToolCenter", makeCallbackF( IdTech3Tool_openHubDialog ) );
	GlobalCommands_insert( "OpenAudioPreview", makeCallbackF( AudioPreview_openDialog ) );
	GlobalCommands_insert( "ToolQ3Map2Help", makeCallbackF( +[](){ IdTech3Tool_runHelp( g_idTech3Tools[0] ); } ) );
	GlobalCommands_insert( "ToolQData3Help", makeCallbackF( +[](){ IdTech3Tool_runHelp( g_idTech3Tools[1] ); } ) );
	GlobalCommands_insert( "ToolQ2MapHelp", makeCallbackF( +[](){ IdTech3Tool_runHelp( g_idTech3Tools[2] ); } ) );
	GlobalCommands_insert( "ToolMBSPCHelp", makeCallbackF( +[](){ IdTech3Tool_runHelp( g_idTech3Tools[3] ); } ) );
	GlobalCommands_insert( "CameraStoreBookmark1", makeCallbackF( +[](){ CameraBookmark_store( 0 ); } ), QKeySequence( "Ctrl+1" ) );
	GlobalCommands_insert( "CameraStoreBookmark2", makeCallbackF( +[](){ CameraBookmark_store( 1 ); } ), QKeySequence( "Ctrl+2" ) );
	GlobalCommands_insert( "CameraStoreBookmark3", makeCallbackF( +[](){ CameraBookmark_store( 2 ); } ), QKeySequence( "Ctrl+3" ) );
	GlobalCommands_insert( "CameraStoreBookmark4", makeCallbackF( +[](){ CameraBookmark_store( 3 ); } ), QKeySequence( "Ctrl+4" ) );
	GlobalCommands_insert( "CameraStoreBookmark5", makeCallbackF( +[](){ CameraBookmark_store( 4 ); } ), QKeySequence( "Ctrl+5" ) );
	GlobalCommands_insert( "CameraRecallBookmark1", makeCallbackF( +[](){ CameraBookmark_recall( 0 ); } ), QKeySequence( "Shift+1" ) );
	GlobalCommands_insert( "CameraRecallBookmark2", makeCallbackF( +[](){ CameraBookmark_recall( 1 ); } ), QKeySequence( "Shift+2" ) );
	GlobalCommands_insert( "CameraRecallBookmark3", makeCallbackF( +[](){ CameraBookmark_recall( 2 ); } ), QKeySequence( "Shift+3" ) );
	GlobalCommands_insert( "CameraRecallBookmark4", makeCallbackF( +[](){ CameraBookmark_recall( 3 ); } ), QKeySequence( "Shift+4" ) );
	GlobalCommands_insert( "CameraRecallBookmark5", makeCallbackF( +[](){ CameraBookmark_recall( 4 ); } ), QKeySequence( "Shift+5" ) );

	GlobalCommands_insert( "Shortcuts", makeCallbackF( DoCommandListDlg ),
	                       g_Layout_expiramentalFeatures.m_value ? QKeySequence( "Ctrl+Alt+P" ) : QKeySequence( "Ctrl+Shift+P" ) );
	GlobalCommands_insert( "Preferences", makeCallbackF( PreferencesDialog_showDialog ), QKeySequence( "P" ) );
	if( g_Layout_expiramentalFeatures.m_value ){
		GlobalCommands_insert( "FrameSelection", makeCallbackF( FocusAllViews ), QKeySequence( "F" ) );
	}

	GlobalCommands_insert( "ToggleConsole", makeCallbackF( Console_ToggleShow ), QKeySequence( "O" ) );
	GlobalCommands_insert( "ToggleEntityInspector", makeCallbackF( EntityInspector_ToggleShow ), QKeySequence( "N" ) );
	GlobalCommands_insert( "ToggleModelBrowser", makeCallbackF( ModelBrowser_ToggleShow ), QKeySequence( "/" ) );
	GlobalCommands_insert( "ToggleLayersBrowser", makeCallbackF( LayersBrowser_ToggleShow ), QKeySequence( "L" ) );
	GlobalCommands_insert( "ToggleEntityList", makeCallbackF( EntityList_toggleShown ), QKeySequence( "Shift+L" ) );
	GlobalCommands_insert( "ToggleExperimentalProperties", makeCallbackF( Experimental_togglePropertiesDock ) );
	GlobalCommands_insert( "ToggleExperimentalPreview", makeCallbackF( Experimental_togglePreviewDock ) );
	GlobalCommands_insert( "ToggleExperimentalAssets", makeCallbackF( Experimental_toggleAssetsDock ) );
	GlobalCommands_insert( "ToggleExperimentalHistory", makeCallbackF( Experimental_toggleHistoryDock ) );
	GlobalCommands_insert( "ToggleExperimentalUSD", makeCallbackF( Experimental_toggleUSDDock ) );
	GlobalCommands_insert( "ImportUSDStructure", makeCallbackF( Experimental_importUSDStructure ) );

	Select_registerCommands();
	Layers_registerCommands();

	Tools_registerCommands();

	GlobalCommands_insert( "BuildMenuCustomize", makeCallbackF( DoBuildMenu ),
	                       g_Layout_expiramentalFeatures.m_value ? QKeySequence( "Ctrl+Shift+P" ) : QKeySequence() );
	GlobalCommands_insert( "Build_runRecentExecutedBuild", makeCallbackF( Build_runRecentExecutedBuild ), QKeySequence( "F5" ) );
	if( g_Layout_expiramentalFeatures.m_value ){
		GlobalCommands_insert( "Build_runRecentExecutedBuildCtrlP", makeCallbackF( Build_runRecentExecutedBuild ), QKeySequence( "Ctrl+P" ) );
		GlobalCommands_insert( "Build_runRecentExecutedBuildCtrlR", makeCallbackF( Build_runRecentExecutedBuild ), QKeySequence( "Ctrl+R" ) );
	}

	GlobalCommands_insert( "OpenGLFont", makeCallbackF( OpenGLFont_select ) );

	Colors_registerCommands();

	GlobalCommands_insert( "Fullscreen", makeCallbackF( MainFrame_toggleFullscreen ), QKeySequence( "F11" ) );
	GlobalCommands_insert( "MaximizeView", makeCallbackF( Maximize_View ), QKeySequence( "F12" ) );

	CSG_registerCommands();

	Grid_registerCommands();

	Patch_registerCommands();
	XYShow_registerCommands();

	GlobalPreferenceSystem().registerPreference( "DetachableMenus", makeBoolStringImportCallback( LatchedAssignCaller( g_Layout_enableDetachableMenus ) ), BoolExportStringCaller( g_Layout_enableDetachableMenus.m_latched ) );
	GlobalPreferenceSystem().registerPreference( "QE4StyleWindows", makeIntStringImportCallback( LatchedAssignCaller( g_Layout_viewStyle ) ), IntExportStringCaller( g_Layout_viewStyle.m_latched ) );
	GlobalPreferenceSystem().registerPreference( "BuiltInGroupDialog", makeBoolStringImportCallback( LatchedAssignCaller( g_Layout_builtInGroupDialog ) ), BoolExportStringCaller( g_Layout_builtInGroupDialog.m_latched ) );
	GlobalPreferenceSystem().registerPreference( "ExpiramentalFeatures", makeBoolStringImportCallback( LatchedAssignCaller( g_Layout_expiramentalFeatures ) ), BoolExportStringCaller( g_Layout_expiramentalFeatures.m_latched ) );
	GlobalPreferenceSystem().registerPreference( "ToolbarHiddenButtons", CopiedStringImportStringCaller( g_toolbarHiddenButtons ), CopiedStringExportStringCaller( g_toolbarHiddenButtons ) );
	GlobalPreferenceSystem().registerPreference( "OpenGLFont", CopiedStringImportStringCaller( g_OpenGLFont ), CopiedStringExportStringCaller( g_OpenGLFont ) );
	GlobalPreferenceSystem().registerPreference( "OpenGLFontSize", IntImportStringCaller( g_OpenGLFontSize ), IntExportStringCaller( g_OpenGLFontSize ) );

	for( size_t i = 0; i < g_strExtraResourcePaths.size(); ++i )
		GlobalPreferenceSystem().registerPreference( StringStream<32>( "ExtraResourcePath", i ),
			CopiedStringImportStringCaller( g_strExtraResourcePaths[i] ), CopiedStringExportStringCaller( g_strExtraResourcePaths[i] ) );

	GlobalPreferenceSystem().registerPreference( "EnginePath", CopiedStringImportStringCaller( g_strEnginePath ), CopiedStringExportStringCaller( g_strEnginePath ) );
	GlobalPreferenceSystem().registerPreference( "InstalledDevFilesPath", CopiedStringImportStringCaller( g_installedDevFilesPath ), CopiedStringExportStringCaller( g_installedDevFilesPath ) );
	if ( g_strEnginePath.empty() )
	{
		g_strEnginePath_was_empty_1st_start = true;
		const char* ENGINEPATH_ATTRIBUTE =
#if defined( WIN32 )
		    "enginepath_win32"
#elif defined( __APPLE__ )
		    "enginepath_macos"
#elif defined( __linux__ ) || defined ( __FreeBSD__ )
		    "enginepath_linux"
#else
#error "unknown platform"
#endif
		    ;
		g_strEnginePath = StringStream( DirectoryCleaned( g_pGameDescription->getRequiredKeyValue( ENGINEPATH_ATTRIBUTE ) ) );
	}


	Layout_registerPreferencesPage();
	Paths_registerPreferencesPage();

	g_brushCount.setCountChangedCallback( makeCallbackF( QE_brushCountChanged ) );
	g_patchCount.setCountChangedCallback( makeCallbackF( QE_brushCountChanged ) );
	g_entityCount.setCountChangedCallback( makeCallbackF( QE_brushCountChanged ) );
	GlobalEntityCreator().setCounter( &g_entityCount );
	GlobalSelectionSystem().addSelectionChangeCallback( FreeCaller<void(const Selectable&), brushCountChanged>() );
	GlobalSelectionSystem().addSelectionChangeCallback( FreeCaller<void(const Selectable&), Experimental_selectionChanged>() );

	GLWidget_sharedContextCreated = GlobalGL_sharedContextCreated;
	GLWidget_sharedContextDestroyed = GlobalGL_sharedContextDestroyed;

	GlobalEntityClassManager().attach( g_WorldspawnColourEntityClassObserver );
}

void MainFrame_Destroy(){
	GlobalEntityClassManager().detach( g_WorldspawnColourEntityClassObserver );

	GlobalEntityCreator().setCounter( 0 );
	g_entityCount.setCountChangedCallback( Callback<void()>() );
	g_patchCount.setCountChangedCallback( Callback<void()>() );
	g_brushCount.setCountChangedCallback( Callback<void()>() );
}
