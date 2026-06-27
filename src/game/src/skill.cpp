// =============================================================================
// skill.cpp - Game SKILL shop + skill-slot handlers.
// Reconstructed byte-faithfully from XKICK_Game1 (unstripped) via Ghidra.
//
// Handlers (TCP client, registered in process.cpp):
//   PacketSkillInfo      @0807584a   (CM_SKILL_INFO 0x0836; chained from PlayerInfo)
//   PacketShopSkillList  @0807c202   (CM_SHOPSKILL_LIST 0x0C80)
//   PacketEquipSkill     @0807c28c   (CM_EQUIP_SKILL 0x0C82)
//   PacketDivestSkill    @0807c3ba   (CM_DIVEST_SKILL 0x0C83)
//   PacketBuySkill       @0807c4ca   (CM_BUY_SKILL 0x0C84)
//   PacketSkillSlot      @0807cda6   (CM_SKILL_SLOTT 0x1217)
// internal sender:
//   PacketUpdateSkill    @0807c264   (CM_UPDATE_SKILL 0x0C81; called by BuySkill)
//
// CPlayer skill logic (binary CPlayer::* members) is reconstructed here as free
// CPlayer_* helpers operating on m_vSkillList (CPlayer+0x4b8), mirroring the
// item/card module convention:
//   CPlayer::EquipSkill         @0808e1bc   CPlayer::DivestSkill        @0808e2e4
//   CPlayer::BuySkill           @0808e41c   CPlayer::GetSkillPointer    @08090f0e
//   CPlayer::GetEquipSkillCount @080911bc   CPlayer::GetShopSkillList   @0808de98
//   CPlayer::GetShopSkillTotalPage @0808e090
//   GetSkillCash @08096e9c  GetSkillPoint @08096f86  (Skill table price formula)
//
// The static skill table (binary std::map<int,CSkillTable*> @0x80def70, built by
// CLoad::LoadSkillTable from Table_Skill.csv) is queried here through the rebuild's
// portable CSV handle API (g_Load.GetSkillTable() + Table_GetData by column name).
// The price formula is the binary's: round(Discount * Price / 100); the "Sell"
// column is the shop/restrict flag (1 = cash-shop, 2 = point-shop, !=0 = listed;
// Sell==2 forbids cash, Sell==1 forbids point) and "Level" gates the buy.
//
// Portable: standard C++ only; all packet/player access is through struct members.
// =============================================================================
#include "Main.h"
#include "Sql.h"
#include "GameLoad.h"
#include "Domain.h"
#include "LogManager.h"
#include <cstring>
#include <cmath>

// g_Sql / g_Load are global OBJECTS (Global.h, pulled in via Main.h).

// money snapshot/commit owned by the player module (forward-declared; stubs.cpp).
int CPlayer_GetMoneyData(CPlayer*);    // _ZN7CPlayer12GetMoneyDataEv
int CPlayer_PutMoneyData(CPlayer*);    // _ZN7CPlayer12PutMoneyDataEv

// internal sender, used by CPlayer_BuySkill (not a dispatch handler).
static void PacketUpdateSkill(CPlayer* pPlayer, CSCUpdateSkill* pPacket);

// =============================================================================
// Skill table (CSV) helpers -- query Table_Skill.csv by record index + column.
// =============================================================================
static int SkF(int idx, const char* col)
{
    return FieldToValue(Table_GetData(g_Load.GetSkillTable(), idx, col));
}
// locate the CSV record index whose "Code" == nCode (-1 if none).
static int SkRowOfCode(int nCode)
{
    int h = g_Load.GetSkillTable();
    for (int i = 0; ; ++i)
    {
        tvar_t* code = Table_GetData(h, i, "Code");
        if (code == 0) return -1;
        if (FieldToValue(code) == nCode) return i;
    }
}

