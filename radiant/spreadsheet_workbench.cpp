#include "spreadsheet_workbench.h"

#include "mainframe.h"
#include "stream/stringstream.h"

#include <QMainWindow>
#include <QDockWidget>
#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QLineEdit>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QFileDialog>
#include <QFile>
#include <QFileInfo>
#include <QTextStream>
#include <QHeaderView>
#include <QApplication>
#include <QClipboard>
#include <QCheckBox>
#include <QMessageBox>
#include <QMenu>
#include <QSettings>

#include <algorithm>
#include <set>

namespace
{

QDockWidget* g_spreadsheetDock{};
QTableWidget* g_spreadsheetTable{};
QLineEdit* g_spreadsheetFormulaEdit{};
QLabel* g_spreadsheetStatusLabel{};
bool g_spreadsheetUpdating{};
QCheckBox* g_spreadsheetAutoRecalcCheck{};
bool g_spreadsheetDirty{};
QString g_spreadsheetCurrentPath;

const char* const c_spreadsheetSettingsPrefix = "SpreadsheetWorkbench/";

QSettings& Spreadsheet_settings(){
	static QSettings settings;
	return settings;
}

QString Spreadsheet_setting( const char* key, const QString& fallback = {} ){
	return Spreadsheet_settings().value( StringStream( c_spreadsheetSettingsPrefix, key ).c_str(), fallback ).toString();
}

void Spreadsheet_setSetting( const char* key, const QVariant& value ){
	Spreadsheet_settings().setValue( StringStream( c_spreadsheetSettingsPrefix, key ).c_str(), value );
}

QString Spreadsheet_defaultDirectory(){
	const QString last = Spreadsheet_setting( "LastDirectory" );
	if ( !last.isEmpty() ) {
		return last;
	}
	return QString::fromLatin1( EnginePath_get() );
}

void Spreadsheet_setLastDirectory( const QString& path ){
	if ( path.isEmpty() ) {
		return;
	}
	const QFileInfo info( path );
	const QString directory = info.isDir() ? info.absoluteFilePath() : info.absolutePath();
	if ( !directory.isEmpty() ) {
		Spreadsheet_setSetting( "LastDirectory", directory );
	}
}

void Spreadsheet_updateDockTitle(){
	if ( g_spreadsheetDock == nullptr ) {
		return;
	}
	const QString marker = g_spreadsheetDirty ? "*" : "";
	if ( g_spreadsheetCurrentPath.isEmpty() ) {
		g_spreadsheetDock->setWindowTitle( StringStream( "Spreadsheet", marker.toUtf8().constData() ).c_str() );
	}
	else{
		const auto name = QFileInfo( g_spreadsheetCurrentPath ).fileName();
		g_spreadsheetDock->setWindowTitle( StringStream( "Spreadsheet - ", name.toUtf8().constData(), marker.toUtf8().constData() ).c_str() );
	}
}

void Spreadsheet_setDirty( bool dirty ){
	g_spreadsheetDirty = dirty;
	Spreadsheet_updateDockTitle();
}

void Spreadsheet_markDirty(){
	Spreadsheet_setDirty( true );
}

QString Spreadsheet_columnName( int column ){
	QString result;
	int value = column;
	do {
		result.prepend( QChar( 'A' + ( value % 26 ) ) );
		value = value / 26 - 1;
	} while ( value >= 0 );
	return result;
}

QString Spreadsheet_cellRef( int row, int column ){
	return StringStream( Spreadsheet_columnName( column ).toUtf8().constData(), row + 1 ).c_str();
}

bool Spreadsheet_parseCellRef( const QString& token, int& row, int& column ){
	if ( token.isEmpty() ) {
		return false;
	}

	int i = 0;
	column = 0;
	while ( i < token.size() && token[i].isLetter() )
	{
		column = column * 26 + ( token[i].toUpper().unicode() - 'A' + 1 );
		++i;
	}
	if ( i == 0 || i >= token.size() ) {
		return false;
	}

	bool ok = false;
	row = token.mid( i ).toInt( &ok ) - 1;
	column -= 1;
	return ok && row >= 0 && column >= 0;
}

QString Spreadsheet_rawCellText( int row, int column ){
	if ( g_spreadsheetTable == nullptr ) {
		return "";
	}
	if ( auto* item = g_spreadsheetTable->item( row, column ) ) {
		const auto raw = item->data( Qt::UserRole ).toString();
		return raw.isNull() ? item->text() : raw;
	}
	return "";
}

double Spreadsheet_evalCell( int row, int column, std::set<quint64>& stack, bool& ok );

struct SpreadsheetExpressionParser
{
	const QString& m_expr;
	int m_pos{};
	std::set<quint64>& m_stack;
	bool m_ok{ true };
	int m_currentRow{};
	int m_currentColumn{};

