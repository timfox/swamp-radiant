#include "mainframe_commands.h"

#include "mainframe.h"
#include "commands.h"
#include "console.h"
#include "entityinspector.h"
#include "entitylist.h"
#include "layerswindow.h"
#include "modelwindow.h"
#include "preferences.h"
#include "tools.h"
#include "build.h"
#include "colors.h"
#include "csg.h"
#include "grid.h"
#include "patchmanip.h"
#include "xywindow.h"
#include "select.h"
#include "audio_workbench.h"
#include "video_workbench.h"
#include "spreadsheet_workbench.h"
#include "referencecache.h"

#include "generic/callback.h"

#include <QKeySequence>

void MainFrame_registerCommands(){
	GlobalCommands_insert( "OpenManual", makeCallbackF( OpenHelpURL ), QKeySequence( "F1" ) );

	GlobalCommands_insert( "RefreshReferences", makeCallbackF( RefreshReferences ) );
	GlobalCommands_insert( "CheckForUpdate", makeCallbackF( CheckForUpdate ) );
	GlobalCommands_insert( "Exit", makeCallbackF( Exit ) );
	GlobalCommands_insert( "AddEntityByName", makeCallbackF( Add_openEntityDialog ) );
	GlobalCommands_insert( "AddLight", makeCallbackF( Add_createLight ) );
	GlobalCommands_insert( "AddInfoPlayerStart", makeCallbackF( Add_createInfoPlayerStart ) );
	GlobalCommands_insert( "AddInfoPlayerDeathmatch", makeCallbackF( Add_createInfoPlayerDeathmatch ) );
	GlobalCommands_insert( "AddMiscModel", makeCallbackF( Add_createMiscModel ) );
	GlobalCommands_insert( "LayoutHammerFourPane", makeCallbackF( Layout_setHammerFourPane ) );
	GlobalCommands_insert( "OpenIdTech3ToolCenter", makeCallbackF( IdTech3Tool_openHubDialog ), QKeySequence( "Ctrl+Alt+T" ) );
	GlobalCommands_insert( "OpenAudioWorkbench", makeCallbackF( AudioWorkbench_open ), QKeySequence( "Ctrl+Alt+M" ) );
	GlobalCommands_insert( "OpenCinematicPlayer", makeCallbackF( VideoWorkbench_open ), QKeySequence( "Ctrl+Alt+V" ) );
	GlobalCommands_insert( "OpenSpreadsheetWorkbench", makeCallbackF( Spreadsheet_open ), QKeySequence( "Ctrl+Alt+E" ) );
	GlobalCommands_insert( "OpenAudioPreview", makeCallbackF( AudioWorkbench_open ) );
	GlobalCommands_insert( "ToolQ3Map2Help", makeCallbackF( +[](){ IdTech3Tool_runHelp( g_idTech3Tools[0] ); } ) );
	GlobalCommands_insert( "ToolQData3Help", makeCallbackF( +[](){ IdTech3Tool_runHelp( g_idTech3Tools[1] ); } ) );
	GlobalCommands_insert( "ToolQ2MapHelp", makeCallbackF( +[](){ IdTech3Tool_runHelp( g_idTech3Tools[2] ); } ) );
	GlobalCommands_insert( "ToolMBSPCHelp", makeCallbackF( +[](){ IdTech3Tool_runHelp( g_idTech3Tools[3] ); } ) );
	GlobalCommands_insert( "LuaEditProps", makeCallbackF( Lua_editProps ) );
	GlobalCommands_insert( "LuaEditEntities", makeCallbackF( Lua_editEntities ) );
	GlobalCommands_insert( "LuaEditItems", makeCallbackF( Lua_editItems ) );
	GlobalCommands_insert( "LuaEditMain", makeCallbackF( Lua_editMain ) );
	GlobalCommands_insert( "LuaEditObjectives", makeCallbackF( Lua_editObjectives ) );
	GlobalCommands_insert( "LuaEditPropsExternal", makeCallbackF( Lua_editPropsExternal ) );
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
}
