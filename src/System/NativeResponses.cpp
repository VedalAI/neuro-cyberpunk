#include <Shared/Raw/JournalManager/JournalManager.hpp>
#include <Shared/Raw/LocalizationSystem/LocalizationSystem.hpp>
#include <Shared/Raw/MappinSystem/MappinSystem.hpp>
#include <Shared/Raw/ScriptableSystem/ScriptableSystem.hpp>
#include <Shared/Util/Enums.hpp>

#include <System/NativeResponses.hpp>

#include <RED4ext/Scripting/Natives/Generated/game/FastTravelPointData.hpp>
#include <RED4ext/Scripting/Natives/Generated/game/JournalEntryState.hpp>
#include <RED4ext/Scripting/Natives/Generated/game/JournalPath.hpp>
#include <RED4ext/Scripting/Natives/Generated/game/JournalPointOfInterestMappin.hpp>
#include <RED4ext/Scripting/Natives/Generated/game/JournalQuest.hpp>
#include <RED4ext/Scripting/Natives/Generated/game/JournalQuestMapPin.hpp>
#include <RED4ext/Scripting/Natives/Generated/game/JournalQuestObjective.hpp>
#include <RED4ext/Scripting/Natives/Generated/game/JournalQuestPhase.hpp>
#include <RED4ext/Scripting/Natives/Generated/game/data/District_Record.hpp>
#include <RED4ext/Scripting/Natives/Generated/game/mappins/FastTravelMappin.hpp>
#include <RED4ext/Scripting/Natives/Generated/game/mappins/MappinSystem.hpp>
#include <RED4ext/Scripting/Natives/Generated/game/mappins/PointOfInterestMappin.hpp>
#include <RED4ext/Scripting/Natives/Generated/game/mappins/QuestMappin.hpp>

#include <tsl/hopscotch_map.h>

#include <glaze/glaze.hpp>

using namespace Red;
namespace Impl
{
using DistrictNameMap = tsl::hopscotch_map<TweakDBID, CString>;

// Note: doesn't account for changing lang in the middle of game
DistrictNameMap GetCachedDistrictNames()
{
    static auto Localization = shared::raw::Localization::LocalizationSystem::GetInstance();

    auto districtRecordType = GetClass<District_Record>();

    tsl::hopscotch_map<TweakDBID, CString> ret{};

    for (auto record : TweakDB::Get()->GetRecordsByType(districtRecordType))
    {
        if (const auto asTdb = Cast<TweakDBRecord>(record))
        {
            CString displayName{};
            TweakDB::Get()->TryGetValue({asTdb->recordID, ".localizedName"}, displayName);

            auto translated = Localization->GetOnscreen(displayName);

            ret.insert_or_assign(asTdb->recordID, std::move(translated));
        }
    }

    return ret;
}

/**
 * \brief Tries to get the name of a quest, point of interest or fast travel mappin.
 *
 * \param aMappin The target mappin.
 * \return A localized name or an empty string in case of an error.
 */
CString GetMappinDisplayName(Handle<IMappin>& aMappin)
{
    static auto Localization = shared::raw::Localization::LocalizationSystem::GetInstance();

    auto& displayName = *shared::raw::Mappin::GetDisplayName(aMappin);
    if (displayName.Length() > 0u)
    {
        return Localization->GetOnscreen(displayName);
    }

    const auto mappinPhase = shared::raw::Mappin::GetMappinPhase(aMappin);

    // Yoink from game
    switch (shared::raw::Mappin::GetMappinVariant(aMappin))
    {
    case game::data::MappinVariant::QuestGiverVariant:
    case game::data::MappinVariant::RetrievingVariant:
    case game::data::MappinVariant::HuntForPsychoVariant:
    case game::data::MappinVariant::ThieveryVariant:
    case game::data::MappinVariant::SabotageVariant:
    case game::data::MappinVariant::ClientInDistressVariant:
    case game::data::MappinVariant::CourierVariant:
    case game::data::MappinVariant::Zzz09_CourierSandboxActivityVariant:
    case game::data::MappinVariant::BountyHuntVariant:
    case game::data::MappinVariant::ConvoyVariant:
    case game::data::MappinVariant::Zzz02_MotorcycleForPurchaseVariant:
    case game::data::MappinVariant::Zzz06_NCPDGigVariant:
    case game::data::MappinVariant::Zzz05_ApartmentToPurchaseVariant:
    case game::data::MappinVariant::Zzz12_WorldEncounterVariant:
    {
        if (mappinPhase == game::data::MappinPhase::UndiscoveredPhase)
        {
            return "Undiscovered gig";
        }
        break;
    }
    default:
        break;
    }

    if (mappinPhase == game::data::MappinPhase::UndiscoveredPhase)
    {
        return "Undiscovered";
    }

    if (const auto& fastTravelMapPin = Cast<FastTravelMappin>(aMappin))
    {
        const auto& pointData = shared::raw::FastTravelMappin::PointData::Ref(fastTravelMapPin);
        CString displayName{};
        TweakDB::Get()->TryGetValue({pointData->pointRecord, ".displayName"}, displayName);
        return Localization->GetOnscreen(displayName);
    }

    auto journalManager = GetGameSystem<JournalManager>();

    if (const auto& questMappin = Cast<QuestMappin>(aMappin))
    {
        uint32_t pathHash = shared::raw::QuestMappin::JournalPathHash::Ref(questMappin);

        Handle<JournalEntry> entry{};

        shared::raw::JournalManager::GetEntryByHash(journalManager, entry, pathHash);

        if (entry)
        {
            if (const auto& asQuestMapPin = Cast<game::JournalQuestMapPin>(entry))
            {
                Handle<JournalEntry> objEntry{};
                shared::raw::JournalManager::GetParentEntry(journalManager, objEntry, entry);
                if (auto entry = Cast<JournalQuestObjective>(objEntry))
                {
                    if (shared::raw::JournalManager::GetEntryState(journalManager, entry) ==
                        game::JournalEntryState::Inactive)
                    {
                        return "Undiscovered quest...";
                    }
                    Handle<JournalEntry> objPhase{};
                    shared::raw::JournalManager::GetParentEntry(journalManager, objPhase, objEntry);
                    if (auto phase = Cast<JournalQuestPhase>(objPhase))
                    {
                        if (shared::raw::JournalManager::GetEntryState(journalManager, phase) ==
                            game::JournalEntryState::Inactive)
                        {
                            return "Undiscovered quest...";
                        }
                        Handle<JournalEntry> objQuest{};
                        shared::raw::JournalManager::GetParentEntry(journalManager, objQuest, objPhase);
                        if (auto quest = Cast<JournalQuest>(objQuest))
                        {
                            if (shared::raw::JournalManager::GetEntryState(journalManager, quest) ==
                                game::JournalEntryState::Inactive)
                            {
                                return "Undiscovered quest...";
                            }
                            return Localization->GetOnscreen(quest->title.unk08);
                        }
                    }
                }
            }
        }
    }
    else if (const auto& poiMappin = Cast<PointOfInterestMappin>(aMappin))
    {
        uint32_t pathHash = shared::raw::PoiMappin::JournalPathHash::Ref(poiMappin);

        Handle<JournalEntry> entry{};

        shared::raw::JournalManager::GetEntryByHash(journalManager, entry, pathHash);

        if (entry)
        {
            if (const auto& asPoiMappin = Cast<game::JournalPointOfInterestMappin>(entry))
            {
                if (asPoiMappin->questPath)
                {
                    const auto questHash = Murmur3_32(asPoiMappin->questPath->realPath.c_str());
                    Handle<JournalEntry> questEntry{};

                    shared::raw::JournalManager::GetEntryByHash(journalManager, questEntry, questHash);

                    if (const auto& quest = Cast<JournalQuest>(questEntry))
                    {
                        if (shared::raw::JournalManager::GetEntryState(journalManager, questEntry) ==
                            game::JournalEntryState::Inactive)
                        {
                            return "Undiscovered quest...";
                        }
                        return Localization->GetOnscreen(quest->title.unk08);
                    }
                }
            }
        }
    }

    return "";
}
} // namespace Impl