	bool parseCellToken( int& row, int& column ){
		const int start = m_pos;
		QString token;
		while ( m_pos < m_expr.size() && m_expr[m_pos].isLetterOrNumber() )
		{
			token += m_expr[m_pos++];
		}
		if ( Spreadsheet_parseCellRef( token, row, column ) ) {
			return true;
		}
		m_pos = start;
		return false;
	}

	bool parseRangeArgument( QVector<double>& values ){
		const int start = m_pos;
		int row1 = 0, column1 = 0, row2 = 0, column2 = 0;
		if ( !parseCellToken( row1, column1 ) ) {
			return false;
		}
		skipSpaces();
		if ( !consume( ':' ) ) {
			m_pos = start;
			return false;
		}
		if ( !parseCellToken( row2, column2 ) ) {
			m_ok = false;
			return false;
		}

		for ( int r = std::min( row1, row2 ); r <= std::max( row1, row2 ); ++r )
		{
			for ( int c = std::min( column1, column2 ); c <= std::max( column1, column2 ); ++c )
			{
				bool ok = false;
				const auto value = Spreadsheet_evalCell( r, c, m_stack, ok );
				if ( !ok ) {
					m_ok = false;
					return true;
				}
				values.push_back( value );
			}
		}
		return true;
	}

	double evalFunction( const QString& name, const QVector<double>& values ){
		if ( values.empty() ) {
			m_ok = false;
			return 0.0;
		}
		if ( name == "SUM" ) {
			double sum = 0.0;
			for ( const auto value : values )
				sum += value;
			return sum;
		}
		if ( name == "AVG" || name == "AVERAGE" ) {
			double sum = 0.0;
			for ( const auto value : values )
				sum += value;
			return sum / double( values.size() );
		}
		if ( name == "MIN" ) {
			double minValue = values[0];
			for ( const auto value : values )
				minValue = std::min( minValue, value );
			return minValue;
		}
		if ( name == "MAX" ) {
			double maxValue = values[0];
			for ( const auto value : values )
				maxValue = std::max( maxValue, value );
			return maxValue;
		}
		if ( name == "COUNT" ) {
			return double( values.size() );
		}
		m_ok = false;
		return 0.0;
	}

	double parseExpression( int currentRow, int currentColumn ){
		m_currentRow = currentRow;
		m_currentColumn = currentColumn;
		double value = parseTerm( currentRow, currentColumn );
		while ( m_ok )
		{
			skipSpaces();
			if ( consume( '+' ) ) {
				value += parseTerm( currentRow, currentColumn );
			}
			else if ( consume( '-' ) ) {
				value -= parseTerm( currentRow, currentColumn );
			}
			else{
				break;
			}
		}
		return value;
	}

	double parseTerm( int currentRow, int currentColumn ){
		double value = parseFactor( currentRow, currentColumn );
		while ( m_ok )
		{
			skipSpaces();
			if ( consume( '*' ) ) {
				value *= parseFactor( currentRow, currentColumn );
			}
			else if ( consume( '/' ) ) {
				const double divisor = parseFactor( currentRow, currentColumn );
				if ( divisor == 0.0 ) {
					m_ok = false;
					return 0.0;
				}
				value /= divisor;
			}
			else{
				break;
			}
		}
		return value;
	}

