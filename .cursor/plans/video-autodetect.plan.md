## Plan

- **Rename the dock**: Change the video workbench UI strings/actions so it’s a “Video Player” rather than “Cinematic Player”. Update tooltips/menus accordingly (`video_workbench.cpp` lines ~196‑275).
- **Build auto scan**: Extend the workbench to scan the configured `content/` folder (reuse `VideoWorkbench_contentFolder`/`setting` helpers) for video files (`.mp4`, `.avi`, `.roq`, etc.), recursively, and populate a `QListWidget` or `QComboBox` with detected movies so the user can just double‑click to play. Add a “Refresh Movies” button/toolbar alongside the existing controls.
- **RoQ support**: Ensure the Qt multimedia backend can open `.RoQ` (might not be supported); provide a fallback by calling the engine’s RoQ playback utility if Qt can’t open it. For example, detect `.roq` files and run the game’s RoQ player via a shell command (like `RoQ.exe` or `roqplayer`), or if possible add a dropdown that uses the existing command-line `roqplay` pipeline (e.g., call `tools/roqplayer` if present). Document that this is only available when the command exists.
- **Persist and auto-run scan**: Store movies detected in settings so the list refreshes quickly at startup, and run the scan automatically when the dock opens (unless the plugin is disabled). Provide status text showing number of movies found.

> Todos

- Update the dock’s labels/buttons to “Video Player” and add a “Refresh Movies” control.
- Implement a recursive scan of the content folder for video files (common extensions + `.roq`) and populate the UI list.
- Auto-run the scan on open and enable RoQ playback by either using Qt where supported or falling back to the RoQ utility.
