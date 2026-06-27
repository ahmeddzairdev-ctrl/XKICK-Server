// config.cpp - CConfig::InitConfig + GetValue (reconstructed from XKICK_Game1
// CConfig::InitConfig @0804c6f0). Parses ../config.ini. Each line is whitespace-
// stripped; '//'-comments and blank lines are skipped. Recognised keys fill the
// matching field. A line equal to this deployment's working-directory LEAF name
// (e.g. "game") - or containing its full absolute path, for backward compat - is
// a section header; the NEXT line in that section provides this server's ServerCode.
// Company selects the table path: 100=Korean, 200=Dubai_Arb, 900/other=Default_Eng.
//
// Differs from Certify only in the field set (Game has no "Version" key; ports
// come from the DB via CSql::InitServer). Portable: std::filesystem for cwd.
#include "Main.h"
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>

// Extract the substring after '=' into tmp (original GetValue).
void GetValue(char* tmp, const char* str)
{
    tmp[0] = '\0';
    int i = 0;
    while (str[i] != '\0')
    {
        if (str[i] == '=') { snprintf(tmp, 100, "%s", str + i + 1); return; }
        ++i;
    }
}

// Strip spaces, tabs and newlines out of a line in place.
static void StripWhitespace(char* line)
{
    int i = 0;
    while (line[i] != '\0')
    {
        if (line[i] == ' ' || line[i] == '\t' || line[i] == '\n')
        {
            if (i > 100) break;
            snprintf(line + i, 100, "%s", line + i + 1);
        }
        else ++i;
    }
}

CConfig::CConfig()  { memset((void*)this, 0, sizeof(CConfig)); }
CConfig::~CConfig() {}

bool CConfig::InitConfig()
{
    // Identify this deployment's config section by the LEAF name of the working
    // directory (portable): cwd ".../run/game" -> "game". A full absolute path is
    // still accepted too, for backward compatibility with old headers.
    char cwd[260];
    char leaf[100];
    memset(cwd, 0, sizeof(cwd));
    memset(leaf, 0, sizeof(leaf));
    snprintf(cwd,  sizeof(cwd),  "%s", std::filesystem::current_path().string().c_str());
    snprintf(leaf, sizeof(leaf), "%s", std::filesystem::current_path().filename().string().c_str());

    std::ifstream fs("../config.ini");
    if (!fs.is_open()) return false;

    char line[100];
    char val[64];
    std::string raw;
    while (std::getline(fs, raw))
    {
        snprintf(line, sizeof(line), "%s", raw.c_str());
        memset(val, 0, sizeof(val));
        StripWhitespace(line);
        if ((line[0] == '/' && line[1] == '/') || line[0] == '\0') continue;

        if      (strstr(line, "DB_IP"))     { GetValue(val, line); snprintf(m_sDBIP,     0x14, "%s", val); }
        else if (strstr(line, "DB_User"))   { GetValue(val, line); snprintf(m_sDBUser,   0x14, "%s", val); }
        else if (strstr(line, "DB_Pass"))   { GetValue(val, line); snprintf(m_sDBPass,   0x32, "%s", val); }
        else if (strstr(line, "DB_Main"))   { GetValue(val, line); snprintf(m_sDBMain,   0x14, "%s", val); }
        else if (strstr(line, "DB_Log"))    { GetValue(val, line); snprintf(m_sDBLog,    0x14, "%s", val); }
        else if (strstr(line, "DB_Sample")) { GetValue(val, line); snprintf(m_sDBSample, 0x14, "%s", val); }
        else if (strstr(line, "Charset"))   { GetValue(val, line); snprintf(m_sCharset,  0x14, "%s", val); }
        else if (strstr(line, "Company"))
        {
            GetValue(val, line);
            m_nCompany = (short)atoi(val);
            if      (m_nCompany == 100) snprintf(m_sPath, sizeof(m_sPath), "Korean");
            else if (m_nCompany == 200) snprintf(m_sPath, sizeof(m_sPath), "Dubai_Arb");
            else                        snprintf(m_sPath, sizeof(m_sPath), "Default_Eng");
        }
        else if (strstr(line, "Nation"))    { GetValue(val, line); m_nNation  = (short)atoi(val); }
        else if (strstr(line, "System"))    { GetValue(val, line); m_nSystem  = (short)atoi(val); }
        else if (strstr(line, "HackCode"))  { GetValue(val, line); m_nHackCode = atoi(val); }
        else if (strcmp(line, leaf) == 0 || (cwd[0] && strstr(line, cwd)))
        {
            // Section header for this deployment dir (leaf name, or full path):
            // the next line has ServerCode.
            if (!std::getline(fs, raw)) break;
            snprintf(line, sizeof(line), "%s", raw.c_str());
            StripWhitespace(line);
            if (((line[0] != '/') || (line[1] != '/')) && line[0] != '\0' &&
                strstr(line, "ServerCode"))
            {
                GetValue(val, line);
                m_nServerCode = (short)atoi(val);
            }
        }
    }
    return true;
}