// GetSkillCash @08096e9c : cash price of a skill code. -1 unknown, -2 cash-forbidden.
int GetSkillCash(int nCode, int /*nKind*/)
{
    int row = SkRowOfCode(nCode);
    if (row < 0) { LOGOUT_ERROR("GetSkillCash: unknown skill code(%d)\n", nCode); return -1; }
    if (SkF(row, "Sell") == 2) return -2;
    return (int)std::lround((double)(SkF(row, "Discount") * SkF(row, "Cash")) / 100.0);
}
// GetSkillPoint @08096f86 : point price of a skill code. -1 unknown, -2 point-forbidden.
int GetSkillPoint(int nCode, int /*nKind*/)
{
    int row = SkRowOfCode(nCode);
    if (row < 0) { LOGOUT_ERROR("GetSkillPoint: unknown skill code(%d)\n", nCode); return -1; }
    if (SkF(row, "Sell") == 1) return -2;
    return (int)std::lround((double)(SkF(row, "Discount") * SkF(row, "Point")) / 100.0);
}

// =============================================================================
// CPlayer skill helpers (binary CPlayer:: members)
// =============================================================================

// CPlayer::GetEquipSkillCount @080911bc : count equipped (m_nEquipKind==1) skills.
int CPlayer_GetEquipSkillCount(CPlayer* pPlayer)
{
    int n = 0;
    for (size_t i = 0; i < pPlayer->m_vSkillList.size(); ++i)
    {
        CSkill* s = pPlayer->m_vSkillList[i];
        if (s && s->m_nState != 0 && s->m_nEquipKind == 1)
            ++n;
    }
    return n;
}

// CPlayer::GetSkillPointer @08090f0e : skill in m_vSkillList by skill_seq (0 if none).
CSkill* CPlayer_GetSkillPointer(CPlayer* pPlayer, int nSkillSeq)
{
    for (size_t i = 0; i < pPlayer->m_vSkillList.size(); ++i)
    {
        CSkill* s = pPlayer->m_vSkillList[i];
        if (s && s->m_nSkillSeq == nSkillSeq)
            return s;
    }
    return 0;
}

// CPlayer::EquipSkill @0808e1bc : wear a learned skill (capped by skill points).
//   -3 over the equip cap, -2 skill not owned, else the equipped flag (1).
int CPlayer_EquipSkill(CPlayer* pPlayer, int nSkillSeq)
{
    if (CPlayer_GetEquipSkillCount(pPlayer) >= (int)pPlayer->m_cLevel.m_nSkill)
        return -3;
    CSkill* s = 0;
    for (size_t i = 0; i < pPlayer->m_vSkillList.size(); ++i)
    {
        CSkill* c = pPlayer->m_vSkillList[i];
        if (c && c->m_nState != 0 && c->m_nSkillSeq == nSkillSeq) { s = c; break; }
    }
    if (s == 0)
        return -2;
    s->m_nEquipKind = 1;
    s->ChangeSkillState();            // m_nState = 2 (dirty)
    return s->m_nEquipKind;
}

// CPlayer::DivestSkill @0808e2e4 : take off a skill.  nSkillSeq==0 -> first worn;
//   else by seq.  -2 seq not found (-1 null entry in the binary -> impossible here).
int CPlayer_DivestSkill(CPlayer* pPlayer, int nSkillSeq)
{
    for (size_t i = 0; i < pPlayer->m_vSkillList.size(); ++i)
    {
        CSkill* s = pPlayer->m_vSkillList[i];
        if (s == 0 || s->m_nState == 0)
            continue;
        if (nSkillSeq == 0)
        {
            s->m_nEquipKind = 0;
            s->ChangeSkillState();
            return 0;
        }
        if (s->m_nSkillSeq == nSkillSeq)
        {
            s->m_nEquipKind = 0;
            s->ChangeSkillState();
            return s->m_nEquipKind;   // 0
        }
    }
    return (nSkillSeq != 0) ? -2 : 0;
}

