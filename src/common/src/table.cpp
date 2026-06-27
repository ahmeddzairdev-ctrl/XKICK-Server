// =============================================================================
// table.cpp - portable CSV table loader (rebuild of the binary's table.cpp).
// Reimplements the public Table_* API over std:: containers and a simple,
// correct CSV parse. No hunk allocator, no OS headers - fully cross-platform.
//
// Semantics preserved from the binary:
//   * Tables are loaded from "<root>/<name>" and cached by name (case-insensitive).
//   * Row 0 holds field names; column 0 ("Index") is a 0-based record id.
//   * Table_GetData(handle, i, "Field") -> the "Field" cell of the row Index==i.
//   * FieldToValue = atoi(string) (NULL -> 0); FieldToString copies (NULL -> "").
// =============================================================================
#include "CTable.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <map>
#include <fstream>
#include <sstream>
#include <algorithm>

namespace {

struct Cell { tvar_t var; std::string text; };

struct Table
{
    std::string name;
    std::map<std::string, size_t>            fieldIndex;   // field name -> column
    std::map<std::string, size_t>            recordIndex;  // record id  -> row
    std::vector<std::vector<Cell> >          rows;         // [row][col]
};

std::vector<Table*> g_tables;
std::string         g_root = "../Table";

bool iequals(const std::string& a, const std::string& b)
{
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); ++i)
        if (tolower((unsigned char)a[i]) != tolower((unsigned char)b[i])) return false;
    return true;
}

// Split one CSV line on commas. The XKICK tables contain no quoted/embedded
// commas (verified), so a plain split is faithful.
void splitCSV(const std::string& line, std::vector<std::string>& out)
{
    out.clear();
    std::string cur;
    for (size_t i = 0; i < line.size(); ++i)
    {
        char c = line[i];
        if (c == '\r' || c == '\n') continue;
        if (c == ',') { out.push_back(cur); cur.clear(); }
        else cur += c;
    }
    out.push_back(cur);
}

bool readFile(const std::string& path, std::string& data)
{
    std::ifstream f(path.c_str(), std::ios::binary);
    if (!f) return false;
    std::ostringstream ss;
    ss << f.rdbuf();
    data = ss.str();
    return true;
}

Table* loadTable(const char* name)
{
    std::string data;
    // Prefer "<root>/<name>"; fall back to "../Table/<name>" (binary default).
    if (!readFile(g_root + "/" + name, data) &&
        !readFile(std::string("../Table/") + name, data))
    {
        printf("Can't load %s\n", name);
        return 0;
    }

    Table* t = new Table();
    t->name = name;

    std::istringstream in(data);
    std::string line;
    std::vector<std::string> fields, cells;
    bool header = true;

    while (std::getline(in, line))
    {
        if (line.empty() || line[0] == '\r') continue;
        if (header)
        {
            splitCSV(line, fields);
            for (size_t c = 0; c < fields.size(); ++c)
                t->fieldIndex[fields[c]] = c;
            header = false;
            continue;
        }
        splitCSV(line, cells);
        if (cells.empty()) continue;

        std::vector<Cell> row(cells.size());
        for (size_t c = 0; c < cells.size(); ++c)
        {
            row[c].text       = cells[c];
            row[c].var.string = 0; // patched after the row is stored (stable address)
            row[c].var.value  = atof(cells[c].c_str());
            row[c].var.flags  = 0;
            row[c].var.modified = false;
        }
        size_t r = t->rows.size();
        t->rows.push_back(row);
        // record id = column 0 ("Index") value
        t->recordIndex[cells[0]] = r;
    }

    // Fix up the char* pointers now that rows live at stable addresses.
    for (size_t r = 0; r < t->rows.size(); ++r)
        for (size_t c = 0; c < t->rows[r].size(); ++c)
            t->rows[r][c].var.string = const_cast<char*>(t->rows[r][c].text.c_str());

    return t;
}

} // namespace

void Table_SetRoot(const char* rootPath)
{
    if (rootPath && *rootPath) g_root = rootPath;
}

int Table_FindTable(const char* name)
{
    if (name == 0 || strlen(name) < 5) return -1;

    // Handles are 1-based: 0 means "no table" (every CLoad guard treats handle 0 as
    // invalid, matching the binary). The first table loaded must therefore be 1, not 0.
    for (size_t i = 0; i < g_tables.size(); ++i)
        if (g_tables[i] && iequals(g_tables[i]->name, name))
            return (int)i + 1;

    Table* t = loadTable(name);
    if (t == 0) return -1;
    g_tables.push_back(t);
    return (int)g_tables.size();
}

tvar_t* Table_GetData(int handle, const char* recordId, const char* fieldName)
{
    if (handle < 1 || handle > (int)g_tables.size()) return 0;   // 1-based handles
    Table* t = g_tables[handle - 1];
    if (t == 0 || recordId == 0 || fieldName == 0) return 0;

    std::map<std::string, size_t>::iterator ri = t->recordIndex.find(recordId);
    if (ri == t->recordIndex.end()) return 0;
    std::map<std::string, size_t>::iterator fi = t->fieldIndex.find(fieldName);
    if (fi == t->fieldIndex.end()) return 0;

    size_t r = ri->second, c = fi->second;
    if (c >= t->rows[r].size()) return 0;
    return &t->rows[r][c].var;
}

tvar_t* Table_GetData(int handle, int recordIndex, const char* fieldName)
{
    char buf[32];
    snprintf(buf, sizeof(buf), "%d", recordIndex);
    return Table_GetData(handle, buf, fieldName);
}

int FieldToValue(const tvar_t* field)
{
    return field ? atoi(field->string) : 0;
}

// FieldsToValue - a cell holds several '|'-separated sub-values (e.g. "50|40|30|
// 20|10"); return the nIndex-th as an int (0 if the field/sub-value is absent).
int FieldsToValue(int nIndex, const tvar_t* field)
{
    if (field == 0) return 0;
    const char* p = field->string;
    for (int i = 0; i < nIndex; ++i)
    {
        p = strchr(p, '|');
        if (p == 0) return 0;
        ++p;
    }
    return atoi(p);
}

void FieldToString(char* dst, const tvar_t* field, int nLen)
{
    if (field == 0) { if (nLen > 0) dst[0] = '\0'; return; }
    snprintf(dst, nLen, "%s", field->string);
}

void Table_DeleteAll()
{
    for (size_t i = 0; i < g_tables.size(); ++i) delete g_tables[i];
    g_tables.clear();
}