/**
 * \brief A DTO object for easier handling of Glaze JSON library. Satisfies the schema:
 *
 * {"id", "name", "type", "district", "tracked", "distance"}
 */
struct NeuroMappinData
{
    // The mappin ID
    uint64_t id{};

    // The display name of the mappin
    std::string name{};

    std::string district{};

    std::string type{};

    bool tracked{};

    float distance{};
};

CString mod::NeuroResponses::CreateMappinQueryResponse()
{
    std::vector<Handle<IMappin>> mappins{};

    {
        auto mappinSystem = GetGameSystem<MappinSystem>();

        std::unique_lock lock(shared::raw::MappinSystem::MappinLock::Ref(mappinSystem));
        auto& mappinList = shared::raw::MappinSystem::MappinList::Ref(mappinSystem);

        mappins.reserve(mappinList.size);

        for (auto& i : mappinList)
        {
            mappins.push_back(i.m_mappin);
        }
    }

    // Maybe Glaze can be set up to serialize REDengine data structures properly?
    std::vector<NeuroMappinData> serializableData{};

    static auto DistrictTDBIDToLocalizedNameMap = Impl::GetCachedDistrictNames();
    static auto LocalizationManager = shared::raw::Localization::LocalizationSystem::GetInstance();

    for (auto& mappin : mappins)
    {
        if (!shared::raw::Mappin::IsActive(mappin))
        {
            continue;
        }

        const auto mappinPhase = shared::raw::Mappin::GetMappinPhase(mappin);

        auto id = shared::raw::Mappin::MappinID::Ref(mappin);

        NeuroMappinData mappinDataDto{.id = id.value,
                                      .name = Impl::GetMappinDisplayName(mappin).c_str(),
                                      .distance = shared::raw::Mappin::Distance::Ref(mappin)};

        if (const auto& fastTravelMapPin = Cast<FastTravelMappin>(mappin))
        {
            const auto& pointData = shared::raw::FastTravelMappin::PointData::Ref(fastTravelMapPin);
            TweakDBID districtTdbid{};
            TweakDB::Get()->TryGetValue({pointData->pointRecord, ".district"}, districtTdbid);

            if (DistrictTDBIDToLocalizedNameMap.contains(districtTdbid))
            {
                mappinDataDto.district = DistrictTDBIDToLocalizedNameMap.at(districtTdbid).c_str();
            }
        }
        else if (mappinPhase == game::data::MappinPhase::CompletedPhase || !shared::raw::Mappin::IsStatic(mappin))
        {
            // Hack: all fast travel mappins are in some weird state?
            continue;
        }

        static auto MappinVariantEnum = GetEnum<game::data::MappinVariant>();

        mappinDataDto.tracked = shared::raw::Mappin::IsTrackedByPlayer::Ref(mappin);
        mappinDataDto.type =
            shared::util::EnumValueToString(MappinVariantEnum, (int64_t)shared::raw::Mappin::GetMappinVariant(mappin))
                .ToString();

        serializableData.push_back(std::move(mappinDataDto));
    }

    auto data = glz::write_json(serializableData).value_or("Failed to serialize waypoint data to JSON");
    // Note: maybe just return std::string? Or work some magic with REDengine JSON implementation
    return {data.c_str(), (uint32_t)data.size()};
}
