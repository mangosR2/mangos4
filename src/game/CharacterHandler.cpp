/*
 * Copyright (C) 2005-2012 MaNGOS <http://getmangos.com/>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "AccountMgr.h"
#include "Common.h"
#include "Database/DatabaseEnv.h"
#include "WorldPacket.h"
#include "SharedDefines.h"
#include "WorldSession.h"
#include "Opcodes.h"
#include "Log.h"
#include "World.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "Guild.h"
#include "GuildMgr.h"
#include "UpdateMask.h"
#include "Auth/md5.h"
#include "ObjectAccessor.h"
#include "Group.h"
#include "Database/DatabaseImpl.h"
#include "PlayerDump.h"
#include "SocialMgr.h"
#include "Util.h"
#include "ArenaTeam.h"
#include "Language.h"
#include "SpellMgr.h"
#include "Calendar.h"

// Playerbot mod:
#include "playerbot/PlayerbotMgr.h"

// config option SkipCinematics supported values
enum CinematicsSkipMode
{
    CINEMATICS_SKIP_NONE      = 0,
    CINEMATICS_SKIP_SAME_RACE = 1,
    CINEMATICS_SKIP_ALL       = 2,
};

class LoginQueryHolder : public SqlQueryHolder
{
    private:
        uint32 m_accountId;
        ObjectGuid m_guid;
    public:
        LoginQueryHolder(uint32 accountId, ObjectGuid guid)
            : m_accountId(accountId), m_guid(guid) { }
        ObjectGuid GetGuid() const { return m_guid; }
        uint32 GetAccountId() const { return m_accountId; }
        bool Initialize();
};

bool LoginQueryHolder::Initialize()
{
    SetSize(MAX_PLAYER_LOGIN_QUERY);

    bool res = true;

    // NOTE: all fields in `characters` must be read to prevent lost character data at next save in case wrong DB structure.
    // !!! NOTE: including unused `zone`,`online`

    res &= SetPQuery(PLAYER_LOGIN_QUERY_LOADFROM,            "SELECT guid, account, name, race, class, gender, level, xp, money, playerBytes, playerBytes2, playerFlags,"
        "position_x, position_y, position_z, map, orientation, taximask, cinematic, totaltime, leveltime, rest_bonus, logout_time, is_logout_resting, resettalents_cost,"
        "resettalents_time, primary_trees, trans_x, trans_y, trans_z, trans_o, transguid, extra_flags, stable_slots, at_login, zone, online, death_expire_time, taxi_path, dungeon_difficulty,"
        "totalKills, todayKills, yesterdayKills, chosenTitle, watchedFaction, drunk,"
        "health, power1, power2, power3, power4, power5, specCount, activeSpec, exploredZones, equipmentCache, knownTitles, actionBars, grantableLevels, slot FROM characters WHERE guid = '%u'", m_guid.GetCounter());

    res &= SetPQuery(PLAYER_LOGIN_QUERY_LOADGROUP,           "SELECT groupId FROM group_member WHERE memberGuid ='%u'", m_guid.GetCounter());
    res &= SetPQuery(PLAYER_LOGIN_QUERY_LOADBOUNDINSTANCES,  "SELECT id, permanent, map, difficulty, extend, resettime FROM character_instance LEFT JOIN instance ON instance = id WHERE guid = '%u'", m_guid.GetCounter());
    res &= SetPQuery(PLAYER_LOGIN_QUERY_LOADAURAS,           "SELECT caster_guid,item_guid,spell,stackcount,remaincharges,"
        "basepoints0,basepoints1,basepoints2,basepoints3,basepoints4,basepoints5,basepoints6,basepoints7,basepoints8,basepoints9,basepoints10,basepoints11,basepoints12,basepoints13,basepoints14,basepoints15,basepoints16,basepoints17,basepoints18,basepoints19,basepoints20,"
        "periodictime0,periodictime1,periodictime2,periodictime3,periodictime4,periodictime5,periodictime6,periodictime7,periodictime8,periodictime9,periodictime10,periodictime11,periodictime12,periodictime13,periodictime14,periodictime15,periodictime16,periodictime17,periodictime18,periodictime19,periodictime20,"
        "maxduration,remaintime,effIndexMask FROM character_aura WHERE guid = '%u'", m_guid.GetCounter());
    res &= SetPQuery(PLAYER_LOGIN_QUERY_LOADSPELLS,          "SELECT spell,active,disabled FROM character_spell WHERE guid = '%u'", m_guid.GetCounter());
    res &= SetPQuery(PLAYER_LOGIN_QUERY_LOADQUESTSTATUS,     "SELECT quest,status,rewarded,explored,timer,mobcount1,mobcount2,mobcount3,mobcount4,itemcount1,itemcount2,itemcount3,itemcount4,itemcount5,itemcount6 FROM character_queststatus WHERE guid = '%u'", m_guid.GetCounter());
    res &= SetPQuery(PLAYER_LOGIN_QUERY_LOADDAILYQUESTSTATUS, "SELECT quest FROM character_queststatus_daily WHERE guid = '%u'", m_guid.GetCounter());
    res &= SetPQuery(PLAYER_LOGIN_QUERY_LOADWEEKLYQUESTSTATUS, "SELECT quest FROM character_queststatus_weekly WHERE guid = '%u'", m_guid.GetCounter());
    res &= SetPQuery(PLAYER_LOGIN_QUERY_LOADMONTHLYQUESTSTATUS, "SELECT quest FROM character_queststatus_monthly WHERE guid = '%u'", m_guid.GetCounter());
    res &= SetPQuery(PLAYER_LOGIN_QUERY_LOADREPUTATION,      "SELECT faction,standing,flags FROM character_reputation WHERE guid = '%u'", m_guid.GetCounter());
    res &= SetPQuery(PLAYER_LOGIN_QUERY_LOADINVENTORY,       "SELECT data,text,bag,slot,item,item_template FROM character_inventory JOIN item_instance ON character_inventory.item = item_instance.guid WHERE character_inventory.guid = '%u' ORDER BY bag,slot", m_guid.GetCounter());
    res &= SetPQuery(PLAYER_LOGIN_QUERY_LOADITEMLOOT,        "SELECT guid,itemid,amount,suffix,property FROM item_loot WHERE owner_guid = '%u'", m_guid.GetCounter());
    res &= SetPQuery(PLAYER_LOGIN_QUERY_LOADACTIONS,         "SELECT spec,button,action,type FROM character_action WHERE guid = '%u' ORDER BY button", m_guid.GetCounter());
    res &= SetPQuery(PLAYER_LOGIN_QUERY_LOADSOCIALLIST,      "SELECT friend,flags,note FROM character_social WHERE guid = '%u' LIMIT 255", m_guid.GetCounter());
    res &= SetPQuery(PLAYER_LOGIN_QUERY_LOADHOMEBIND,        "SELECT map,zone,position_x,position_y,position_z FROM character_homebind WHERE guid = '%u'", m_guid.GetCounter());
    res &= SetPQuery(PLAYER_LOGIN_QUERY_LOADSPELLCOOLDOWNS,  "SELECT spell,item,time FROM character_spell_cooldown WHERE guid = '%u'", m_guid.GetCounter());
    if (sWorld.getConfig(CONFIG_BOOL_DECLINED_NAMES_USED))
        res &= SetPQuery(PLAYER_LOGIN_QUERY_LOADDECLINEDNAMES,   "SELECT genitive, dative, accusative, instrumental, prepositional FROM character_declinedname WHERE guid = '%u'", m_guid.GetCounter());
    // in other case still be dummy query
    res &= SetPQuery(PLAYER_LOGIN_QUERY_LOADGUILD,           "SELECT guildid,rank FROM guild_member WHERE guid = '%u'", m_guid.GetCounter());
    res &= SetPQuery(PLAYER_LOGIN_QUERY_LOADARENAINFO,       "SELECT arenateamid, played_week, played_season, wons_season, personal_rating FROM arena_team_member WHERE guid='%u'", m_guid.GetCounter());
    res &= SetPQuery(PLAYER_LOGIN_QUERY_LOADACHIEVEMENTS,    "SELECT achievement, date FROM character_achievement WHERE guid = '%u'", m_guid.GetCounter());
    res &= SetPQuery(PLAYER_LOGIN_QUERY_LOADCRITERIAPROGRESS, "SELECT criteria, counter, date FROM character_achievement_progress WHERE guid = '%u'", m_guid.GetCounter());
    res &= SetPQuery(PLAYER_LOGIN_QUERY_LOADEQUIPMENTSETS,   "SELECT setguid, setindex, name, iconname, ignore_mask, item0, item1, item2, item3, item4, item5, item6, item7, item8, item9, item10, item11, item12, item13, item14, item15, item16, item17, item18 FROM character_equipmentsets WHERE guid = '%u' ORDER BY setindex", m_guid.GetCounter());
    res &= SetPQuery(PLAYER_LOGIN_QUERY_LOADBGDATA,          "SELECT instance_id, team, join_x, join_y, join_z, join_o, join_map, taxi_start, taxi_end, mount_spell FROM character_battleground_data WHERE guid = '%u'", m_guid.GetCounter());
    res &= SetPQuery(PLAYER_LOGIN_QUERY_LOADACCOUNTDATA,     "SELECT type, time, data FROM character_account_data WHERE guid='%u'", m_guid.GetCounter());
    res &= SetPQuery(PLAYER_LOGIN_QUERY_LOADTALENTS,         "SELECT talent_id, current_rank, spec FROM character_talent WHERE guid = '%u'", m_guid.GetCounter());
    res &= SetPQuery(PLAYER_LOGIN_QUERY_LOADSKILLS,          "SELECT skill, value, max FROM character_skills WHERE guid = '%u'", m_guid.GetCounter());
    res &= SetPQuery(PLAYER_LOGIN_QUERY_LOADGLYPHS,          "SELECT spec, slot, glyph FROM character_glyphs WHERE guid='%u'", m_guid.GetCounter());
    res &= SetPQuery(PLAYER_LOGIN_QUERY_LOADMAILS,           "SELECT id,messageType,sender,receiver,subject,body,expire_time,deliver_time,money,cod,checked,stationery,mailTemplateId,has_items FROM mail WHERE receiver = '%u' ORDER BY id DESC", m_guid.GetCounter());
    res &= SetPQuery(PLAYER_LOGIN_QUERY_LOADMAILEDITEMS,     "SELECT data, text, mail_id, item_guid, item_template FROM mail_items JOIN item_instance ON item_guid = guid WHERE receiver = '%u'", m_guid.GetCounter());
    res &= SetPQuery(PLAYER_LOGIN_QUERY_LOADRANDOMBG,        "SELECT guid FROM character_battleground_random WHERE guid = '%u'", m_guid.GetCounter());
    res &= SetPQuery(PLAYER_LOGIN_QUERY_LOADCURRENCIES,      "SELECT id, totalCount, weekCount, seasonCount, flags FROM character_currencies WHERE guid = '%u'", m_guid.GetCounter());
    res &= SetPQuery(PLAYER_LOGIN_QUERY_LOAD_CUF_PROFILES,   "SELECT id, name, frameHeight, frameWidth, sortBy, healthText, boolOptions, unk146, unk147, unk148, unk150, unk152, unk154 FROM character_cuf_profiles WHERE guid = '%u'", m_guid.GetCounter());

    return res;
}

// don't call WorldSession directly
// it may get deleted before the query callbacks get executed
// instead pass an account id to this handler
class CharacterHandler
{
    public:
        void HandleCharEnumCallback(QueryResult* result, uint32 account)
        {
            WorldSession* session = sWorld.FindSession(account);
            if (!session)
            {
                delete result;
                return;
            }
            session->HandleCharEnum(result);
        }
        void HandlePlayerLoginCallback(QueryResult* /*dummy*/, SqlQueryHolder* holder)
        {
            if (!holder) return;
            WorldSession* session = sWorld.FindSession(((LoginQueryHolder*)holder)->GetAccountId());
            if (!session)
            {
                delete holder;
                return;
            }
            session->HandlePlayerLogin((LoginQueryHolder*)holder);
        }
        // Playerbot mod: is different from the normal HandlePlayerLoginCallback in that it
        // sets up the bot's world session and also stores the pointer to the bot player in the master's
        // world session m_playerBots map
        void HandlePlayerBotLoginCallback(QueryResult * /*dummy*/, SqlQueryHolder * holder, uint32 masterId)
        {
            if (!holder)
                return;

            LoginQueryHolder* lqh = (LoginQueryHolder*) holder;

            WorldSession* masterSession = sWorld.FindSession(masterId);

            if (! masterSession || sObjectMgr.GetPlayer(lqh->GetGuid()))
            {
                delete holder;
                return;
            }

            // The bot's WorldSession is owned by the bot's Player object
            // The bot's WorldSession is deleted by PlayerbotMgr::LogoutPlayerBot
            WorldSession *botSession = new WorldSession(lqh->GetAccountId(), NULL, SEC_PLAYER, masterSession->Expansion(), 0, masterSession->GetSessionDbcLocale());
            botSession->m_Address = "bot";
            botSession->HandlePlayerLogin(lqh); // will delete lqh
            masterSession->GetPlayer()->GetPlayerbotMgr()->OnBotLogin(botSession->GetPlayer());
        }
} chrHandler;