// CPlayer::GetShopSkillTotalPage @0808e090 : #pages (6/page) of buyable skills that
// match the position filter.
static int CPlayer_GetShopSkillTotalPage(int nPosition)
{
    int h = g_Load.GetSkillTable();
    int n = 0;
    for (int i = 0; ; ++i)
    {
        tvar_t* code = Table_GetData(h, i, "Code");
        if (code == 0) break;
        if (FieldToValue(Table_GetData(h, i, "Sell")) == 0) continue;
        int pos = FieldToValue(Table_GetData(h, i, "Position"));
        if (pos == 0 || pos == nPosition || pos == (nPosition / 10) * 10)
            ++n;
    }
    return (n % 6 == 0) ? n / 6 : n / 6 + 1;
}

// CPlayer::GetShopSkillList @0808de98 : fill one page (<=6) of the skill shop.
void CPlayer_GetShopSkillList(CPlayer* pPlayer, int nType, int nPosition,
                              int nPage, CSCShopSkillList* pPacket)
{
    for (int i = 0; i < 6; ++i)
        pPacket->m_cShopSkillData[i].m_nCode = 0;

    if (nType == 0)
        nPosition = (int)pPlayer->m_nPosition;           // CPlayer m_nPosition

    int nTotal = CPlayer_GetShopSkillTotalPage(nPosition);
    if (nPage < 0)              nPage = nTotal - 1;
    else if (nTotal - 1 < nPage) nPage = 0;

    int nSkip = nPage * 6;
    int nSlot = 0, nSeen = 0;
    int h = g_Load.GetSkillTable();
    for (int i = 0; ; ++i)
    {
        tvar_t* code = Table_GetData(h, i, "Code");
        if (code == 0) break;
        if (FieldToValue(Table_GetData(h, i, "Sell")) == 0) continue;
        if (nSlot > 5) break;
        int pos = FieldToValue(Table_GetData(h, i, "Position"));
        if (!(pos == 0 || pos == nPosition || pos == (nPosition / 10) * 10))
            continue;
        bool bTake = (nSkip <= nSeen);
        ++nSeen;
        if (bTake)
        {
            pPacket->m_cShopSkillData[nSlot].m_nCode     = FieldToValue(code);
            pPacket->m_cShopSkillData[nSlot].m_nDiscount = 0;
            ++nSlot;
        }
    }
    if (pPacket->m_cShopSkillData[0].m_nCode == 0) pPacket->m_nCurrentPage = 0;
    else                                           pPacket->m_nCurrentPage = (short)nPage;
    pPacket->m_nTotalPage = (short)nTotal;
    pPacket->m_nType      = (char)nType;
    pPacket->m_nPosition  = (char)nPosition;
}