	double parseFactor( int currentRow, int currentColumn ){
		skipSpaces();
		if ( consume( '+' ) ) {
			return parseFactor( currentRow, currentColumn );
		}
		if ( consume( '-' ) ) {
			return -parseFactor( currentRow, currentColumn );
		}
		if ( consume( '(' ) ) {
			const double value = parseExpression( currentRow, currentColumn );
			if ( !consume( ')' ) ) {
				m_ok = false;
			}
			return value;
		}

		if ( m_pos < m_expr.size() && m_expr[m_pos].isLetter() ) {
			QString identifier;
			while ( m_pos < m_expr.size() && m_expr[m_pos].isLetter() )
			{
				identifier += m_expr[m_pos++];
			}

			skipSpaces();
			if ( consume( '(' ) ) {
				QVector<double> values;
				skipSpaces();
				if ( !consume( ')' ) ) {
					while ( m_ok )
					{
						QVector<double> rangeValues;
						if ( parseRangeArgument( rangeValues ) ) {
							for ( const auto value : rangeValues )
								values.push_back( value );
						}
						else{
							values.push_back( parseExpression( m_currentRow, m_currentColumn ) );
						}
						skipSpaces();
						if ( consume( ',' ) ) {
							continue;
						}
						if ( consume( ')' ) ) {
							break;
						}
						m_ok = false;
						break;
					}
				}
				return evalFunction( identifier.toUpper(), values );
			}

			QString token = identifier;
			while ( m_pos < m_expr.size() && m_expr[m_pos].isDigit() )
			{
				token += m_expr[m_pos++];
			}
			int row = 0, column = 0;
			if ( !Spreadsheet_parseCellRef( token, row, column ) ) {
				m_ok = false;
				return 0.0;
			}
			return Spreadsheet_evalCell( row, column, m_stack, m_ok );
		}

		QString number;
		bool hasDot{};
		while ( m_pos < m_expr.size() )
		{
			const auto c = m_expr[m_pos];
			if ( c.isDigit() ) {
				number += c;
				++m_pos;
			}
			else if ( c == '.' && !hasDot ) {
				hasDot = true;
				number += c;
				++m_pos;
			}
			else{
				break;
			}
		}
		bool ok = false;
		const double value = number.toDouble( &ok );
		if ( !ok ) {
			m_ok = false;
			return 0.0;
		}
		return value;
	}

	void skipSpaces(){
		while ( m_pos < m_expr.size() && m_expr[m_pos].isSpace() )
		{
			++m_pos;
		}
	}

