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

#ifndef MANGOS_DBCSFRM_H
#define MANGOS_DBCSFRM_H

const char Achievementfmt[]="niiissiiiiisiix"; // 16357
const char AchievementCriteriafmt[]="niiiiiiiixsiiiiixxxxxxx";
const char AreaTableEntryfmt[]="iiinixxxxxxxisiiiiixxxxxxxxxxx"; // 16357
const char AreaGroupEntryfmt[]="niiiiiii";
const char AreaTriggerEntryfmt[]="nifffxxxfffffxxx"; // 16357
const char ArmorLocationfmt[]="nfffff";
const char AuctionHouseEntryfmt[]="niiix";
const char BankBagSlotPricesEntryfmt[]="ni";
const char BarberShopStyleEntryfmt[]="nixxxiii";
const char BattlemasterListEntryfmt[]="niiiiiiiiiiixsiiiiiiii"; // 16357
const char CharStartOutfitEntryfmt[]="diiiiiiiiiiiiiiiiiiiiiiiiixxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx";
const char CharTitlesEntryfmt[]="nxsxix";
const char ChatChannelsEntryfmt[]="iixsx";
                                                            // ChatChannelsEntryfmt, index not used (more compact store)
const char ChrClassesEntryfmt[]="nixsxxxixiiiixxxxx"; // 16357
const char ChrRacesEntryfmt[]="nxixiixixxxxixsxxxxxixxxxxxxxxxxxxxx"; // 16357
const char ChrClassesXPowerTypesfmt[]="nii";
const char CinematicSequencesEntryfmt[]="nxxxxxxxxx";
const char CreatureDisplayInfofmt[]="nxxifxxxxxxxxxxxxxx"; // 16357
const char CreatureDisplayInfoExtrafmt[]="nixxxxxxxxxxxxxxxxxxx";
const char CreatureFamilyfmt[]="nfifiiiiixsx";
const char CreatureModelDatafmt[] = "nxxifxxxxxxxxxxffxxxxxxxxxxxxxx";
const char CreatureSpellDatafmt[]="niiiixxxx";
const char CreatureTypefmt[]="nxx";
const char CurrencyTypesfmt[]="nisxxxxiiix";
const char DestructibleModelDataFmt[] = "nixxxixxxxixxxxixxxxixxx";
const char DungeonEncounterfmt[]="niiiisxx";
const char DurabilityCostsfmt[]="niiiiiiiiiiiiiiiiiiiiiiiiiiiii";
const char DurabilityQualityfmt[]="nf";
const char EmotesEntryfmt[]="nxxiiixx";
const char EmotesTextEntryfmt[]="nxixxxxxxxxxxxxxxxx";
const char FactionEntryfmt[]="niiiiiiiiiiiiiiiiiiffixsxx";
const char FactionTemplateEntryfmt[]="niiiiiiiiiiiii";
const char GameObjectDisplayInfofmt[]="nxxxxxxxxxxxffffffxxx";
const char GemPropertiesEntryfmt[]="nixxix";
const char GlyphPropertiesfmt[]="niii";
const char GlyphSlotfmt[]="nii";
const char GtBarberShopCostBasefmt[]="xf";
const char GtCombatRatingsfmt[]="xf";
const char GtChanceToMeleeCritBasefmt[]="xf";
const char GtChanceToMeleeCritfmt[]="xf";
const char GtChanceToSpellCritBasefmt[]="xf";
const char GtOCTClassCombatRatingScalarfmt[]="df";
const char GtChanceToSpellCritfmt[]="xf";
const char GtOCTHpPerStaminafmt[]="df"; // 16357
const char GtOCTRegenHPfmt[]="xf";
const char GtRegenHPPerSptfmt[]="xf";
const char GtRegenMPPerSptfmt[]="xf";
const char GtSpellScalingfmt[]="df";
const char GtOCTBaseHPByClassfmt[]="df";
const char GtOCTBaseMPByClassfmt[]="df";
const char Holidaysfmt[]="nxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx";
const char ItemClassfmt[]="nxfs"; // 16357
const char ItemArmorQualityfmt[]="nfffffffi";
const char ItemArmorShieldfmt[]="nifffffff";
const char ItemArmorTotalfmt[]="niffff";
const char ItemBagFamilyfmt[]="nx";
//const char ItemDisplayTemplateEntryfmt[]="nxxxxxxxxxxixxxxxxxxxxx";
//const char ItemCondExtCostsEntryfmt[]="xiii";
//const char ItemExtendedCostEntryfmt[]="niiiiiiiiiiiiiix";
const char ItemDamagefmt[]="nfffffffi";
const char ItemLimitCategoryEntryfmt[]="nxii";
const char ItemRandomPropertiesfmt[]="nxiiiiis";
const char ItemRandomSuffixfmt[]="nsxiiiiiiiiii";
const char ItemSetEntryfmt[]="dsxxxxxxxxxxxxxxxxxiiiiiiiiiiiiiiiiii";
const char LFGDungeonEntryfmt[] = "nxiiiiiiiiiixiiixixxx";
const char LFGDungeonExpansionEntryfmt[] = "niiiiiii";
const char LiquidTypefmt[] = "nxxixixxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx";
const char LockEntryfmt[]="niiiiiiiiiiiiiiiiiiiiiiiixxxxxxxx";
const char MailTemplateEntryfmt[]="nxs";
const char MapEntryfmt[]="nsiiisissififfiiiii"; // 16357
const char MapDifficultyEntryfmt[]="niisiis";
const char MovieEntryfmt[]="nxxxx"; // 16357
const char MountCapabilityfmt[]="niiiiiii";
const char MountTypefmt[]="niiiiiiiiiiiiiiiiiiiiiiii";
const char NumTalentsAtLevelfmt[]="df";
const char OverrideSpellDatafmt[]="niiiiiiiiiixx";
const char QuestFactionRewardfmt[]="niiiiiiiiii";
const char QuestSortEntryfmt[]="nx";
const char QuestXPLevelfmt[]="niiiiiiiiii";
const char Phasefmt[]="nii";
const char PvPDifficultyfmt[]="diiiii";
const char RandomPropertiesPointsfmt[]="niiiiiiiiiiiiiii";
const char ScalingStatDistributionfmt[]="niiiiiiiiiiiiiiiiiiiixi";
const char ScalingStatValuesfmt[]="iniiiiiixiiiiiiiiiiiiiixxxxxxxxxxxxxxxxxxxxxxxxx"; // 16357
const char SkillLinefmt[]="nisxixixx"; // 16357
const char SkillLineAbilityfmt[]="niiiiiiiiixxx"; // 16357
const char SkillRaceClassInfofmt[]="diiiiixx"; // 16357
const char SoundEntriesfmt[]="nissssssssssssssssssssssxxxxxxxxxxxx"; // 16357
const char SpellCastTimefmt[]="niii";
const char SpellDurationfmt[]="niii";
const char SpellDifficultyfmt[]="niiii";
const char SpellEntryfmt[]="nssxxixxxiiiiiiiiiiiiiixi"; // 16357
const char SpellAuraOptionsEntryfmt[]="dxxiiii"; // 16357
const char SpellAuraRestrictionsEntryfmt[]="dxxiiiiiiii"; // 16357
const char SpellCastingRequirementsEntryfmt[]="dixxixi";
const char SpellCategoriesEntryfmt[]="dxxiiiiiix"; // 16357
const char SpellClassOptionsEntryfmt[]="dxiiiix";
const char SpellCooldownsEntryfmt[]="dxxiii"; // 16357
const char SpellEffectEntryfmt[]="dxifiiiffiiiiiifiifiiiiixiiiix"; // 16357
const char SpellEquippedItemsEntryfmt[]="dxxiii"; // 16357
const char SpellInterruptsEntryfmt[]="dxxixixi"; // 16357
const char SpellLevelsEntryfmt[]="dxxiii"; // 16357
const char SpellPowerEntryfmt[]="xnxiiiiixxxxx"; // 16357
const char SpellReagentsEntryfmt[]="diiiiiiiiiiiiiiiixx"; // 16357
const char SpellScalingEntryfmt[]="diiiiffffffffffi"; // FIXME
const char SpellShapeshiftEntryfmt[]="dixixx"; // 16357
const char SpellTargetRestrictionsEntryfmt[]="dxxfxiiii"; // 16357
const char SpellTotemsEntryfmt[]="diiii";
const char SpellFocusObjectfmt[]="nx";
const char SpellItemEnchantmentfmt[]="nxiiiiiiiiisiiiixxixxxxxx"; // 16357
const char SpellItemEnchantmentConditionfmt[]="nbbbbbxxxxxbbbbbbbbbbiiiiiXXXXX";
const char SpellMiscfmt[]="dxxiiiiiiiiiiiiiiifiiiii"; // 16357
const char SpellRadiusfmt[]="nfxxx"; // 16357
const char SpellRangefmt[]="nffffxxx";
const char SpellRuneCostfmt[]="niiixi"; // 16357
const char SpellShapeshiftFormfmt[]="nxxiixiiixxiiiiiiiixx";
//const char StableSlotPricesfmt[] = "ni"; // removed
const char SummonPropertiesfmt[] = "niiiii";
const char TalentEntryfmt[]="niiiiiixxixxixxxxxx";
const char TalentTabEntryfmt[]="nxxiiixxiii";
const char TalentTreePrimarySpellsfmt[]="diix";
const char TaxiNodesEntryfmt[]="nifffsiixxx";
const char TaxiPathEntryfmt[]="niii";
const char TaxiPathNodeEntryfmt[]="diiifffiiii";
const char TotemCategoryEntryfmt[]="nxii";
const char TransportAnimationEntryfmt[]="diixxxx";
const char VehicleEntryfmt[]="nixffffiiiiiiiifffffffffffffffssssfifiixx"; // 16357
const char VehicleSeatEntryfmt[]="niiffffffffffiiiiiifffffffiiifffiiiiiiiffiiiiixxxxxxxxxxxxxxxxxxxx";
const char WMOAreaTableEntryfmt[]="niiixxxxxiixxxx";
const char WorldMapAreaEntryfmt[]="xinxffffixxxxx";
const char WorldMapOverlayEntryfmt[]="nxiiiixxxxxxxxxx"; // 16357
const char WorldSafeLocsEntryfmt[]="nifffxx"; // 16357
const char WorldStateEntryfmt[]="niiixxxssiixssiix";
const char WorldPvPAreaEnrtyfmt[]="niiiiii";

#endif