void WorldSession::HandleCharEnum(QueryResult* result)
{
    WorldPacket data(SMSG_CHAR_ENUM, 270);

    ByteBuffer buffer;

    data.WriteBits(0, 23);
    data.WriteBits(result ? result->GetRowCount() : 0, 17);

    if (result)
    {
        do
        {
            sLog.outDetail("Loading char guid %u from account %u.", (*result)[0].GetUInt32(), GetAccountId());

            if (!Player::BuildEnumData(result, &data, &buffer))
            {
                sLog.outError("Building enum data for SMSG_CHAR_ENUM has failed, aborting");
                return;
            }
        }
        while (result->NextRow());
    }

    data.WriteBit(1);
    if (!buffer.empty())
    {
        data.FlushBits();
        data.append(buffer);
    }

    SendPacket(&data);
}

void WorldSession::HandleCharEnumOpcode(WorldPacket& /*recv_data*/)
{
    /// get all the data necessary for loading all characters (along with their pets) on the account
    CharacterDatabase.AsyncPQuery(&chrHandler, &CharacterHandler::HandleCharEnumCallback, GetAccountId(),
         !sWorld.getConfig(CONFIG_BOOL_DECLINED_NAMES_USED) ?
    //   ------- Query Without Declined Names --------
    //           0               1                2                3                 4                  5                       6                        7
        "SELECT characters.guid, characters.name, characters.race, characters.class, characters.gender, characters.playerBytes, characters.playerBytes2, characters.level, "
    //   8                9               10                     11                     12                     13                    14
        "characters.zone, characters.map, characters.position_x, characters.position_y, characters.position_z, guild_member.guildid, characters.playerFlags, "
                                  //  15                    16                   17                     18                   19                     20
                                  "characters.at_login, character_pet.entry, character_pet.modelid, character_pet.level, characters.equipmentCache, characters.slot "
        "FROM characters LEFT JOIN character_pet ON characters.guid=character_pet.owner AND character_pet.slot='%u' "
        "LEFT JOIN guild_member ON characters.guid = guild_member.guid "
        "WHERE characters.account = '%u' ORDER BY characters.guid"
        :
    //   --------- Query With Declined Names ---------
    //           0               1                2                3                 4                  5                       6                        7
        "SELECT characters.guid, characters.name, characters.race, characters.class, characters.gender, characters.playerBytes, characters.playerBytes2, characters.level, "
    //   8                9               10                     11                     12                     13                    14
        "characters.zone, characters.map, characters.position_x, characters.position_y, characters.position_z, guild_member.guildid, characters.playerFlags, "
                                  //  15                    16                   17                     18                   19                        20                 21
                                  "characters.at_login, character_pet.entry, character_pet.modelid, character_pet.level, characters.equipmentCache, characters.slot, character_declinedname.genitive "
        "FROM characters LEFT JOIN character_pet ON characters.guid = character_pet.owner AND character_pet.slot='%u' "
        "LEFT JOIN character_declinedname ON characters.guid = character_declinedname.guid "
        "LEFT JOIN guild_member ON characters.guid = guild_member.guid "
        "WHERE characters.account = '%u' ORDER BY characters.guid",
        PET_SAVE_AS_CURRENT, GetAccountId());
}