	bool consume( QChar c ){
		skipSpaces();
		if ( m_pos < m_expr.size() && m_expr[m_pos] == c ) {
			++m_pos;
			return true;
		}
		return false;
	}
};

double Spreadsheet_evalCell( int row, int column, std::set<quint64>& stack, bool& ok ){
	ok = true;
	if ( g_spreadsheetTable == nullptr ) {
		return 0.0;
	}
	if ( row < 0 || column < 0 || row >= g_spreadsheetTable->rowCount() || column >= g_spreadsheetTable->columnCount() ) {
		ok = false;
		return 0.0;
	}

	const quint64 key = ( quint64( row ) << 32 ) | quint64( column );
	if ( stack.find( key ) != stack.end() ) {
		ok = false;
		return 0.0;
	}
	stack.insert( key );

	const auto raw = Spreadsheet_rawCellText( row, column ).trimmed();
	if ( !raw.startsWith( '=' ) ) {
		bool numeric = false;
		const auto number = raw.toDouble( &numeric );
		stack.erase( key );
		if ( raw.isEmpty() ) {
			return 0.0;
		}
		if ( numeric ) {
			return number;
		}
		ok = false;
		return 0.0;
	}

	const auto expression = raw.mid( 1 );
	SpreadsheetExpressionParser parser{ expression, 0, stack, true };
	const double value = parser.parseExpression( row, column );
	parser.skipSpaces();
	ok = parser.m_ok && parser.m_pos == expression.size();
	stack.erase( key );
	return ok ? value : 0.0;
}

void Spreadsheet_refreshHeaders(){
	if ( g_spreadsheetTable == nullptr ) {
		return;
	}
	QStringList horizontal;
	for ( int c = 0; c < g_spreadsheetTable->columnCount(); ++c )
	{
		horizontal.push_back( Spreadsheet_columnName( c ) );
	}
	g_spreadsheetTable->setHorizontalHeaderLabels( horizontal );

	QStringList vertical;
	for ( int r = 0; r < g_spreadsheetTable->rowCount(); ++r )
	{
		vertical.push_back( QString::number( r + 1 ) );
	}
	g_spreadsheetTable->setVerticalHeaderLabels( vertical );
}

void Spreadsheet_recalculateAll(){
	if ( g_spreadsheetTable == nullptr ) {
		return;
	}
	g_spreadsheetUpdating = true;
	g_spreadsheetTable->blockSignals( true );
	for ( int row = 0; row < g_spreadsheetTable->rowCount(); ++row )
	{
		for ( int column = 0; column < g_spreadsheetTable->columnCount(); ++column )
		{
			auto* item = g_spreadsheetTable->item( row, column );
			if ( item == nullptr ) {
				item = new QTableWidgetItem();
				g_spreadsheetTable->setItem( row, column, item );
			}
			const auto raw = Spreadsheet_rawCellText( row, column );
			if ( raw.startsWith( '=' ) ) {
				bool ok = false;
				std::set<quint64> stack;
				const auto value = Spreadsheet_evalCell( row, column, stack, ok );
				item->setText( ok ? QString::number( value, 'g', 15 ) : "#ERR" );
			}
			else{
				item->setText( raw );
			}
			item->setData( Qt::UserRole, raw );
		}
	}
	g_spreadsheetTable->blockSignals( false );
	g_spreadsheetUpdating = false;
}

QVector<QStringList> Spreadsheet_parseCSVText( const QString& text ){
	QVector<QStringList> rows;
	QStringList currentRow;
	QString currentField;
	bool inQuotes{};

	auto flushField = [&](){
		currentRow.push_back( currentField );
		currentField.clear();
	};
	auto flushRow = [&](){
		rows.push_back( currentRow );
		currentRow.clear();
	};

	for ( int i = 0; i < text.size(); ++i )
	{
		const QChar c = text[i];
		if ( inQuotes ) {
			if ( c == '"' ) {
				if ( i + 1 < text.size() && text[i + 1] == '"' ) {
					currentField += '"';
					++i;
				}
				else{
					inQuotes = false;
				}
			}
			else{
				currentField += c;
			}
			continue;
		}

		if ( c == '"' ) {
			inQuotes = true;
		}
		else if ( c == ',' ) {
			flushField();
		}
		else if ( c == '\n' ) {
			flushField();
			flushRow();
		}
		else if ( c == '\r' ) {
			if ( i + 1 < text.size() && text[i + 1] == '\n' ) {
				++i;
			}
			flushField();
			flushRow();
		}
		else{
			currentField += c;
		}
	}

	if ( !text.isEmpty() ) {
		const bool endedWithNewline = text.endsWith( '\n' ) || text.endsWith( '\r' );
		if ( !endedWithNewline || !currentField.isEmpty() || !currentRow.isEmpty() ) {
			flushField();
			flushRow();
		}
	}

	return rows;
}

QString Spreadsheet_joinCSVLine( const QStringList& fields ){
	QStringList escaped;
	for ( const auto& field : fields )
	{
		QString value = field;
		value.replace( "\"", "\"\"" );
		if ( value.contains( ',' ) || value.contains( '"' ) || value.contains( '\n' ) || value.contains( '\r' ) ) {
			value = StringStream( "\"", value.toUtf8().constData(), "\"" ).c_str();
		}
		escaped.push_back( value );
	}
	return escaped.join( ',' );
}

bool Spreadsheet_shouldAutoRecalc(){
	return g_spreadsheetAutoRecalcCheck == nullptr || g_spreadsheetAutoRecalcCheck->isChecked();
}

void Spreadsheet_copySelection(){
	if ( g_spreadsheetTable == nullptr ) {
		return;
	}
	const auto ranges = g_spreadsheetTable->selectedRanges();
	if ( ranges.isEmpty() ) {
		return;
	}
	const auto range = ranges.front();
	QStringList lines;
	for ( int r = range.topRow(); r <= range.bottomRow(); ++r )
	{
		QStringList fields;
		for ( int c = range.leftColumn(); c <= range.rightColumn(); ++c )
		{
			fields.push_back( Spreadsheet_rawCellText( r, c ) );
		}
		lines.push_back( fields.join( '\t' ) );
	}
	QApplication::clipboard()->setText( lines.join( '\n' ) );
}

void Spreadsheet_pasteClipboard(){
	if ( g_spreadsheetTable == nullptr ) {
		return;
	}
	const auto text = QApplication::clipboard()->text();
	if ( text.isEmpty() ) {
		return;
	}
	const int startRow = std::max( 0, g_spreadsheetTable->currentRow() );
	const int startColumn = std::max( 0, g_spreadsheetTable->currentColumn() );
	const auto rows = text.split( '\n', Qt::KeepEmptyParts );
	int rowOffset = 0;
	for ( const auto& row : rows )
	{
		if ( rowOffset > 0 && row.isEmpty() ) {
			continue;
		}
		const auto fields = row.split( '\t', Qt::KeepEmptyParts );
		int columnOffset = 0;
		for ( const auto& field : fields )
		{
			const int rowIndex = startRow + rowOffset;
			const int columnIndex = startColumn + columnOffset;
			if ( rowIndex >= g_spreadsheetTable->rowCount() ) {
				g_spreadsheetTable->setRowCount( rowIndex + 1 );
			}
			if ( columnIndex >= g_spreadsheetTable->columnCount() ) {
				g_spreadsheetTable->setColumnCount( columnIndex + 1 );
			}
			auto* item = g_spreadsheetTable->item( rowIndex, columnIndex );
			if ( item == nullptr ) {
				item = new QTableWidgetItem();
				g_spreadsheetTable->setItem( rowIndex, columnIndex, item );
			}
			item->setData( Qt::UserRole, field );
			item->setText( field );
			++columnOffset;
		}
		++rowOffset;
	}
	Spreadsheet_refreshHeaders();
	if ( Spreadsheet_shouldAutoRecalc() ) {
		Spreadsheet_recalculateAll();
	}
	Spreadsheet_markDirty();
}

void Spreadsheet_clearSelection(){
	if ( g_spreadsheetTable == nullptr ) {
		return;
	}
	for ( auto* item : g_spreadsheetTable->selectedItems() )
	{
		item->setData( Qt::UserRole, "" );
		item->setText( "" );
	}
	if ( Spreadsheet_shouldAutoRecalc() ) {
		Spreadsheet_recalculateAll();
	}
	Spreadsheet_markDirty();
}

void Spreadsheet_applyFormulaEdit(){
	if ( g_spreadsheetTable == nullptr || g_spreadsheetFormulaEdit == nullptr ) {
		return;
	}
	auto* item = g_spreadsheetTable->currentItem();
	if ( item == nullptr ) {
		const int row = g_spreadsheetTable->currentRow() >= 0 ? g_spreadsheetTable->currentRow() : 0;
		const int column = g_spreadsheetTable->currentColumn() >= 0 ? g_spreadsheetTable->currentColumn() : 0;
		item = new QTableWidgetItem();
		g_spreadsheetTable->setItem( row, column, item );
		g_spreadsheetTable->setCurrentItem( item );
	}
	item->setData( Qt::UserRole, g_spreadsheetFormulaEdit->text() );
	if ( Spreadsheet_shouldAutoRecalc() ) {
		Spreadsheet_recalculateAll();
	}
	else{
		item->setText( g_spreadsheetFormulaEdit->text() );
	}
	Spreadsheet_markDirty();
}

void Spreadsheet_showContextMenu( const QPoint& pos ){
	if ( g_spreadsheetTable == nullptr ) {
		return;
	}
	QMenu menu( g_spreadsheetTable );
	auto* copyAction = menu.addAction( "Copy" );
	auto* pasteAction = menu.addAction( "Paste" );
	auto* clearAction = menu.addAction( "Clear Selection" );
	menu.addSeparator();
	auto* recalcAction = menu.addAction( "Recalculate" );

	if ( QAction* chosen = menu.exec( g_spreadsheetTable->viewport()->mapToGlobal( pos ) ) ) {
		if ( chosen == copyAction ) {
			Spreadsheet_copySelection();
		}
		else if ( chosen == pasteAction ) {
			Spreadsheet_pasteClipboard();
		}
		else if ( chosen == clearAction ) {
			Spreadsheet_clearSelection();
		}
		else if ( chosen == recalcAction ) {
			Spreadsheet_recalculateAll();
		}
	}
}

bool Spreadsheet_saveToPath( const QString& path, bool showErrors ){
	if ( g_spreadsheetTable == nullptr || path.isEmpty() ) {
		return false;
	}

	QFile file( path );
	if ( !file.open( QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate ) ) {
		if ( showErrors ) {
			QMessageBox::warning( MainFrame_getWindow(), "Spreadsheet", "Failed to save CSV file." );
		}
		return false;
	}

	int lastRow = -1;
	int lastColumn = -1;
	for ( int r = 0; r < g_spreadsheetTable->rowCount(); ++r )
	{
		for ( int c = 0; c < g_spreadsheetTable->columnCount(); ++c )
		{
			if ( !Spreadsheet_rawCellText( r, c ).isEmpty() ) {
				lastRow = std::max( lastRow, r );
				lastColumn = std::max( lastColumn, c );
			}
		}
	}

	QTextStream out( &file );
	for ( int r = 0; r <= lastRow; ++r )
	{
		QStringList fields;
		for ( int c = 0; c <= lastColumn; ++c )
		{
			fields.push_back( Spreadsheet_rawCellText( r, c ) );
		}
		out << Spreadsheet_joinCSVLine( fields ) << '\n';
	}

	g_spreadsheetCurrentPath = QFileInfo( path ).absoluteFilePath();
	Spreadsheet_setLastDirectory( g_spreadsheetCurrentPath );
	Spreadsheet_setDirty( false );
	return true;
}

bool Spreadsheet_saveInteractive(){
	if ( g_spreadsheetTable == nullptr ) {
		return false;
	}
	const auto path = QFileDialog::getSaveFileName( MainFrame_getWindow(), "Save Spreadsheet CSV", Spreadsheet_defaultDirectory(), "CSV Files (*.csv)" );
	if ( path.isEmpty() ) {
		return false;
	}
	return Spreadsheet_saveToPath( path, true );
}

bool Spreadsheet_promptSaveIfDirty( const char* contextTitle ){
	if ( !g_spreadsheetDirty ) {
		return true;
	}

	const auto answer = QMessageBox::warning(
	    MainFrame_getWindow(),
	    contextTitle,
	    "Spreadsheet has unsaved changes. Save before continuing?",
	    QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel,
	    QMessageBox::Save
	);
	if ( answer == QMessageBox::Cancel ) {
		return false;
	}
	if ( answer == QMessageBox::Discard ) {
		return true;
	}

	if ( !g_spreadsheetCurrentPath.isEmpty() ) {
		return Spreadsheet_saveToPath( g_spreadsheetCurrentPath, true );
	}
	return Spreadsheet_saveInteractive();
}

bool Spreadsheet_loadFromPath( const QString& path ){
	if ( g_spreadsheetTable == nullptr ) {
		return false;
	}

	QFile file( path );
	if ( !file.open( QIODevice::ReadOnly | QIODevice::Text ) ) {
		QMessageBox::warning( MainFrame_getWindow(), "Spreadsheet", "Failed to open CSV file." );
		return false;
	}

	QTextStream in( &file );
	const QVector<QStringList> rows = Spreadsheet_parseCSVText( in.readAll() );
	int maxColumns = 0;
	for ( const auto& row : rows )
	{
		maxColumns = std::max( maxColumns, row.size() );
	}

	g_spreadsheetTable->clearContents();
	g_spreadsheetTable->setRowCount( std::max( 50, rows.size() ) );
	g_spreadsheetTable->setColumnCount( std::max( 16, maxColumns ) );
	Spreadsheet_refreshHeaders();

	for ( int r = 0; r < rows.size(); ++r )
	{
		for ( int c = 0; c < rows[r].size(); ++c )
		{
			auto* item = new QTableWidgetItem();
			item->setData( Qt::UserRole, rows[r][c] );
			item->setText( rows[r][c] );
			g_spreadsheetTable->setItem( r, c, item );
		}
	}
	Spreadsheet_recalculateAll();
	g_spreadsheetCurrentPath = QFileInfo( path ).absoluteFilePath();
	Spreadsheet_setLastDirectory( g_spreadsheetCurrentPath );
	Spreadsheet_setDirty( false );
	return true;
}

void Spreadsheet_resetNewDocument(){
	if ( g_spreadsheetTable == nullptr ) {
		return;
	}
	g_spreadsheetTable->clearContents();
	g_spreadsheetTable->setRowCount( 50 );
	g_spreadsheetTable->setColumnCount( 16 );
	Spreadsheet_refreshHeaders();
	Spreadsheet_recalculateAll();
	g_spreadsheetCurrentPath.clear();
	Spreadsheet_setDirty( false );
}

}