// CPlayer::BuySkill @0808e41c : learn a new skill (cash/point), persist, equip,
// log the sale, push the update. Returns the new skill's equip flag, or a reason:
//   -4 unknown code, -5 already owned / under-level, -1 bad pay kind / alloc fail,
//   -2 db insert fail, cash: -11/-12/-13/-14, point: -21/-22/-23/-24.
int CPlayer_BuySkill(CPlayer* pPlayer, CCSBuySkill* pBuy)
{
    int   nCode    = pBuy->m_nCode;
    char  nBuyKind = pBuy->m_nBuyKind;
    int   nPrice   = pBuy->m_nPrice;

    int row = SkRowOfCode(nCode);
    if (row < 0) { LOGOUT_ERROR("BuySkill: unknown skill code(%d)\n", nCode); return -4; }

    // already own a skill of this code?
    for (size_t i = 0; i < pPlayer->m_vSkillList.size(); ++i)
    {
        CSkill* s = pPlayer->m_vSkillList[i];
        if (s && s->m_nState != 0 && s->m_nCode == nCode)
            return -5;
    }
    // level requirement (Table "Level" column).
    if ((char)pPlayer->m_cLevel.m_nLevel < (char)SkF(row, "Level"))
        return -5;

    int nCost;
    if (nBuyKind == 1)             // pay with cash
    {
        nCost = GetSkillCash(nCode, 1);
        if (nCost < 0)                       return -11;
        if (nPrice != nCost)                 return -12;
        if (CPlayer_GetMoneyData(pPlayer) < 0) return -13;
        if (pPlayer->m_cMoney.m_nCash < nCost) return -14;
        pPlayer->m_cMoney.m_nCash -= nCost;
        if (CPlayer_PutMoneyData(pPlayer) < 0) return -13;
    }
    else if (nBuyKind == 2)        // pay with point
    {
        nCost = GetSkillPoint(nCode, 1);
        if (nCost < 0)                       return -21;
        if (nPrice != nCost)                 return -22;
        if (CPlayer_GetMoneyData(pPlayer) < 0) return -23;
        if (pPlayer->m_cMoney.m_nPoint < nCost) return -24;
        pPlayer->m_cMoney.m_nPoint -= nCost;
        if (CPlayer_PutMoneyData(pPlayer) < 0) return -23;
    }
    else
        return -1;

    CSkill* pSkill = new CSkill();
    if (pSkill == 0)
        return -1;
    pSkill->m_nState     = 1;
    pSkill->m_nSkillSeq  = 0;
    pSkill->m_nCode      = nCode;
    pSkill->m_nEquipKind = 0;
    pSkill->m_nLevel     = 1;
    pSkill->m_nSkillSeq  = g_Sql.InsertSkillField(pPlayer, pSkill);
    if (pSkill->m_nSkillSeq < 0)
    {
        delete pSkill;
        return -2;
    }
    pPlayer->m_vSkillList.push_back(pSkill);
    CPlayer_EquipSkill(pPlayer, pSkill->m_nSkillSeq);

    // sale log : {seq, code, kind 2(skill), buyKind, sale 1, price, amount 1}
    CSale sale;
    memset(&sale, 0, sizeof(sale));
    sale.m_nObjectSeq  = pSkill->m_nSkillSeq;
    sale.m_nObjectCode = pSkill->m_nCode;
    sale.m_nObjectKind = 2;
    sale.m_nBuyKind    = nBuyKind;
    sale.m_nSaleKind   = 1;
    sale.m_nPrice      = nPrice;
    sale.m_nAmount     = 1;
    g_Sql.InsertSaleLog(pPlayer, &sale);

    CSCUpdateSkill uc;
    uc.m_nCommand    = CM_UPDATE_SKILL;
    uc.m_nBodySize   = 0xc;
    uc.m_nUpdateKind = 1;                 // 1 = added (kind 3 = upgrade; unused here)
    uc.m_nSkillSeq   = pSkill->m_nSkillSeq;
    uc.m_nCode       = pSkill->m_nCode;
    uc.m_nEquipKind  = pSkill->m_nEquipKind;
    uc.m_nLevel      = pSkill->m_nLevel;
    PacketUpdateSkill(pPlayer, &uc);

    return pSkill->m_nEquipKind;
}

// =============================================================================
// PacketSkillInfo @0807584a (CM_SKILL_INFO 0x0836) - chained from PacketPlayerInfo.
// Packs up to 50 (0x32) 10-byte rows {seq, code, equip, level} per page.
// =============================================================================
void PacketSkillInfo(CPlayer* pPlayer)
{
    CSCSkillInfo sc;
    sc.m_nCommand = CM_SKILL_INFO;
    int   nCount = 0;

    for (size_t i = 0; i < pPlayer->m_vSkillList.size(); ++i)
    {
        CSkill* s = pPlayer->m_vSkillList[i];
        if (s == 0 || s->m_nState == 0)
            continue;
        CSkillInfo& r = sc.m_cSkillInfo[nCount];
        r.m_nSkillSeq  = s->m_nSkillSeq;
        r.m_nCode      = s->m_nCode;
        r.m_nEquipKind = s->m_nEquipKind;
        r.m_nLevel     = s->m_nLevel;
        if (++nCount == 0x32)
        {
            sc.m_nBodySize = nCount * 10 + 2;
            sc.m_nCount    = 0x32;
            sc.m_nResponse = 0;
            SendPlayer(pPlayer, &sc, 0);
            nCount = 0;
        }
    }
    if (nCount != 0)
    {
        sc.m_nBodySize = nCount * 10 + 2;
        sc.m_nCount    = (char)nCount;
        sc.m_nResponse = 0;
        SendPlayer(pPlayer, &sc, 0);
    }
}