void WorldSession::HandleCharCreateOpcode(WorldPacket& recv_data)
{
    // extract other data required for player creating
    uint8 gender, skin, face, hairStyle, hairColor, facialHair, outfitId;
    std::string name;
    uint8 race_, class_;

    recv_data >> class_ >> hairStyle >> facialHair >> race_;
    recv_data >> face >> skin >> gender >> hairColor >> outfitId;
    name = recv_data.ReadString(recv_data.ReadBits(8));

    WorldPacket data(SMSG_CHAR_CREATE, 1);                  // returned with diff.values in all cases

    if (GetSecurity() == SEC_PLAYER)
    {
        if (uint32 mask = sWorld.getConfig(CONFIG_UINT32_CHARACTERS_CREATING_DISABLED))
        {
            bool disabled = false;

            Team team = Player::TeamForRace(race_);
            switch (team)
            {
                case ALLIANCE: disabled = mask & (1 << 0); break;
                case HORDE:    disabled = mask & (1 << 1); break;
                default: break;
            }

            if (disabled)
            {
                data << (uint8)CHAR_CREATE_DISABLED;
                SendPacket(&data);
                return;
            }
        }
    }

    ChrClassesEntry const* classEntry = sChrClassesStore.LookupEntry(class_);
    ChrRacesEntry const* raceEntry = sChrRacesStore.LookupEntry(race_);

    if (!classEntry || !raceEntry)
    {
        data << (uint8)CHAR_CREATE_FAILED;
        SendPacket(&data);
        sLog.outError("Class: %u or Race %u not found in DBC (Wrong DBC files?) or Cheater?", class_, race_);
        return;
    }

    // FIXME
    // prevent character creating Expansion race without Expansion account
    //if (raceEntry->expansion > Expansion())
    //{
    //    data << (uint8)CHAR_CREATE_EXPANSION;
    //    sLog.outError("Expansion %u account:[%d] tried to Create character with expansion %u race (%u)", Expansion(), GetAccountId(), raceEntry->expansion, race_);
    //    SendPacket(&data);
    //    return;
    //}

    // FIXME
    // prevent character creating Expansion class without Expansion account
    //if (classEntry->expansion > Expansion())
    //{
    //    data << (uint8)CHAR_CREATE_EXPANSION_CLASS;
    //    sLog.outError("Expansion %u account:[%d] tried to Create character with expansion %u class (%u)", Expansion(), GetAccountId(), classEntry->expansion, class_);
    //    SendPacket(&data);
    //    return;
    //}

    // prevent character creating with invalid name
    if (!normalizePlayerName(name))
    {
        data << (uint8)CHAR_NAME_NO_NAME;
        SendPacket(&data);
        sLog.outError("Account:[%d] but tried to Create character with empty [name]", GetAccountId());
        return;
    }

    // check name limitations
    uint8 res = ObjectMgr::CheckPlayerName(name, true);
    if (res != CHAR_NAME_SUCCESS)
    {
        data << uint8(res);
        SendPacket(&data);
        return;
    }

    if (GetSecurity() == SEC_PLAYER && sObjectMgr.IsReservedName(name))
    {
        data << (uint8)CHAR_NAME_RESERVED;
        SendPacket(&data);
        return;
    }

    if (sAccountMgr.GetPlayerGuidByName(name))
    {
        data << (uint8)CHAR_CREATE_NAME_IN_USE;
        SendPacket(&data);
        return;
    }


    if (sAccountMgr.GetCharactersCount(GetAccountId(), true) >= sWorld.getConfig(CONFIG_UINT32_CHARACTERS_PER_ACCOUNT))
    {
        data << (uint8)CHAR_CREATE_ACCOUNT_LIMIT;
        SendPacket(&data);
        return;
    }
    else if (sAccountMgr.GetCharactersCount(GetAccountId(), false) >= sWorld.getConfig(CONFIG_UINT32_CHARACTERS_PER_REALM))
    {
        data << (uint8)CHAR_CREATE_SERVER_LIMIT;
        SendPacket(&data);
        return;
    }

    // speedup check for heroic class disabled case
    uint32 heroic_free_slots = sWorld.getConfig(CONFIG_UINT32_HEROIC_CHARACTERS_PER_REALM);
    if (heroic_free_slots == 0 && GetSecurity() == SEC_PLAYER && class_ == CLASS_DEATH_KNIGHT)
    {
        data << (uint8)CHAR_CREATE_UNIQUE_CLASS_LIMIT;
        SendPacket(&data);
        return;
    }

    // speedup check for heroic class disabled case
    uint32 req_level_for_heroic = sWorld.getConfig(CONFIG_UINT32_MIN_LEVEL_FOR_HEROIC_CHARACTER_CREATING);
    if (GetSecurity() == SEC_PLAYER && class_ == CLASS_DEATH_KNIGHT && req_level_for_heroic > sWorld.getConfig(CONFIG_UINT32_MAX_PLAYER_LEVEL))
    {
        data << (uint8)CHAR_CREATE_LEVEL_REQUIREMENT;
        SendPacket(&data);
        return;
    }

    bool AllowTwoSideAccounts = sWorld.getConfig(CONFIG_BOOL_ALLOW_TWO_SIDE_ACCOUNTS) || GetSecurity() > SEC_PLAYER;
    CinematicsSkipMode skipCinematics = CinematicsSkipMode(sWorld.getConfig(CONFIG_UINT32_SKIP_CINEMATICS));

    bool have_same_race = false;

    // if 0 then allowed creating without any characters
    bool have_req_level_for_heroic = (req_level_for_heroic == 0);

    if (!AllowTwoSideAccounts || skipCinematics == CINEMATICS_SKIP_SAME_RACE || class_ == CLASS_DEATH_KNIGHT)
    {
        QueryResult* result2 = CharacterDatabase.PQuery("SELECT level,race,class FROM characters WHERE account = '%u' %s",
                               GetAccountId(), (skipCinematics == CINEMATICS_SKIP_SAME_RACE || class_ == CLASS_DEATH_KNIGHT) ? "" : "LIMIT 1");
        if (result2)
        {
            Team team_ = Player::TeamForRace(race_);

            Field* field = result2->Fetch();
            uint8 acc_race  = field[1].GetUInt32();

            if (GetSecurity() == SEC_PLAYER && class_ == CLASS_DEATH_KNIGHT)
            {
                uint8 acc_class = field[2].GetUInt32();
                if (acc_class == CLASS_DEATH_KNIGHT)
                {
                    if (heroic_free_slots > 0)
                        --heroic_free_slots;

                    if (heroic_free_slots == 0)
                    {
                        data << (uint8)CHAR_CREATE_UNIQUE_CLASS_LIMIT;
                        SendPacket(&data);
                        delete result2;
                        return;
                    }
                }

                if (!have_req_level_for_heroic)
                {
                    uint32 acc_level = field[0].GetUInt32();
                    if (acc_level >= req_level_for_heroic)
                        have_req_level_for_heroic = true;
                }
            }

            // need to check team only for first character
            // TODO: what to if account already has characters of both races?
            if (!AllowTwoSideAccounts)
            {
                if (acc_race == 0 || Player::TeamForRace(acc_race) != team_)
                {
                    data << (uint8)CHAR_CREATE_PVP_TEAMS_VIOLATION;
                    SendPacket(&data);
                    delete result2;
                    return;
                }
            }

            // search same race for cinematic or same class if need
            // TODO: check if cinematic already shown? (already logged in?; cinematic field)
            while ((skipCinematics == CINEMATICS_SKIP_SAME_RACE && !have_same_race) || class_ == CLASS_DEATH_KNIGHT)
            {
                if (!result2->NextRow())
                    break;

                field = result2->Fetch();
                acc_race = field[1].GetUInt32();

                if (!have_same_race)
                    have_same_race = race_ == acc_race;

                if (GetSecurity() == SEC_PLAYER && class_ == CLASS_DEATH_KNIGHT)
                {
                    uint8 acc_class = field[2].GetUInt32();
                    if (acc_class == CLASS_DEATH_KNIGHT)
                    {
                        if (heroic_free_slots > 0)
                            --heroic_free_slots;

                        if (heroic_free_slots == 0)
                        {
                            data << (uint8)CHAR_CREATE_UNIQUE_CLASS_LIMIT;
                            SendPacket(&data);
                            delete result2;
                            return;
                        }
                    }

                    if (!have_req_level_for_heroic)
                    {
                        uint32 acc_level = field[0].GetUInt32();
                        if (acc_level >= req_level_for_heroic)
                            have_req_level_for_heroic = true;
                    }
                }
            }
            delete result2;
        }
    }

    if (GetSecurity() == SEC_PLAYER && class_ == CLASS_DEATH_KNIGHT && !have_req_level_for_heroic)
    {
        data << (uint8)CHAR_CREATE_LEVEL_REQUIREMENT;
        SendPacket(&data);
        return;
    }

    Player pNewChar(this);
    if (!pNewChar.Create(sObjectMgr.GeneratePlayerLowGuid(), name, race_, class_, gender, skin, face, hairStyle, hairColor, facialHair, outfitId))
    {
        // Player not create (race/class problem?)
        data << (uint8)CHAR_CREATE_ERROR;
        SendPacket(&data);

        return;
    }

    if ((have_same_race && skipCinematics == CINEMATICS_SKIP_SAME_RACE) || skipCinematics == CINEMATICS_SKIP_ALL)
        pNewChar.setCinematic(1);                          // not show intro

    pNewChar.SetAtLoginFlag(AT_LOGIN_FIRST);               // First login

    // Player created, save it now
    pNewChar.SaveToDB();

    sAccountMgr.UpdateCharactersCount(GetAccountId(), sWorld.getConfig(CONFIG_UINT32_REALMID));

    data << (uint8)CHAR_CREATE_SUCCESS;
    SendPacket(&data);

    std::string IP_str = GetRemoteAddress();
    BASIC_LOG("Account: %d (IP: %s) Create Character:[%s] (guid: %u)", GetAccountId(), IP_str.c_str(), name.c_str(), pNewChar.GetGUIDLow());
    sLog.outChar("Account: %d (IP: %s) Create Character:[%s] (guid: %u)", GetAccountId(), IP_str.c_str(), name.c_str(), pNewChar.GetGUIDLow());
}

void WorldSession::HandleCharDeleteOpcode(WorldPacket& recv_data)
{
    ObjectGuid guid;
    recv_data >> guid;

    // can't delete loaded character
    if (sObjectMgr.GetPlayer(guid))
        return;

    uint32 accountId = 0;
    std::string name;

    // is guild leader
    if (sGuildMgr.GetGuildByLeader(guid))
    {
        WorldPacket data(SMSG_CHAR_DELETE, 1);
        data << (uint8)CHAR_DELETE_FAILED_GUILD_LEADER;
        SendPacket(&data);
        return;
    }

    // is arena team captain
    if (sObjectMgr.GetArenaTeamByCaptain(guid))
    {
        WorldPacket data(SMSG_CHAR_DELETE, 1);
        data << (uint8)CHAR_DELETE_FAILED_ARENA_CAPTAIN;
        SendPacket(&data);
        return;
    }

    uint32 lowguid = guid.GetCounter();

    QueryResult* result = CharacterDatabase.PQuery("SELECT account,name FROM characters WHERE guid='%u'", lowguid);
    if (result)
    {
        Field* fields = result->Fetch();
        accountId = fields[0].GetUInt32();
        name = fields[1].GetCppString();
        delete result;
    }

    // prevent deleting other players' characters using cheating tools
    if (accountId != GetAccountId())
        return;

    std::string IP_str = GetRemoteAddress();
    BASIC_LOG("Account: %d (IP: %s) Delete Character:[%s] (guid: %u)", GetAccountId(), IP_str.c_str(), name.c_str(), lowguid);
    sLog.outChar("Account: %d (IP: %s) Delete Character:[%s] (guid: %u)", GetAccountId(), IP_str.c_str(), name.c_str(), lowguid);

    if (sLog.IsOutCharDump())                               // optimize GetPlayerDump call
    {
        std::string dump = PlayerDumpWriter().GetDump(lowguid);
        sLog.outCharDump(dump.c_str(), GetAccountId(), lowguid, name.c_str());
    }

    sCalendarMgr.RemovePlayerCalendar(guid);

    Player::DeleteFromDB(guid, GetAccountId());

    WorldPacket data(SMSG_CHAR_DELETE, 1);
    data << (uint8)CHAR_DELETE_SUCCESS;
    SendPacket(&data);
}

void WorldSession::HandlePlayerLoginOpcode(WorldPacket& recv_data)
{
    ObjectGuid playerGuid;

    recv_data.ReadGuidMask<5, 7, 6, 1, 2, 3, 4, 0>(playerGuid);
    recv_data.ReadGuidBytes<6, 4, 3, 5, 0, 2, 7, 1>(playerGuid);
    float unk = recv_data.ReadSingle();

    DEBUG_LOG("WORLD: Received opcode Player Logon Message from %s, unk float: %f", playerGuid.GetString().c_str(), unk);

    if (PlayerLoading() || GetPlayer() != NULL)
    {
        sLog.outError("Player tryes to login again, AccountId = %d", GetAccountId());
        return;
    }

    m_playerLoading = true;

    // check if character is currently a playerbot, if so then logout
    Player *checkChar = sObjectMgr.GetPlayer(playerGuid);
    if (checkChar && checkChar->GetPlayerbotAI())
    {
        checkChar->GetPlayerbotAI()->GetManager()->LogoutPlayerBot(playerGuid);
        --checkChar->GetPlayerbotAI()->GetManager()->m_botCount;
    }

    LoginQueryHolder* holder = new LoginQueryHolder(GetAccountId(), playerGuid);
    if (!holder->Initialize())
    {
        delete holder;                                      // delete all unprocessed queries
        m_playerLoading = false;
        return;
    }

    CharacterDatabase.DelayQueryHolder(&chrHandler, &CharacterHandler::HandlePlayerLoginCallback, holder);
}

