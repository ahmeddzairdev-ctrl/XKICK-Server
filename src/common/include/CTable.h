// =============================================================================
// CTable.h - portable CSV table API (rebuild of the binary's Quake-derived
// table.cpp). The original used a hunk allocator + custom CSV_Parse + table_t
// linked list; this reimplements the SAME PUBLIC API over a clean std:: parse,
// producing identical query results. Used by load.cpp on both servers.
//
// CSV layout (verified against ../Table/*/*.csv): row 0 = field names; column 0
// is "Index", a 0-based record id (0,1,2,...). Table_GetData(handle, i, "Field")
// returns the cell of column "Field" in the row whose Index == i.
// =============================================================================
#ifndef _CTABLE_H_
#define _CTABLE_H_

// Field value record (layout mirrors the binary's tvar_s for familiarity).
struct tvar_s
{
    char*  string;     // cell text (owned by the table)
    double value;      // atof(string), precomputed
    int    flags;
    bool   modified;
};
typedef struct tvar_s tvar_t;

// Set the directory tables are loaded from (default "../Table"). CLoad::InitLoad
// calls this with "../Table/<CompanyDir>" (CompanyDir = g_Config.m_sPath). Keeps
// this module decoupled from CConfig so it stays shared/common.
void Table_SetRoot(const char* rootPath);

// Loads "<root>/<name>" (and falls back to "../Table/<name>"). Cached by name
// (case-insensitive). Returns a non-negative handle, or -1 on failure.
int Table_FindTable(const char* name);

// Look up a cell by record id (int, 0-based) or by its string form, and field
// name. Returns NULL if the table/record/field does not exist.
tvar_t* Table_GetData(int handle, int recordIndex, const char* fieldName);
tvar_t* Table_GetData(int handle, const char* recordId, const char* fieldName);

// Convenience converters (NULL-safe), matching the binary.
int  FieldToValue(const tvar_t* field);                 // atoi(field->string) or 0
int  FieldsToValue(int nIndex, const tvar_t* field);    // nIndex-th '|'-separated sub-value
void FieldToString(char* dst, const tvar_t* field, int nLen); // copy or ""

void Table_DeleteAll();   // free all cached tables (shutdown)

#endif // _CTABLE_H_