// =============================================================================
// PacketShopSkillList @0807c202 (CM_SHOPSKILL_LIST 0x0C80)
// =============================================================================
void PacketShopSkillList(CPlayer* pPlayer, CHeadPacket* pPacket)
{
    CSCShopSkillList sc;
    sc.m_nCommand  = CM_SHOPSKILL_LIST;
    sc.m_nBodySize = 0x25;
    CCSShopSkillList* cs = (CCSShopSkillList*)pPacket;
    CPlayer_GetShopSkillList(pPlayer,
                             (int)cs->m_nType,
                             (int)cs->m_nPosition,
                             (int)cs->m_nPage,
                             &sc);
    sc.m_nResponse = 0;
    SendPlayer(pPlayer, &sc, 0);
}

// =============================================================================
// PacketUpdateSkill @0807c264 (CM_UPDATE_SKILL 0x0C81) - internal sender.
// =============================================================================
static void PacketUpdateSkill(CPlayer* pPlayer, CSCUpdateSkill* pPacket)
{
    pPacket->m_nResponse = 0;
    SendPlayer(pPlayer, pPacket, (int)pPacket->m_nResponse);
}

// =============================================================================
// PacketEquipSkill @0807c28c (CM_EQUIP_SKILL 0x0C82)
// =============================================================================
void PacketEquipSkill(CPlayer* pPlayer, CHeadPacket* pPacket)
{
    CSCEquipSkill sc;
    sc.m_nCommand  = CM_EQUIP_SKILL;
    sc.m_nBodySize = 6;
    int nSkillSeq = ((CCSEquipSkill*)pPacket)->m_nSkillSeq;
    int nRet = CPlayer_EquipSkill(pPlayer, nSkillSeq);
    if (nRet < 0)
    {
        char nResp = (char)nRet;
        if (nRet == -2)      nResp = (char)0xf4;          // -12
        else if (nRet == -3) nResp = (char)0xf3;          // -13
        else if (nRet == -1) { nResp = (char)0xf5;        // -11
            LOGOUT_ERROR("PacketEquipSkill: EquipSkill=-1 seq(%d)\n", pPlayer->m_nPlayerSeq); }
        sc.m_nResponse = nResp;
        SendPlayer(pPlayer, &sc, (int)nResp);
        return;
    }
    sc.m_nSkillSeq  = nSkillSeq;
    sc.m_nEquipKind = 1;
    sc.m_nResponse  = 0;
    SendPlayer(pPlayer, &sc, 0);
}

// =============================================================================
// PacketDivestSkill @0807c3ba (CM_DIVEST_SKILL 0x0C83)
// =============================================================================
void PacketDivestSkill(CPlayer* pPlayer, CHeadPacket* pPacket)
{
    CSCDivestSkill sc;
    sc.m_nCommand  = CM_DIVEST_SKILL;
    sc.m_nBodySize = 6;
    int nSkillSeq = ((CCSDivestSkill*)pPacket)->m_nSkillSeq;
    int nRet = CPlayer_DivestSkill(pPlayer, nSkillSeq);
    if (nRet < 0)
    {
        char nResp = (char)nRet;
        if (nRet == -2)      nResp = (char)0xf4;          // -12
        else if (nRet == -1) { nResp = (char)0xf5;        // -11
            LOGOUT_ERROR("PacketDivestSkill: DivestSkill=-1 seq(%d)\n", pPlayer->m_nPlayerSeq); }
        sc.m_nResponse = nResp;
        SendPlayer(pPlayer, &sc, (int)nResp);
        return;
    }
    sc.m_nSkillSeq  = nSkillSeq;
    sc.m_nEquipKind = 0;
    sc.m_nResponse  = 0;
    SendPlayer(pPlayer, &sc, 0);
}