// Playerbot mod. Can't easily reuse HandlePlayerLoginOpcode for logging in bots because it assumes
// a WorldSession exists for the bot. The WorldSession for a bot is created after the character is loaded.
void PlayerbotMgr::AddPlayerBot(ObjectGuid playerGuid)
{
    if (sWorld.getConfig(CONFIG_BOOL_PLAYERBOT_DISABLE))
        return;

    // has bot already been added?
    if (sObjectMgr.GetPlayer(playerGuid))
        return;

    uint32 accountId = sAccountMgr.GetPlayerAccountIdByGUID(playerGuid);
    if (accountId == 0)
        return;

    LoginQueryHolder *holder = new LoginQueryHolder(accountId, playerGuid);
    if (!holder->Initialize())
    {
        delete holder;                                      // delete all unprocessed queries
        return;
    }

    uint32 masterId = sAccountMgr.GetPlayerAccountIdByGUID(GetMaster()->GetObjectGuid());
    CharacterDatabase.DelayQueryHolder(&chrHandler, &CharacterHandler::HandlePlayerBotLoginCallback, holder, masterId);
}

void WorldSession::HandlePlayerLogin(LoginQueryHolder* holder)
{
    ObjectGuid playerGuid = holder->GetGuid();

    Player* pCurrChar = new Player(this);
    pCurrChar->GetMotionMaster()->Initialize();

    // "GetAccountId()==db stored account id" checked in LoadFromDB (prevent login not own character using cheating tools)
    if (!pCurrChar->LoadFromDB(playerGuid, holder))
    {
        KickPlayer();                                       // disconnect client, player no set to session and it will not deleted or saved at kick
        delete pCurrChar;                                   // delete it manually
        delete holder;                                      // delete all unprocessed queries
        m_playerLoading = false;
        return;
    }

    SetPlayer(pCurrChar);

    pCurrChar->SendDungeonDifficulty(false);

    WorldPacket data(SMSG_LOGIN_VERIFY_WORLD, 20);
    data << pCurrChar->GetMapId();
    data << pCurrChar->GetPositionX();
    data << pCurrChar->GetPositionY();
    data << pCurrChar->GetPositionZ();
    data << NormalizeOrientation(pCurrChar->GetOrientation());
    SendPacket(&data);

    // load player specific part before send times
    LoadAccountData(holder->GetResult(PLAYER_LOGIN_QUERY_LOADACCOUNTDATA), PER_CHARACTER_CACHE_MASK);
    SendAccountDataTimes(PER_CHARACTER_CACHE_MASK);

    data.Initialize(SMSG_FEATURE_SYSTEM_STATUS, 34);        // added in 2.2.0
    data << uint32(1);                                      // Scrolls of Ressurection?
    data << uint32(1);
    data << uint32(2);
    data << uint32(0);
    data << uint8(2);                                       // complain system status
    data.WriteBit(false);
    data.WriteBit(false);                                   // session time alert
    data.WriteBit(true);
    data.WriteBit(false);                                   // enable(1)/disable(0) voice chat interface in client ?
    data.WriteBit(true);                                    // quick ticket?
    data << uint32(60);
    data << uint32(10);
    data << uint32(1);
    data << uint32(2);
    data << uint32(0);
    SendPacket(&data);

    // Send MOTD
    {
        data.Initialize(SMSG_MOTD, 50);                     // new in 2.0.1
        data << (uint32)0;

        uint32 linecount = 0;
        std::string str_motd = sWorld.GetMotd();
        std::string::size_type pos, nextpos;

        pos = 0;
        while ((nextpos = str_motd.find('@', pos)) != std::string::npos)
        {
            if (nextpos != pos)
            {
                data << str_motd.substr(pos, nextpos - pos);
                ++linecount;
            }
            pos = nextpos + 1;
        }

        if (pos < str_motd.length())
        {
            data << str_motd.substr(pos);
            ++linecount;
        }

        data.put(0, linecount);

        SendPacket(&data);
        DEBUG_LOG("WORLD: Sent motd (SMSG_MOTD)");
    }

    // QueryResult *result = CharacterDatabase.PQuery("SELECT guildid,rank FROM guild_member WHERE guid = '%u'",pCurrChar->GetGUIDLow());
    QueryResult* resultGuild = holder->GetResult(PLAYER_LOGIN_QUERY_LOADGUILD);

    if (resultGuild)
    {
        Field* fields = resultGuild->Fetch();
        pCurrChar->SetInGuild(fields[0].GetUInt32());
        pCurrChar->SetRank(fields[1].GetUInt32());
        delete resultGuild;
    }
    else if (pCurrChar->GetGuildId())                       // clear guild related fields in case wrong data about nonexistent membership
    {
        pCurrChar->SetInGuild(0);
        pCurrChar->SetGuildLevel(0);
        pCurrChar->SetRank(0);
    }

    if (pCurrChar->GetGuildId() != 0)
    {
        Guild* guild = sGuildMgr.GetGuildById(pCurrChar->GetGuildId());
        if (guild)
        {
            pCurrChar->SetGuildLevel(guild->GetLevel());

            data.Initialize(SMSG_GUILD_EVENT, (1 + 1 + guild->GetMOTD().size() + 1));
            data << uint8(GE_MOTD);
            data << uint8(1);
            data << guild->GetMOTD();
            SendPacket(&data);
            DEBUG_LOG("WORLD: Sent guild-motd (SMSG_GUILD_EVENT)");

            guild->DisplayGuildBankTabsInfo(this);

            guild->BroadcastEvent(GE_SIGNED_ON, pCurrChar->GetObjectGuid(), pCurrChar->GetName());
        }
        else
        {
            // remove wrong guild data
            sLog.outError("Player %s (GUID: %u) marked as member of nonexistent guild (id: %u), removing guild membership for player.", pCurrChar->GetName(), pCurrChar->GetGUIDLow(), pCurrChar->GetGuildId());
            pCurrChar->SetInGuild(0);
            pCurrChar->SetGuildLevel(0);
        }
    }

    data.Initialize(SMSG_LEARNED_DANCE_MOVES, 4 + 4);
    data << uint64(0);
    SendPacket(&data);

    data.Initialize(SMSG_HOTFIX_INFO);
    HotfixData const& hotfix = sObjectMgr.GetHotfixData();
    data.WriteBits(hotfix.size(), 22);
    data.FlushBits();
    for (uint32 i = 0; i < hotfix.size(); ++i)
    {
        data << uint32(hotfix[i].Type);
        data << uint32(hotfix[i].Timestamp);
        data << uint32(hotfix[i].Entry);
    }
    SendPacket(&data);

    pCurrChar->SendInitialPacketsBeforeAddToMap();

    // Show cinematic at the first time that player login
    if (!pCurrChar->getCinematic())
    {
        pCurrChar->setCinematic(1);

        if (ChrClassesEntry const* cEntry = sChrClassesStore.LookupEntry(pCurrChar->getClass()))
        {
            if (cEntry->CinematicSequence)
                pCurrChar->SendCinematicStart(cEntry->CinematicSequence);
            else if (ChrRacesEntry const* rEntry = sChrRacesStore.LookupEntry(pCurrChar->getRace()))
                pCurrChar->SendCinematicStart(rEntry->CinematicSequence);
        }
    }

    if (pCurrChar->NeedEjectFromThisMap() || !pCurrChar->GetMap()->Add(pCurrChar))
    {
        AreaTrigger const* at = sObjectMgr.GetGoBackTrigger(pCurrChar->GetMapId());
        if (!at || !pCurrChar->TeleportTo(at->loc))
            pCurrChar->TeleportToHomebind();
    }

    sObjectAccessor.AddObject(pCurrChar);
    // DEBUG_LOG("Player %s added to Map.",pCurrChar->GetName());

    pCurrChar->SendInitialPacketsAfterAddToMap();

    static SqlStatementID updChars;

    SqlStatement stmt = CharacterDatabase.CreateStatement(updChars, "UPDATE characters SET online = 1 WHERE guid = ?");
    stmt.PExecute(pCurrChar->GetGUIDLow());

    // don't update active realm if is playerbot
    if (pCurrChar->GetSession()->GetRemoteAddress() != "bot")
    {
        static SqlStatementID updAccount;

        stmt = LoginDatabase.CreateStatement(updAccount, "UPDATE account SET active_realm_id = ? WHERE id = ?");
        stmt.PExecute(sWorld.getConfig(CONFIG_UINT32_REALMID), GetAccountId());
    }

    pCurrChar->SetInGameTime(WorldTimer::getMSTime());

    // announce group about member online (must be after add to player list to receive announce to self)
    if (Group* group = pCurrChar->GetGroup())
        group->SendUpdate();

    // friend status
    sSocialMgr.SendFriendStatus(pCurrChar, FRIEND_ONLINE, pCurrChar->GetObjectGuid(), true);

    // Place character in world (and load zone) before some object loading
    pCurrChar->LoadCorpse();

    // setting Ghost+speed if dead
    if (pCurrChar->m_deathState != ALIVE)
    {
        // not blizz like, we must correctly save and load player instead...
        if (pCurrChar->getRace() == RACE_NIGHTELF)
            pCurrChar->CastSpell(pCurrChar, 20584, true);   // auras SPELL_AURA_INCREASE_SPEED(+speed in wisp form), SPELL_AURA_INCREASE_SWIM_SPEED(+swim speed in wisp form), SPELL_AURA_TRANSFORM (to wisp form)
        pCurrChar->CastSpell(pCurrChar, 8326, true);        // auras SPELL_AURA_GHOST, SPELL_AURA_INCREASE_SPEED(why?), SPELL_AURA_INCREASE_SWIM_SPEED(why?)

        pCurrChar->SetWaterWalk(true);
    }

    pCurrChar->ContinueTaxiFlight();

    // reset for all pets before pet loading
    if (pCurrChar->HasAtLoginFlag(AT_LOGIN_RESET_PET_TALENTS))
        Pet::resetTalentsForAllPetsOf(pCurrChar);

    // Load pet if any (if player not alive and in taxi flight or another then pet will remember as temporary unsummoned)
    pCurrChar->LoadPet();

    // Set FFA PvP for non GM in non-rest mode
    if (sWorld.IsFFAPvPRealm() && !pCurrChar->isGameMaster() && !pCurrChar->HasFlag(PLAYER_FLAGS, PLAYER_FLAGS_RESTING))
        pCurrChar->SetFFAPvP(true);

    if (pCurrChar->HasFlag(PLAYER_FLAGS, PLAYER_FLAGS_CONTESTED_PVP))
        pCurrChar->SetContestedPvP();

    // Apply at_login requests
    if (pCurrChar->HasAtLoginFlag(AT_LOGIN_RESET_SPELLS))
    {
        pCurrChar->resetSpells();
        SendNotification(LANG_RESET_SPELLS);
    }

    if (pCurrChar->HasAtLoginFlag(AT_LOGIN_RESET_TALENTS))
    {
        pCurrChar->resetTalents(true, true);
        pCurrChar->SendTalentsInfoData(false);              // original talents send already in to SendInitialPacketsBeforeAddToMap, resend reset state
        SendNotification(LANG_RESET_TALENTS);               // we can use SMSG_TALENTS_INVOLUNTARILY_RESET here
    }

    if (pCurrChar->HasAtLoginFlag(AT_LOGIN_FIRST))
        pCurrChar->RemoveAtLoginFlag(AT_LOGIN_FIRST);

    // show time before shutdown if shutdown planned.
    if (sWorld.IsShutdowning())
        sWorld.ShutdownMsg(true, pCurrChar);

    if (sWorld.getConfig(CONFIG_BOOL_ALL_TAXI_PATHS))
        pCurrChar->SetTaxiCheater(true);

    if (pCurrChar->isGameMaster())
        SendNotification(LANG_GM_ON);

    if (!pCurrChar->isGMVisible())
    {
        SendNotification(LANG_INVISIBLE_INVISIBLE);
        SpellEntry const* invisibleAuraInfo = sSpellStore.LookupEntry(sWorld.getConfig(CONFIG_UINT32_GM_INVISIBLE_AURA));
        if (invisibleAuraInfo && IsSpellAppliesAura(invisibleAuraInfo))
            pCurrChar->CastSpell(pCurrChar, invisibleAuraInfo, true);
    }

    std::string IP_str = GetRemoteAddress();
    sLog.outChar("Account: %d (IP: %s) Login Character:[%s] (guid: %u)",
                 GetAccountId(), IP_str.c_str(), pCurrChar->GetName(), pCurrChar->GetGUIDLow());

    if (!pCurrChar->IsStandState() && !pCurrChar->hasUnitState(UNIT_STAT_STUNNED))
        pCurrChar->SetStandState(UNIT_STAND_STATE_STAND);

    m_playerLoading = false;

    // Handle Login-Achievements (should be handled after loading)
    pCurrChar->GetAchievementMgr().UpdateAchievementCriteria(ACHIEVEMENT_CRITERIA_TYPE_ON_LOGIN, 1);

    // Titles check
    if (pCurrChar->HasAtLoginFlag(AT_LOGIN_CHECK_TITLES))
    {
        Team team = pCurrChar->GetTeam();
        if (QueryResult *result = WorldDatabase.Query("SELECT alliance_id, horde_id FROM player_factionchange_titles"))
        {
            do
            {
                Field *fields = result->Fetch();
                uint32 title_alliance = fields[0].GetUInt32();
                uint32 title_horde = fields[1].GetUInt32();

                CharTitlesEntry const* titleEntry = sCharTitlesStore.LookupEntry(team == HORDE ? title_alliance : title_horde);
                uint32 fieldIndexOffset = titleEntry->bit_index / 32;
                uint32 flag = 1 << (titleEntry->bit_index % 32);
                if (pCurrChar->HasFlag(PLAYER__FIELD_KNOWN_TITLES + fieldIndexOffset, flag))
                {
                    pCurrChar->SetTitle(titleEntry, true);
                    if (CharTitlesEntry const* titleNewEntry = sCharTitlesStore.LookupEntry(team == HORDE ? title_horde : title_alliance))
                    {
                        pCurrChar->SetTitle(titleNewEntry);
                        pCurrChar->SetUInt32Value(PLAYER_CHOSEN_TITLE, 0);
                    }
                }
            }
            while(result->NextRow());
        }
        pCurrChar->RemoveAtLoginFlag(AT_LOGIN_CHECK_TITLES);
    }

    delete holder;
}

