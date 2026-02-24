#pragma once

class QMainWindow;

void Spreadsheet_createDock( QMainWindow* window );
void Spreadsheet_open();
void Spreadsheet_stopAndRelease();
bool Spreadsheet_requestClose();