void Spreadsheet_open(){
	if ( g_spreadsheetDock == nullptr ) {
		return;
	}
	g_spreadsheetDock->show();
	g_spreadsheetDock->raise();
}

void Spreadsheet_createDock( QMainWindow* window ){
	if ( window == nullptr || g_spreadsheetDock != nullptr ) {
		return;
	}

	g_spreadsheetDock = new QDockWidget( "Spreadsheet", window );
	g_spreadsheetDock->setObjectName( "dock_spreadsheet_workbench" );

	auto* root = new QWidget( g_spreadsheetDock );
	auto* layout = new QVBoxLayout( root );

	auto* topButtons = new QHBoxLayout();
	auto* newButton = new QPushButton( "New", root );
	auto* openButton = new QPushButton( "Open CSV...", root );
	auto* saveButton = new QPushButton( "Save CSV...", root );
	auto* addRowButton = new QPushButton( "Add Row", root );
	auto* addColumnButton = new QPushButton( "Add Column", root );
	auto* insertRowButton = new QPushButton( "Insert Row", root );
	auto* insertColumnButton = new QPushButton( "Insert Column", root );
	auto* deleteRowButton = new QPushButton( "Delete Row", root );
	auto* deleteColumnButton = new QPushButton( "Delete Column", root );
	auto* copyButton = new QPushButton( "Copy", root );
	auto* pasteButton = new QPushButton( "Paste", root );
	auto* clearSelectionButton = new QPushButton( "Clear Sel", root );
	auto* recalcButton = new QPushButton( "Recalculate", root );
	topButtons->addWidget( newButton );
	topButtons->addWidget( openButton );
	topButtons->addWidget( saveButton );
	topButtons->addWidget( addRowButton );
	topButtons->addWidget( addColumnButton );
	topButtons->addWidget( insertRowButton );
	topButtons->addWidget( insertColumnButton );
	topButtons->addWidget( deleteRowButton );
	topButtons->addWidget( deleteColumnButton );
	topButtons->addWidget( copyButton );
	topButtons->addWidget( pasteButton );
	topButtons->addWidget( clearSelectionButton );
	topButtons->addWidget( recalcButton );
	layout->addLayout( topButtons );

	auto* formulaRow = new QHBoxLayout();
	formulaRow->addWidget( new QLabel( "Formula", root ) );
	g_spreadsheetFormulaEdit = new QLineEdit( root );
	g_spreadsheetFormulaEdit->setPlaceholderText( "=SUM(A1:A8), =AVG(B1:B8), =A1*2" );
	g_spreadsheetAutoRecalcCheck = new QCheckBox( "Auto recalc", root );
	g_spreadsheetAutoRecalcCheck->setChecked( true );
	auto* applyFormulaButton = new QPushButton( "Apply", root );
	formulaRow->addWidget( g_spreadsheetFormulaEdit, 1 );
	formulaRow->addWidget( g_spreadsheetAutoRecalcCheck );
	formulaRow->addWidget( applyFormulaButton );
	layout->addLayout( formulaRow );

	g_spreadsheetTable = new QTableWidget( 50, 16, root );
	g_spreadsheetTable->horizontalHeader()->setSectionResizeMode( QHeaderView::Interactive );
	g_spreadsheetTable->horizontalHeader()->setDefaultSectionSize( 110 );
	g_spreadsheetTable->verticalHeader()->setDefaultSectionSize( 24 );
	g_spreadsheetTable->setAlternatingRowColors( true );
	g_spreadsheetTable->setContextMenuPolicy( Qt::CustomContextMenu );
	layout->addWidget( g_spreadsheetTable, 1 );

	g_spreadsheetStatusLabel = new QLabel( "Ready", root );
	layout->addWidget( g_spreadsheetStatusLabel );

	Spreadsheet_refreshHeaders();
	Spreadsheet_recalculateAll();
	Spreadsheet_setDirty( false );

	QObject::connect( g_spreadsheetTable, &QTableWidget::itemSelectionChanged, [](){
		if ( g_spreadsheetTable == nullptr || g_spreadsheetFormulaEdit == nullptr || g_spreadsheetStatusLabel == nullptr ) {
			return;
		}
		auto* item = g_spreadsheetTable->currentItem();
		if ( item != nullptr ) {
			const auto raw = item->data( Qt::UserRole ).toString();
			g_spreadsheetFormulaEdit->setText( raw.isNull() ? item->text() : raw );
			g_spreadsheetStatusLabel->setText( StringStream( "Cell ", Spreadsheet_cellRef( item->row(), item->column() ).toUtf8().constData() ).c_str() );
		}
		else{
			g_spreadsheetFormulaEdit->clear();
			g_spreadsheetStatusLabel->setText( "Ready" );
		}
	} );
	QObject::connect( g_spreadsheetTable, &QTableWidget::itemChanged, []( QTableWidgetItem* item ){
		if ( item == nullptr || g_spreadsheetUpdating ) {
			return;
		}
		item->setData( Qt::UserRole, item->text() );
		if ( Spreadsheet_shouldAutoRecalc() ) {
			Spreadsheet_recalculateAll();
		}
		Spreadsheet_markDirty();
	} );
	QObject::connect( g_spreadsheetTable, &QWidget::customContextMenuRequested, []( const QPoint& pos ){
		Spreadsheet_showContextMenu( pos );
	} );
	QObject::connect( applyFormulaButton, &QPushButton::clicked, [](){ Spreadsheet_applyFormulaEdit(); } );
	QObject::connect( g_spreadsheetFormulaEdit, &QLineEdit::returnPressed, [](){ Spreadsheet_applyFormulaEdit(); } );

	QObject::connect( newButton, &QPushButton::clicked, [](){
		if ( !Spreadsheet_promptSaveIfDirty( "New Spreadsheet" ) ) {
			return;
		}
		Spreadsheet_resetNewDocument();
	} );
	QObject::connect( addRowButton, &QPushButton::clicked, [](){
		if ( g_spreadsheetTable == nullptr ) {
			return;
		}
		g_spreadsheetTable->setRowCount( g_spreadsheetTable->rowCount() + 1 );
		Spreadsheet_refreshHeaders();
		Spreadsheet_markDirty();
	} );
	QObject::connect( addColumnButton, &QPushButton::clicked, [](){
		if ( g_spreadsheetTable == nullptr ) {
			return;
		}
		g_spreadsheetTable->setColumnCount( g_spreadsheetTable->columnCount() + 1 );
		Spreadsheet_refreshHeaders();
		Spreadsheet_markDirty();
	} );
	QObject::connect( insertRowButton, &QPushButton::clicked, [](){
		if ( g_spreadsheetTable == nullptr ) {
			return;
		}
		const int row = g_spreadsheetTable->currentRow() >= 0 ? g_spreadsheetTable->currentRow() : 0;
		g_spreadsheetTable->insertRow( row );
		Spreadsheet_refreshHeaders();
		if ( Spreadsheet_shouldAutoRecalc() ) {
			Spreadsheet_recalculateAll();
		}
		Spreadsheet_markDirty();
	} );
	QObject::connect( insertColumnButton, &QPushButton::clicked, [](){
		if ( g_spreadsheetTable == nullptr ) {
			return;
		}
		const int column = g_spreadsheetTable->currentColumn() >= 0 ? g_spreadsheetTable->currentColumn() : 0;
		g_spreadsheetTable->insertColumn( column );
		Spreadsheet_refreshHeaders();
		if ( Spreadsheet_shouldAutoRecalc() ) {
			Spreadsheet_recalculateAll();
		}
		Spreadsheet_markDirty();
	} );
	QObject::connect( deleteRowButton, &QPushButton::clicked, [](){
		if ( g_spreadsheetTable == nullptr || g_spreadsheetTable->rowCount() <= 1 ) {
			return;
		}
		const int row = g_spreadsheetTable->currentRow();
		if ( row >= 0 ) {
			g_spreadsheetTable->removeRow( row );
			Spreadsheet_refreshHeaders();
			if ( Spreadsheet_shouldAutoRecalc() ) {
				Spreadsheet_recalculateAll();
			}
			Spreadsheet_markDirty();
		}
	} );
	QObject::connect( deleteColumnButton, &QPushButton::clicked, [](){
		if ( g_spreadsheetTable == nullptr || g_spreadsheetTable->columnCount() <= 1 ) {
			return;
		}
		const int column = g_spreadsheetTable->currentColumn();
		if ( column >= 0 ) {
			g_spreadsheetTable->removeColumn( column );
			Spreadsheet_refreshHeaders();
			if ( Spreadsheet_shouldAutoRecalc() ) {
				Spreadsheet_recalculateAll();
			}
			Spreadsheet_markDirty();
		}
	} );
	QObject::connect( copyButton, &QPushButton::clicked, [](){ Spreadsheet_copySelection(); } );
	QObject::connect( pasteButton, &QPushButton::clicked, [](){ Spreadsheet_pasteClipboard(); } );
	QObject::connect( clearSelectionButton, &QPushButton::clicked, [](){ Spreadsheet_clearSelection(); } );
	QObject::connect( recalcButton, &QPushButton::clicked, [](){ Spreadsheet_recalculateAll(); } );
	QObject::connect( openButton, &QPushButton::clicked, [](){
		if ( !Spreadsheet_promptSaveIfDirty( "Open Spreadsheet" ) ) {
			return;
		}
		const auto path = QFileDialog::getOpenFileName( MainFrame_getWindow(), "Open Spreadsheet CSV", Spreadsheet_defaultDirectory(), "CSV Files (*.csv);;All Files (*)" );
		if ( path.isEmpty() ) {
			return;
		}
		Spreadsheet_loadFromPath( path );
	} );
	QObject::connect( saveButton, &QPushButton::clicked, [](){
		Spreadsheet_saveInteractive();
	} );

	g_spreadsheetDock->setWidget( root );
	window->addDockWidget( Qt::BottomDockWidgetArea, g_spreadsheetDock );
	g_spreadsheetDock->hide();

	const auto lastFile = Spreadsheet_setting( "LastFile" );
	if ( !lastFile.isEmpty() && QFileInfo::exists( lastFile ) ) {
		Spreadsheet_loadFromPath( lastFile );
	}
	else{
		Spreadsheet_resetNewDocument();
	}
}

void Spreadsheet_stopAndRelease(){
	if ( !g_spreadsheetCurrentPath.isEmpty() ) {
		Spreadsheet_setSetting( "LastFile", g_spreadsheetCurrentPath );
	}
	g_spreadsheetDock = nullptr;
	g_spreadsheetTable = nullptr;
	g_spreadsheetFormulaEdit = nullptr;
	g_spreadsheetStatusLabel = nullptr;
	g_spreadsheetAutoRecalcCheck = nullptr;
	g_spreadsheetUpdating = false;
	g_spreadsheetDirty = false;
	g_spreadsheetCurrentPath.clear();
}

bool Spreadsheet_requestClose(){
	return Spreadsheet_promptSaveIfDirty( "Exit Radiant" );
}