void WorldSession::HandleSetFactionAtWarOpcode(WorldPacket& recv_data)
{
    DEBUG_LOG("WORLD: Received CMSG_SET_FACTION_ATWAR");

    uint32 repListID;
    uint8  flag;

    recv_data >> repListID;
    recv_data >> flag;

    GetPlayer()->GetReputationMgr().SetAtWar(repListID, flag);
}

void WorldSession::HandleTutorialFlagOpcode(WorldPacket& recv_data)
{
    uint32 iFlag;
    recv_data >> iFlag;

    uint32 wInt = (iFlag / 32);
    if (wInt >= 8)
    {
        // sLog.outError("CHEATER? Account:[%d] Guid[%u] tried to send wrong CMSG_TUTORIAL_FLAG", GetAccountId(),GetGUID());
        return;
    }
    uint32 rInt = (iFlag % 32);

    uint32 tutflag = GetTutorialInt(wInt);
    tutflag |= (1 << rInt);
    SetTutorialInt(wInt, tutflag);

    // DEBUG_LOG("Received Tutorial Flag Set {%u}.", iFlag);
}

void WorldSession::HandleTutorialClearOpcode(WorldPacket& /*recv_data*/)
{
    for (int i = 0; i < 8; ++i)
        SetTutorialInt(i, 0xFFFFFFFF);
}

void WorldSession::HandleTutorialResetOpcode(WorldPacket& /*recv_data*/)
{
    for (int i = 0; i < 8; ++i)
        SetTutorialInt(i, 0x00000000);
}

void WorldSession::HandleSetWatchedFactionOpcode(WorldPacket& recv_data)
{
    DEBUG_LOG("WORLD: Received CMSG_SET_WATCHED_FACTION");
    int32 repId;
    recv_data >> repId;
    GetPlayer()->SetInt32Value(PLAYER_FIELD_WATCHED_FACTION_INDEX, repId);
}

void WorldSession::HandleSetFactionInactiveOpcode(WorldPacket& recv_data)
{
    DEBUG_LOG("WORLD: Received CMSG_SET_FACTION_INACTIVE");
    uint32 replistid;
    uint8 inactive;
    recv_data >> replistid >> inactive;

    _player->GetReputationMgr().SetInactive(replistid, inactive);
}

void WorldSession::HandleShowingHelmOpcode(WorldPacket& /*recv_data*/)
{
    DEBUG_LOG("CMSG_SHOWING_HELM for %s", _player->GetName());
    _player->ToggleFlag(PLAYER_FLAGS, PLAYER_FLAGS_HIDE_HELM);
}

void WorldSession::HandleShowingCloakOpcode(WorldPacket& /*recv_data*/)
{
    DEBUG_LOG("CMSG_SHOWING_CLOAK for %s", _player->GetName());
    _player->ToggleFlag(PLAYER_FLAGS, PLAYER_FLAGS_HIDE_CLOAK);
}

void WorldSession::HandleCharRenameOpcode(WorldPacket& recv_data)
{
    ObjectGuid guid;
    std::string newname;

    recv_data >> guid;
    recv_data >> newname;

    // prevent character rename to invalid name
    if (!normalizePlayerName(newname))
    {
        WorldPacket data(SMSG_CHAR_RENAME, 1);
        data << uint8(CHAR_NAME_NO_NAME);
        SendPacket(&data);
        return;
    }

    uint8 res = ObjectMgr::CheckPlayerName(newname, true);
    if (res != CHAR_NAME_SUCCESS)
    {
        WorldPacket data(SMSG_CHAR_RENAME, 1);
        data << uint8(res);
        SendPacket(&data);
        return;
    }

    // check name limitations
    if (GetSecurity() == SEC_PLAYER && sObjectMgr.IsReservedName(newname))
    {
        WorldPacket data(SMSG_CHAR_RENAME, 1);
        data << uint8(CHAR_NAME_RESERVED);
        SendPacket(&data);
        return;
    }

    std::string escaped_newname = newname;
    CharacterDatabase.escape_string(escaped_newname);

    // make sure that the character belongs to the current account, that rename at login is enabled
    // and that there is no character with the desired new name
    CharacterDatabase.AsyncPQuery(&WorldSession::HandleChangePlayerNameOpcodeCallBack,
                                  GetAccountId(), newname,
                                  "SELECT guid, name FROM characters WHERE guid = %u AND account = %u AND (at_login & %u) = %u AND NOT EXISTS (SELECT NULL FROM characters WHERE name = '%s')",
                                  guid.GetCounter(), GetAccountId(), AT_LOGIN_RENAME, AT_LOGIN_RENAME, escaped_newname.c_str()
                                 );
}

void WorldSession::HandleChangePlayerNameOpcodeCallBack(QueryResult* result, uint32 accountId, std::string newname)
{
    WorldSession* session = sWorld.FindSession(accountId);
    if (!session)
    {
        if (result) delete result;
        return;
    }

    if (!result)
    {
        WorldPacket data(SMSG_CHAR_RENAME, 1);
        data << uint8(CHAR_CREATE_ERROR);
        session->SendPacket(&data);
        return;
    }

    uint32 guidLow = result->Fetch()[0].GetUInt32();
    ObjectGuid guid = ObjectGuid(HIGHGUID_PLAYER, guidLow);
    std::string oldname = result->Fetch()[1].GetCppString();

    delete result;

    CharacterDatabase.BeginTransaction();
    CharacterDatabase.PExecute("UPDATE characters set name = '%s', at_login = at_login & ~ %u WHERE guid ='%u'", newname.c_str(), uint32(AT_LOGIN_RENAME), guidLow);
    CharacterDatabase.PExecute("DELETE FROM character_declinedname WHERE guid ='%u'", guidLow);
    CharacterDatabase.CommitTransaction();

    sLog.outChar("Account: %d (IP: %s) Character:[%s] (guid:%u) Changed name to: %s", session->GetAccountId(), session->GetRemoteAddress().c_str(), oldname.c_str(), guidLow, newname.c_str());

    WorldPacket data(SMSG_CHAR_RENAME, 1 + 8 + (newname.size() + 1));
    data << uint8(RESPONSE_SUCCESS);
    data << guid;
    data << newname;
    session->SendPacket(&data);
}