// =============================================================================
// PacketBuySkill @0807c4ca (CM_BUY_SKILL 0x0C84)
// =============================================================================
void PacketBuySkill(CPlayer* pPlayer, CHeadPacket* pPacket)
{
    CSCBuySkill sc;
    sc.m_nCommand  = CM_BUY_SKILL;
    sc.m_nBodySize = 0x12;
    int  nRet  = CPlayer_BuySkill(pPlayer, (CCSBuySkill*)pPacket);
    char nResp = (char)nRet;
    if (nRet < 0)
    {
        if ((unsigned)(nRet + 2) < 2)         // -1 or -2
            LOGOUT_ERROR("PacketBuySkill: BuySkill=%d seq(%d)\n", nResp, pPlayer->m_nPlayerSeq);
        sc.m_nResponse = nResp;
        SendPlayer(pPlayer, &sc, (int)nResp);
        return;
    }
    sc.m_cMoney.m_nCash      = pPlayer->m_cMoney.m_nCash;
    sc.m_cMoney.m_nPoint     = pPlayer->m_cMoney.m_nPoint;
    sc.m_cMoney.m_nCredit    = pPlayer->m_cMoney.m_nCredit;
    sc.m_cMoney.m_nClubPoint = pPlayer->m_cMoney.m_nClubPoint;
    sc.m_nEquipKind = nResp;                  // the equipped flag
    sc.m_nResponse  = 0;
    SendPlayer(pPlayer, &sc, 0);
}

// =============================================================================
// PacketSkillSlot @0807cda6 (CM_SKILL_SLOTT 0x1217)
// Consume a skill-slot item: +5 skill points (CPlayer+0xce), persist level field,
// then echo the refreshed level/exp/faculty block.
// =============================================================================
void PacketSkillSlot(CPlayer* pPlayer, CHeadPacket* pPacket)
{
    CSCSkillSlot sc;
    if (pPlayer == 0 || pPacket == 0)
        return;
    int nItemSeq = ((CCSSkillSlot*)pPacket)->m_nItemSeq;
    CItem* pItem = pPlayer->GetItemPointer(nItemSeq);
    if (pItem == 0)
    {
        sc.m_nResponse = (char)0xfd;          // -3
        SendPlayer(pPlayer, &sc, -3);
        return;
    }
    pPlayer->m_cLevel.m_nSkill += 5;          // +5 skill points
    g_Sql.UpdateLevelField(pPlayer);
    pItem->m_nState = 0;                       // consume (CItem m_nState)

    sc.m_nItemSeq  = nItemSeq;
    sc.m_nLevel    = pPlayer->m_cLevel.m_nLevel;   // wire field is int; level fits in the short (high bytes are struct pad)
    sc.m_nExp      = pPlayer->m_cLevel.m_nExp;
    // wire packs faculty (low 16) + skill points (high 16), matching the binary's
    // 4-byte read at CPlayer+0xcc (m_nFaculty short @+0, m_nSkill short @+2).
    sc.m_nFaculty  = (int)((unsigned short)pPlayer->m_cLevel.m_nFaculty
                           | ((unsigned int)(unsigned short)pPlayer->m_cLevel.m_nSkill << 16));
    sc.m_nResponse = 0;
    SendPlayer(pPlayer, &sc, 0);
}