void WorldSession::HandleSetPlayerDeclinedNamesOpcode(WorldPacket& recv_data)
{
    ObjectGuid guid;

    recv_data >> guid;

    // not accept declined names for unsupported languages
    std::string name;
    if (!sAccountMgr.GetPlayerNameByGUID(guid, name))
    {
        WorldPacket data(SMSG_SET_PLAYER_DECLINED_NAMES_RESULT, 4 + 8);
        data << uint32(1);
        data << ObjectGuid(guid);
        SendPacket(&data);
        return;
    }

    std::wstring wname;
    if (!Utf8toWStr(name, wname))
    {
        WorldPacket data(SMSG_SET_PLAYER_DECLINED_NAMES_RESULT, 4 + 8);
        data << uint32(1);
        data << ObjectGuid(guid);
        SendPacket(&data);
        return;
    }

    if (!isCyrillicCharacter(wname[0]))                     // name already stored as only single alphabet using
    {
        WorldPacket data(SMSG_SET_PLAYER_DECLINED_NAMES_RESULT, 4 + 8);
        data << uint32(1);
        data << ObjectGuid(guid);
        SendPacket(&data);
        return;
    }

    std::string name2;
    DeclinedName declinedname;

    recv_data >> name2;

    if (name2 != name)                                      // character have different name
    {
        WorldPacket data(SMSG_SET_PLAYER_DECLINED_NAMES_RESULT, 4 + 8);
        data << uint32(1);
        data << ObjectGuid(guid);
        SendPacket(&data);
        return;
    }

    for (int i = 0; i < MAX_DECLINED_NAME_CASES; ++i)
    {
        recv_data >> declinedname.name[i];
        if (!normalizePlayerName(declinedname.name[i]))
        {
            WorldPacket data(SMSG_SET_PLAYER_DECLINED_NAMES_RESULT, 4 + 8);
            data << uint32(1);
            data << ObjectGuid(guid);
            SendPacket(&data);
            return;
        }
    }

    if (!ObjectMgr::CheckDeclinedNames(wname, declinedname))
    {
        WorldPacket data(SMSG_SET_PLAYER_DECLINED_NAMES_RESULT, 4 + 8);
        data << uint32(1);
        data << ObjectGuid(guid);
        SendPacket(&data);
        return;
    }

    for (int i = 0; i < MAX_DECLINED_NAME_CASES; ++i)
        CharacterDatabase.escape_string(declinedname.name[i]);

    CharacterDatabase.BeginTransaction();
    CharacterDatabase.PExecute("DELETE FROM character_declinedname WHERE guid = '%u'", guid.GetCounter());
    CharacterDatabase.PExecute("INSERT INTO character_declinedname (guid, genitive, dative, accusative, instrumental, prepositional) VALUES ('%u','%s','%s','%s','%s','%s')",
                               guid.GetCounter(), declinedname.name[0].c_str(), declinedname.name[1].c_str(), declinedname.name[2].c_str(), declinedname.name[3].c_str(), declinedname.name[4].c_str());
    CharacterDatabase.CommitTransaction();

    WorldPacket data(SMSG_SET_PLAYER_DECLINED_NAMES_RESULT, 4 + 8);
    data << uint32(0);                                      // OK
    data << ObjectGuid(guid);
    SendPacket(&data);
}

void WorldSession::HandleAlterAppearanceOpcode(WorldPacket& recv_data)
{
    DEBUG_LOG("CMSG_ALTER_APPEARANCE");

    uint32 skinTone_id = -1;

    uint32 Hair, Color, FacialHair, SkinTone;
    if(_player->getRace() != RACE_TAUREN) recv_data >> Hair >> Color >> FacialHair;
    else
    {
        recv_data >> Hair >> Color >> FacialHair >> SkinTone;
        BarberShopStyleEntry const* bs_skinTone = sBarberShopStyleStore.LookupEntry(SkinTone);
        if (!bs_skinTone || bs_skinTone->type != 3 || bs_skinTone->race != _player->getRace() || bs_skinTone->gender != _player->getGender())
            return;
        skinTone_id = bs_skinTone->hair_id;
    }

    BarberShopStyleEntry const* bs_hair = sBarberShopStyleStore.LookupEntry(Hair);

    if (!bs_hair || bs_hair->type != 0 || bs_hair->race != _player->getRace() || bs_hair->gender != _player->getGender())
        return;

    BarberShopStyleEntry const* bs_facialHair = sBarberShopStyleStore.LookupEntry(FacialHair);

    if (!bs_facialHair || bs_facialHair->type != 2 || bs_facialHair->race != _player->getRace() || bs_facialHair->gender != _player->getGender())
        return;

    uint32 Cost = _player->GetBarberShopCost(bs_hair->hair_id, Color, bs_facialHair->hair_id, skinTone_id);

    // 0 - ok
    // 1,3 - not enough money
    // 2 - you have to seat on barber chair
    if (_player->GetMoney() < Cost)
    {
        WorldPacket data(SMSG_BARBER_SHOP_RESULT, 4);
        data << uint32(1);                                  // no money
        SendPacket(&data);
        return;
    }
    else
    {
        WorldPacket data(SMSG_BARBER_SHOP_RESULT, 4);
        data << uint32(0);                                  // ok
        SendPacket(&data);
    }

    _player->ModifyMoney(-int64(Cost));                     // it isn't free
    _player->GetAchievementMgr().UpdateAchievementCriteria(ACHIEVEMENT_CRITERIA_TYPE_GOLD_SPENT_AT_BARBER, Cost);

    _player->SetByteValue(PLAYER_BYTES, 2, uint8(bs_hair->hair_id));
    _player->SetByteValue(PLAYER_BYTES, 3, uint8(Color));
    _player->SetByteValue(PLAYER_BYTES_2, 0, uint8(bs_facialHair->hair_id));
    if (_player->getRace() == RACE_TAUREN)
        _player->SetByteValue(PLAYER_BYTES, 0, uint8(skinTone_id));

    _player->GetAchievementMgr().UpdateAchievementCriteria(ACHIEVEMENT_CRITERIA_TYPE_VISIT_BARBER_SHOP, 1);

    _player->SetStandState(0);                              // stand up
}

void WorldSession::HandleRemoveGlyphOpcode(WorldPacket& recv_data)
{
    uint32 slot;
    recv_data >> slot;

    if (slot >= MAX_GLYPH_SLOT_INDEX)
    {
        DEBUG_LOG("Client sent wrong glyph slot number in opcode CMSG_REMOVE_GLYPH %u", slot);
        return;
    }

    if (_player->GetGlyph(slot))
    {
        _player->ApplyGlyph(slot, false);
        _player->SetGlyph(slot, 0);
        _player->SendTalentsInfoData(false);
    }
}

void WorldSession::HandleCharFactionOrRaceChangeOpcode(WorldPacket& recv_data)
{
    ObjectGuid guid;
    std::string newname;
    uint8 gender, skin, face, hairStyle, hairColor, facialHair, newRace;
    recv_data >> guid;
    recv_data >> newname;
    recv_data >> gender >> skin >> hairColor >> hairStyle >> facialHair >> face >> newRace;

    QueryResult* result = CharacterDatabase.PQuery("SELECT at_login, name, race FROM characters WHERE guid ='%u'", guid.GetCounter());
    if (!result)
    {
        WorldPacket data(SMSG_CHAR_FACTION_CHANGE, 1);
        data << uint8(CHAR_CREATE_ERROR);
        SendPacket(&data);
        return;
    }

    Field* fields = result->Fetch();
    uint32 at_loginFlags = fields[0].GetUInt32();
    std::string oldname = fields[1].GetCppString();
    uint8 oldRace = fields[2].GetUInt8();
    AtLoginFlags used_loginFlag = recv_data.GetOpcode() == CMSG_CHAR_FACTION_CHANGE ? AT_LOGIN_CHANGE_FACTION : AT_LOGIN_CHANGE_RACE;
    delete result;

    if (!(at_loginFlags & used_loginFlag))
    {
        WorldPacket data(SMSG_CHAR_FACTION_CHANGE, 1);
        data << uint8(CHAR_CREATE_ERROR);
        SendPacket(&data);
        return;
    }

    if (sWorld.getConfig(CONFIG_BOOL_FACTION_AND_RACE_CHANGE_WITHOUT_RENAMING))
        newname = oldname;
    else
    {
        // prevent character rename to invalid name
        if (!normalizePlayerName(newname))
        {
            WorldPacket data(SMSG_CHAR_FACTION_CHANGE, 1);
            data << uint8(CHAR_NAME_NO_NAME);
            SendPacket(&data);
            return;
        }

        uint8 res = ObjectMgr::CheckPlayerName(newname,true);
        if (res != CHAR_NAME_SUCCESS)
        {
            WorldPacket data(SMSG_CHAR_FACTION_CHANGE, 1);
            data << uint8(res);
            SendPacket(&data);
            return;
        }

        // check name limitations
        if (GetSecurity() == SEC_PLAYER && sObjectMgr.IsReservedName(newname))
        {
            WorldPacket data(SMSG_CHAR_FACTION_CHANGE, 1);
            data << uint8(CHAR_NAME_RESERVED);
            SendPacket(&data);
            return;
        }

        // character with this name already exist
        if (sAccountMgr.GetPlayerGuidByName(newname))
        {
            ObjectGuid newguid = sAccountMgr.GetPlayerGuidByName(newname);
            if (newguid != guid)
            {
                WorldPacket data(SMSG_CHAR_FACTION_CHANGE, 1);
                data << uint8(CHAR_CREATE_NAME_IN_USE);
                SendPacket(&data);
                return;
            }
        }
    }

    // The player was uninvited already on logout so just remove from group
    // immediately remove from group before start change process
    QueryResult* resultGroup = CharacterDatabase.PQuery("SELECT groupId FROM group_member WHERE memberGuid='%u'", guid.GetCounter());
    if (resultGroup)
    {
        uint32 groupId = (*resultGroup)[0].GetUInt32();
        delete resultGroup;

        Group* group = sObjectMgr.GetGroupById(groupId);
        if (group)
            Player::RemoveFromGroup(group, guid);
    }

    CharacterDatabase.escape_string(newname);
    Player::Customize(guid, gender, skin, face, hairStyle, hairColor, facialHair);
    CharacterDatabase.BeginTransaction();
    CharacterDatabase.PExecute("UPDATE characters SET name = '%s', race = '%u', at_login = at_login & ~ %u WHERE guid ='%u'", newname.c_str(), newRace, uint32(used_loginFlag), guid.GetCounter());
    CharacterDatabase.PExecute("DELETE FROM character_declinedname WHERE guid ='%u'", guid.GetCounter());
    uint32 deletedGuild = 0;

    // Search old faction.
    TeamIndex oldTeam = TEAM_INDEX_ALLIANCE;
    switch (oldRace)
    {
        case RACE_ORC:
        case RACE_TAUREN:
        case RACE_UNDEAD:
        case RACE_TROLL:
        case RACE_BLOODELF:
        // case RACE_GOBLIN: for cataclysm
            oldTeam = TEAM_INDEX_HORDE;
            break;
        default:
            break;
    }

    // Search each faction is targeted
    TeamIndex newTeam = TEAM_INDEX_ALLIANCE;
    switch (newRace)
    {
        case RACE_ORC:
        case RACE_TAUREN:
        case RACE_UNDEAD:
        case RACE_TROLL:
        case RACE_BLOODELF:
        // case RACE_GOBLIN: for cataclysm
            newTeam = TEAM_INDEX_HORDE;
            break;
        default:
            break;
    }

    if (used_loginFlag == AT_LOGIN_CHANGE_FACTION && newTeam != oldTeam)
    {
        // Delete all Flypaths
        CharacterDatabase.PExecute("UPDATE characters SET taxi_path = '' WHERE guid ='%u'", guid.GetCounter());
        // Delete all current quests
        CharacterDatabase.PExecute("DELETE FROM `character_queststatus` WHERE `status` = 3 AND guid ='%u'", guid.GetCounter());
        // Reset guild
        if (uint32 guildId = Player::GetGuildIdFromDB(guid))
        {
            Guild* guild = sGuildMgr.GetGuildById(guildId);
            if (guild)
                if (guild->DelMember(guid))
                    deletedGuild = guildId;
        }

        // Delete Friend List
        // Cleanup friends for online players
        if (QueryResult* resultFriend = CharacterDatabase.PQuery("SELECT DISTINCT guid FROM character_social WHERE friend = '%u'", guid.GetCounter()))
        {
            do
            {
                Field* fieldsFriend = resultFriend->Fetch();
                if (Player* sFriend = sObjectAccessor.FindPlayer(ObjectGuid(HIGHGUID_PLAYER, fieldsFriend[0].GetUInt32())))
                {
                    if (sFriend->IsInWorld())
                    {
                        sFriend->GetSocial()->RemoveFromSocialList(guid, false, true);
                        sSocialMgr.SendFriendStatus(sFriend, FRIEND_REMOVED, guid, false);
                    }
                }
            }
            while (resultFriend->NextRow());
            delete resultFriend;
        }
        // Cleanup friends for offline players
        CharacterDatabase.PExecute("DELETE FROM character_social WHERE guid = '%u' OR friend='%u'", guid.GetCounter(), guid.GetCounter());
        // Leave Arena Teams
        Player::LeaveAllArenaTeams(guid);
        // Remove signs from petitions (also remove petitions if owner)
        // NOTE: This is the same as call Player::RemovePetitionsAndSigns(guid, 10); but this can't be called in a Transaction because it initialize another one!!
        if (QueryResult *result = CharacterDatabase.PQuery("SELECT ownerguid,petitionguid FROM petition_sign WHERE playerguid = '%u'", guid.GetCounter()))
        {
            do // this part effectively does nothing, since the deletion / modification only takes place _after_ the PetitionQuery. Though I don't know if the result remains intact if I execute the delete query beforehand.
            { // and SendPetitionQueryOpcode reads data from the DB
                Field *fields = result->Fetch();
                ObjectGuid ownerguid = ObjectGuid(HIGHGUID_PLAYER, fields[0].GetUInt32());
                ObjectGuid petitionguid = ObjectGuid(HIGHGUID_ITEM, fields[1].GetUInt32());

                // send update if charter owner in game
                Player* owner = sObjectMgr.GetPlayer(ownerguid);
                if (owner)
                    owner->GetSession()->SendPetitionQueryOpcode(petitionguid);
            }
            while (result->NextRow());

            delete result;

            CharacterDatabase.PExecute("DELETE FROM petition_sign WHERE playerguid = '%u'", guid.GetCounter());
        }
        CharacterDatabase.PExecute("DELETE FROM petition WHERE ownerguid = '%u'", guid.GetCounter());
        CharacterDatabase.PExecute("DELETE FROM petition_sign WHERE ownerguid = '%u'", guid.GetCounter());
        // Reset Language (will be added automatically after faction change)
        CharacterDatabase.PExecute("DELETE FROM `character_spell` WHERE `spell` IN (668, 7340, 671, 672, 814, 29932, 17737, 816, 7341, 669, 813, 670) AND guid ='%u'", guid.GetCounter());

        // Reset homebind
        CharacterDatabase.PExecute("DELETE FROM `character_homebind` WHERE guid = '%u'", guid.GetCounter());
        if (newTeam == TEAM_INDEX_ALLIANCE)
            CharacterDatabase.PExecute("INSERT INTO `character_homebind` VALUES ('%u','0','1519','-8867.68','673.373','97.9034')", guid.GetCounter());
        else
            CharacterDatabase.PExecute("INSERT INTO `character_homebind` VALUES ('%u','1','1637','1633.33','-4439.11','15.7588')", guid.GetCounter());

        // Achievement conversion
        if (QueryResult *result2 = WorldDatabase.Query("SELECT alliance_id, horde_id FROM player_factionchange_achievements"))
        {
            do
            {
                Field *fields2 = result2->Fetch();
                uint32 achiev_alliance = fields2[0].GetUInt32();
                uint32 achiev_horde = fields2[1].GetUInt32();
                CharacterDatabase.PExecute("UPDATE IGNORE `character_achievement` set achievement = '%u' where achievement = '%u' AND guid = '%u'",
                    newTeam == TEAM_INDEX_ALLIANCE ? achiev_alliance : achiev_horde, newTeam == TEAM_INDEX_ALLIANCE ? achiev_horde : achiev_alliance, guid.GetCounter());
            }
            while (result2->NextRow());
        }

        // Item conversion
        if (QueryResult *result2 = WorldDatabase.Query("SELECT alliance_id, horde_id FROM player_factionchange_items"))
        {
            do
            {
                Field *fields2 = result2->Fetch();
                uint32 item_alliance = fields2[0].GetUInt32();
                uint32 item_horde = fields2[1].GetUInt32();
                CharacterDatabase.PExecute("UPDATE IGNORE `character_inventory` set item = '%u' where item = '%u' AND guid = '%u'",
                    newTeam == TEAM_INDEX_ALLIANCE ? item_alliance : item_horde, newTeam == TEAM_INDEX_ALLIANCE ? item_horde : item_alliance, guid.GetCounter());

                CharacterDatabase.PExecute("UPDATE IGNORE `item_instance` SET `data`=CONCAT(CAST(SUBSTRING_INDEX(`data`, ' ', 3) AS CHAR), ' ', '%u', ' ', CAST(SUBSTRING_INDEX(TRIM(`data`), ' ', (4-64))AS CHAR)) WHERE CAST(SUBSTRING_INDEX(SUBSTRING_INDEX(`data`, ' ', 4), ' ', '-1') AS UNSIGNED) = '%u' AND owner_guid = '%u'",
                        newTeam == TEAM_INDEX_ALLIANCE ? item_alliance : item_horde, newTeam == TEAM_INDEX_ALLIANCE ? item_horde : item_alliance, guid.GetCounter());
            }
            while (result2->NextRow());
        }

        // Spell conversion
        if (QueryResult *result2 = WorldDatabase.Query("SELECT alliance_id, horde_id FROM player_factionchange_spells"))
        {
            do
            {
                Field *fields2 = result2->Fetch();
                uint32 spell_alliance = fields2[0].GetUInt32();
                uint32 spell_horde = fields2[1].GetUInt32();
                CharacterDatabase.PExecute("UPDATE IGNORE `character_spell` set spell = '%u' where spell = '%u' AND guid = '%u'",
                    newTeam == TEAM_INDEX_ALLIANCE ? spell_alliance : spell_horde, newTeam == TEAM_INDEX_ALLIANCE ? spell_horde : spell_alliance, guid.GetCounter());
            }
            while (result2->NextRow());
        }

        // Reputation conversion
        if (QueryResult *result2 = WorldDatabase.Query("SELECT alliance_id, horde_id FROM player_factionchange_reputations"))
        {
            do
            {
                Field *fields2 = result2->Fetch();
                uint32 reputation_alliance = fields2[0].GetUInt32();
                uint32 reputation_horde = fields2[1].GetUInt32();
                CharacterDatabase.PExecute("DELETE FROM character_reputation WHERE faction = '%u' AND guid = '%u'", newTeam == TEAM_INDEX_ALLIANCE ? reputation_alliance : reputation_horde, guid.GetCounter());
                CharacterDatabase.PExecute("UPDATE IGNORE `character_reputation` set faction = '%u' where faction = '%u' AND guid = '%u'",
                    newTeam == TEAM_INDEX_ALLIANCE ? reputation_alliance : reputation_horde, newTeam == TEAM_INDEX_ALLIANCE ? reputation_horde : reputation_alliance, guid.GetCounter());
            }
            while (result2->NextRow());
        }

        // Quest conversion
        if (QueryResult *result2 = WorldDatabase.Query("SELECT alliance_id, horde_id FROM player_factionchange_quests"))
        {
            do
            {
                Field *fields2 = result2->Fetch();
                uint32 quest_alliance = fields2[0].GetUInt32();
                uint32 quest_horde = fields2[1].GetUInt32();
                CharacterDatabase.PExecute("UPDATE IGNORE `character_queststatus` SET quest = '%u' WHERE quest = '%u' AND guid = '%u'",
                    newTeam == TEAM_INDEX_ALLIANCE ? quest_alliance : quest_horde, newTeam == TEAM_INDEX_ALLIANCE ? quest_horde : quest_alliance, guid.GetCounter());
            }
            while (result2->NextRow());
        }
    }
    CharacterDatabase.CommitTransaction();

    if (deletedGuild)
    {
        Guild* guild = sGuildMgr.GetGuildById(deletedGuild);
        if (guild)
        {
            guild->Disband();
            delete guild;
        }
    }

    std::string IP_str = GetRemoteAddress();
    sLog.outChar("Account: %d (IP: %s), Character guid: %u Change Race/Faction to: %s", GetAccountId(), IP_str.c_str(), guid.GetCounter(), newname.c_str());

    WorldPacket data(SMSG_CHAR_FACTION_CHANGE, 1 + 8 + (newname.size() + 1) + 7);
    data << uint8(RESPONSE_SUCCESS);
    data << ObjectGuid(guid);
    data << newname;
    data << uint8(gender);
    data << uint8(skin);
    data << uint8(face);
    data << uint8(hairStyle);
    data << uint8(hairColor);
    data << uint8(facialHair);
    data << uint8(newRace);
    SendPacket(&data);
}

void WorldSession::HandleCharCustomizeOpcode(WorldPacket& recv_data)
{
    ObjectGuid guid;
    std::string newname;

    recv_data >> guid;
    recv_data >> newname;

    uint8 gender, skin, face, hairStyle, hairColor, facialHair;
    recv_data >> gender >> skin >> hairColor >> hairStyle >> facialHair >> face;

    QueryResult *result = CharacterDatabase.PQuery("SELECT at_login FROM characters WHERE guid ='%u'", guid.GetCounter());
    if (!result)
    {
        WorldPacket data(SMSG_CHAR_CUSTOMIZE, 1);
        data << uint8(CHAR_CREATE_ERROR);
        SendPacket( &data );
        return;
    }

    Field *fields = result->Fetch();
    uint32 at_loginFlags = fields[0].GetUInt32();
    delete result;

    if (!(at_loginFlags & AT_LOGIN_CUSTOMIZE))
    {
        WorldPacket data(SMSG_CHAR_CUSTOMIZE, 1);
        data << uint8(CHAR_CREATE_ERROR);
        SendPacket( &data );
        return;
    }

    // prevent character rename to invalid name
    if (!normalizePlayerName(newname))
    {
        WorldPacket data(SMSG_CHAR_CUSTOMIZE, 1);
        data << uint8(CHAR_NAME_NO_NAME);
        SendPacket( &data );
        return;
    }

    uint8 res = ObjectMgr::CheckPlayerName(newname,true);
    if (res != CHAR_NAME_SUCCESS)
    {
        WorldPacket data(SMSG_CHAR_CUSTOMIZE, 1);
        data << uint8(res);
        SendPacket( &data );
        return;
    }

    // check name limitations
    if (GetSecurity() == SEC_PLAYER && sObjectMgr.IsReservedName(newname))
    {
        WorldPacket data(SMSG_CHAR_CUSTOMIZE, 1);
        data << uint8(CHAR_NAME_RESERVED);
        SendPacket(&data);
        return;
    }

    // character with this name already exist
    ObjectGuid newguid = sAccountMgr.GetPlayerGuidByName(newname);
    if (newguid && newguid != guid)
    {
        WorldPacket data(SMSG_CHAR_CUSTOMIZE, 1);
        data << uint8(CHAR_CREATE_NAME_IN_USE);
        SendPacket(&data);
        return;
    }

    CharacterDatabase.escape_string(newname);
    Player::Customize(guid, gender, skin, face, hairStyle, hairColor, facialHair);
    CharacterDatabase.PExecute("UPDATE characters set name = '%s', at_login = at_login & ~ %u WHERE guid ='%u'", newname.c_str(), uint32(AT_LOGIN_CUSTOMIZE), guid.GetCounter());
    CharacterDatabase.PExecute("DELETE FROM character_declinedname WHERE guid ='%u'", guid.GetCounter());

    std::string IP_str = GetRemoteAddress();
    sLog.outChar("Account: %d (IP: %s), Character %s customized to: %s", GetAccountId(), IP_str.c_str(), guid.GetString().c_str(), newname.c_str());

    WorldPacket data(SMSG_CHAR_CUSTOMIZE, 1 + 8 + (newname.size() + 1) + 6);
    data << uint8(RESPONSE_SUCCESS);
    data << ObjectGuid(guid);
    data << newname;
    data << uint8(gender);
    data << uint8(skin);
    data << uint8(face);
    data << uint8(hairStyle);
    data << uint8(hairColor);
    data << uint8(facialHair);
    SendPacket(&data);
}

void WorldSession::HandleEquipmentSetSaveOpcode(WorldPacket& recv_data)
{
    DEBUG_LOG("CMSG_EQUIPMENT_SET_SAVE");

    ObjectGuid setGuid;
    uint32 index;
    std::string name;
    std::string iconName;

    recv_data >> setGuid.ReadAsPacked();
    recv_data >> index;
    recv_data >> name;
    recv_data >> iconName;

    if (index >= MAX_EQUIPMENT_SET_INDEX)                   // client set slots amount
        return;

    EquipmentSet eqSet;

    eqSet.Guid      = setGuid.GetRawValue();
    eqSet.Name      = name;
    eqSet.IconName  = iconName;
    eqSet.state     = EQUIPMENT_SET_NEW;

    for (uint32 i = 0; i < EQUIPMENT_SLOT_END; ++i)
    {
        ObjectGuid itemGuid;

        recv_data >> itemGuid.ReadAsPacked();

        // equipment manager sends "1" (as raw GUID) for slots set to "ignore" (not touch slot at equip set)
        if (itemGuid.GetRawValue() == 1)
        {
            // ignored slots saved as bit mask because we have no free special values for Items[i]
            eqSet.IgnoreMask |= 1 << i;
            continue;
        }

        Item* item = _player->GetItemByPos(INVENTORY_SLOT_BAG_0, i);

        if (!item && itemGuid)                              // cheating check 1
            return;

        if (item && item->GetObjectGuid() != itemGuid)      // cheating check 2
            return;

        eqSet.Items[i] = itemGuid.GetCounter();
    }

    _player->SetEquipmentSet(index, eqSet);
}

void WorldSession::HandleEquipmentSetDeleteOpcode(WorldPacket& recv_data)
{
    DEBUG_LOG("CMSG_EQUIPMENT_SET_DELETE");

    ObjectGuid setGuid;

    recv_data >> setGuid.ReadAsPacked();

    _player->DeleteEquipmentSet(setGuid.GetRawValue());
}

void WorldSession::HandleEquipmentSetUseOpcode(WorldPacket& recv_data)
{
    DEBUG_LOG("CMSG_EQUIPMENT_SET_USE");
    recv_data.hexlike();

    for (uint32 i = 0; i < EQUIPMENT_SLOT_END; ++i)
    {
        ObjectGuid itemGuid;
        uint8 srcbag, srcslot;

        recv_data >> itemGuid.ReadAsPacked();
        recv_data >> srcbag >> srcslot;

        DEBUG_LOG("Item (%s): srcbag %u, srcslot %u", itemGuid.GetString().c_str(), srcbag, srcslot);

        // check if item slot is set to "ignored" (raw value == 1), must not be unequipped then
        if (itemGuid.GetRawValue() == 1)
            continue;

        Item* item = _player->GetItemByGuid(itemGuid);

        uint16 dstpos = i | (INVENTORY_SLOT_BAG_0 << 8);

        if (!item)
        {
            Item* uItem = _player->GetItemByPos(INVENTORY_SLOT_BAG_0, i);
            if (!uItem)
                continue;

            ItemPosCountVec sDest;
            InventoryResult msg = _player->CanStoreItem(NULL_BAG, NULL_SLOT, sDest, uItem, false);
            if (msg == EQUIP_ERR_OK)
            {
                _player->RemoveItem(INVENTORY_SLOT_BAG_0, i, true);
                _player->StoreItem(sDest, uItem, true);
            }
            else
                _player->SendEquipError(msg, uItem, NULL);

            continue;
        }

        if (item->GetPos() == dstpos)
            continue;

        _player->SwapItem(item->GetPos(), dstpos);
    }

    WorldPacket data(SMSG_USE_EQUIPMENT_SET_RESULT, 1);
    data << uint8(0);                                       // 4 - equipment swap failed - inventory is full
    SendPacket(&data);
}

void WorldSession::HandleReorderCharactersOpcode(WorldPacket& recv_data)
{
    uint32 charCount = recv_data.ReadBits(10);

    if (charCount > sWorld.getConfig(CONFIG_UINT32_CHARACTERS_PER_REALM))
    {
        DEBUG_LOG("SESSION: received CMSG_REORDER_CHARACTERS, but characters count %u is beyond server limit.", charCount);
        recv_data.rfinish();
        return;
    }

    std::vector<ObjectGuid> guids;
    std::vector<uint8> slots;

    for (uint32 i = 0; i < charCount; ++i)
    {
        ObjectGuid guid;
        recv_data.ReadGuidMask<1, 4, 5, 3, 0, 7, 6, 2>(guid);
        guids.push_back(guid);
    }

    for (uint32 i = 0; i < charCount; ++i)
    {
        recv_data.ReadGuidBytes<6, 5, 1, 4, 0, 3>(guids[i]);
        slots.push_back(recv_data.ReadUInt8());
        recv_data.ReadGuidBytes<2, 7>(guids[i]);
    }

    CharacterDatabase.BeginTransaction();
    for (uint32 i = 0; i < charCount; ++i)
        CharacterDatabase.PExecute("UPDATE `characters` SET `slot` = '%u' WHERE `guid` = '%u' AND `account` = '%u'",
        slots[i], guids[i].GetCounter(), GetAccountId());
    CharacterDatabase.CommitTransaction();
}

void WorldSession::HandleSetCurrencyFlagsOpcode(WorldPacket& recv_data)
{
    uint32 currencyId, flags;
    recv_data >> flags >> currencyId;

    DEBUG_LOG("CMSG_SET_CURRENCY_FLAGS: currency: %u, flags: %u", currencyId, flags);

    if (flags & ~PLAYERCURRENCY_MASK_USED_BY_CLIENT)
    {
        DEBUG_LOG("CMSG_SET_CURRENCY_FLAGS: received unknown currency flags 0x%X from player %s account %u for currency %u",
            flags & ~PLAYERCURRENCY_MASK_USED_BY_CLIENT, GetPlayer()->GetGuidStr().c_str(), GetAccountId(), currencyId);
    }

    flags &= PLAYERCURRENCY_MASK_USED_BY_CLIENT;
    GetPlayer()->SetCurrencyFlags(currencyId, uint8(flags));
}

