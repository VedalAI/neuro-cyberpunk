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

#include <algorithm>
#include <cmath>

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

namespace Impl
{
constexpr std::size_t MaxMappinQueryResults = 50;
constexpr std::uint32_t MaxQuestMappins = 20;
constexpr std::uint32_t MaxFastTravelMappins = 15;
constexpr std::uint32_t MaxFastTravelMappinsPerDistrict = 2;
constexpr std::uint32_t MaxServiceMappins = 10;

/**
 * \brief Broad usefulness bucket for a mappin returned by the waypoint query.
 *
 * The category is used only for ranking and per-category caps. It is not serialized into the response.
 */
enum class MappinUseCategory : uint8_t
{
    Tracked = 0,
    Quest,
    FastTravel,
    Service,
    Utility,
    Misc,
};

/**
 * \brief Internal waypoint candidate with ranking metadata.
 *
 * The serialized payload remains in m_data; the extra fields exist only while trimming query_waypoints results.
 */
struct ScoredMappinData
{
    NeuroMappinData m_data{};
    game::data::MappinVariant m_variant{};
    MappinUseCategory m_category{};
    float m_score{};
    std::size_t m_originalIndex{};
};

/**
 * \brief Assigns a mappin to a coarse usefulness category.
 *
 * \param aData Serializable mappin data already collected for the response.
 * \param aVariant Native mappin variant used to infer gameplay intent.
 * \return Category used by scoring and result caps.
 */
MappinUseCategory GetMappinUseCategory(const NeuroMappinData& aData, game::data::MappinVariant aVariant)
{
    if (aData.tracked)
    {
        return MappinUseCategory::Tracked;
    }

    switch (aVariant)
    {
    case game::data::MappinVariant::BountyHuntVariant:
    case game::data::MappinVariant::ClientInDistressVariant:
    case game::data::MappinVariant::ConvoyVariant:
    case game::data::MappinVariant::CourierVariant:
    case game::data::MappinVariant::DefaultQuestVariant:
    case game::data::MappinVariant::DynamicEventVariant:
    case game::data::MappinVariant::GangWatchVariant:
    case game::data::MappinVariant::HuntForPsychoVariant:
    case game::data::MappinVariant::MinorActivityVariant:
    case game::data::MappinVariant::QuestGiverVariant:
    case game::data::MappinVariant::RetrievingVariant:
    case game::data::MappinVariant::SOSsignalVariant:
    case game::data::MappinVariant::SabotageVariant:
    case game::data::MappinVariant::SmugglersDenVariant:
    case game::data::MappinVariant::ThieveryVariant:
    case game::data::MappinVariant::Zzz06_NCPDGigVariant:
    case game::data::MappinVariant::Zzz09_CourierSandboxActivityVariant:
    case game::data::MappinVariant::Zzz12_WorldEncounterVariant:
        return MappinUseCategory::Quest;
    case game::data::MappinVariant::FastTravelVariant:
    case game::data::MappinVariant::Zzz17_NCARTVariant:
        return MappinUseCategory::FastTravel;
    case game::data::MappinVariant::DropboxVariant:
    case game::data::MappinVariant::ServicePointBarVariant:
    case game::data::MappinVariant::ServicePointClothesVariant:
    case game::data::MappinVariant::ServicePointCyberwareVariant:
    case game::data::MappinVariant::ServicePointDropPointVariant:
    case game::data::MappinVariant::ServicePointFoodVariant:
    case game::data::MappinVariant::ServicePointGunsVariant:
    case game::data::MappinVariant::ServicePointJunkVariant:
    case game::data::MappinVariant::ServicePointMedsVariant:
    case game::data::MappinVariant::ServicePointMeleeTrainerVariant:
    case game::data::MappinVariant::ServicePointNetTrainerVariant:
    case game::data::MappinVariant::ServicePointProstituteVariant:
    case game::data::MappinVariant::ServicePointRipperdocVariant:
    case game::data::MappinVariant::ServicePointTechVariant:
    case game::data::MappinVariant::WanderingMerchantVariant:
    case game::data::MappinVariant::Zzz14_ServicePointBlackMarketVariant:
        return MappinUseCategory::Service;
    case game::data::MappinVariant::ApartmentVariant:
    case game::data::MappinVariant::HiddenStashVariant:
    case game::data::MappinVariant::Zzz05_ApartmentToPurchaseVariant:
    case game::data::MappinVariant::Zzz07_PlayerStashVariant:
    case game::data::MappinVariant::Zzz08_WardrobeVariant:
        return MappinUseCategory::Utility;
    default:
        return MappinUseCategory::Misc;
    }
}

/**
 * \brief Returns the base sort score for a usefulness category.
 *
 * Lower scores are better. Distance and data-quality penalties are applied separately.
 *
 * \param aCategory Category assigned by GetMappinUseCategory.
 * \return Base score for the category.
 */
float GetMappinCategoryScore(MappinUseCategory aCategory)
{
    switch (aCategory)
    {
    case MappinUseCategory::Tracked:
        return -10000.0f;
    case MappinUseCategory::Quest:
        return 0.0f;
    case MappinUseCategory::FastTravel:
        return 25.0f;
    case MappinUseCategory::Service:
        return 45.0f;
    case MappinUseCategory::Utility:
        return 35.0f;
    case MappinUseCategory::Misc:
    default:
        return 75.0f;
    }
}

/**
 * \brief Calculates the final ranking score for a waypoint candidate.
 *
 * The distance penalty is logarithmic so nearby destinations are preferred without letting raw distance dominate
 * tracked or quest-relevant waypoints.
 *
 * \param aData Serializable mappin data used by the response.
 * \param aCategory Usefulness category assigned to the mappin.
 * \return Final score where lower values are selected first.
 */
float ScoreMappinData(const NeuroMappinData& aData, MappinUseCategory aCategory)
{
    const auto normalizedDistance = std::isfinite(aData.distance) ? std::max(aData.distance, 0.0f) : 0.0f;
    const auto distancePenalty = std::log1p(normalizedDistance / 100.0f) * 10.0f;
    const auto qualityPenalty = aData.name.empty() ? 1000.0f : 0.0f;

    return GetMappinCategoryScore(aCategory) + distancePenalty + qualityPenalty;
}

/**
 * \brief Wraps serializable mappin data with category, score and stable ordering metadata.
 *
 * \param aData Serializable mappin data to score.
 * \param aVariant Native mappin variant used to infer category.
 * \param aOriginalIndex Insertion order used as a final stable tie-breaker.
 * \return Scored waypoint candidate.
 */
ScoredMappinData ScoreMappinData(NeuroMappinData&& aData, game::data::MappinVariant aVariant,
                                 std::size_t aOriginalIndex)
{
    const auto category = GetMappinUseCategory(aData, aVariant);
    const auto score = ScoreMappinData(aData, category);
    return {.m_data = std::move(aData),
            .m_variant = aVariant,
            .m_category = category,
            .m_score = score,
            .m_originalIndex = aOriginalIndex};
}

/**
 * \brief Selects the most useful waypoint candidates for query_waypoints.
 *
 * Candidates are sorted by score, then selected with light category caps so high-volume groups such as fast travel
 * and vendors do not crowd out quest or tracked destinations.
 *
 * \param aMappins Candidate list. The function sorts and moves from this vector.
 * \param aLimit Maximum number of entries to return.
 * \return Serializable waypoint list capped to aLimit.
 */
std::vector<NeuroMappinData> SelectUsefulMappins(std::vector<ScoredMappinData>& aMappins,
                                                 std::size_t aLimit = MaxMappinQueryResults)
{
    std::stable_sort(aMappins.begin(), aMappins.end(), [](const auto& aLhs, const auto& aRhs) {
        if (aLhs.m_score != aRhs.m_score)
        {
            return aLhs.m_score < aRhs.m_score;
        }

        if (aLhs.m_data.distance != aRhs.m_data.distance)
        {
            return aLhs.m_data.distance < aRhs.m_data.distance;
        }

        return aLhs.m_originalIndex < aRhs.m_originalIndex;
    });

    std::vector<NeuroMappinData> selected{};
    selected.reserve(std::min(aMappins.size(), aLimit));

    std::uint32_t questCount{};
    std::uint32_t fastTravelCount{};
    std::uint32_t serviceCount{};
    tsl::hopscotch_map<std::string, std::uint32_t> fastTravelDistrictCounts{};

    auto canSelect = [&](const ScoredMappinData& aMappin) {
        switch (aMappin.m_category)
        {
        case MappinUseCategory::Tracked:
            return true;
        case MappinUseCategory::Quest:
            return questCount < MaxQuestMappins;
        case MappinUseCategory::FastTravel:
        {
            if (fastTravelCount >= MaxFastTravelMappins)
            {
                return false;
            }

            const auto& district = aMappin.m_data.district;
            if (district.empty())
            {
                return true;
            }

            const auto existingDistrictCount = fastTravelDistrictCounts.find(district);
            return existingDistrictCount == fastTravelDistrictCounts.end() ||
                   existingDistrictCount->second < MaxFastTravelMappinsPerDistrict;
        }
        case MappinUseCategory::Service:
            return serviceCount < MaxServiceMappins;
        case MappinUseCategory::Utility:
        case MappinUseCategory::Misc:
        default:
            return true;
        }
    };

    auto markSelected = [&](const ScoredMappinData& aMappin) {
        switch (aMappin.m_category)
        {
        case MappinUseCategory::Quest:
            ++questCount;
            break;
        case MappinUseCategory::FastTravel:
            ++fastTravelCount;
            if (!aMappin.m_data.district.empty())
            {
                ++fastTravelDistrictCounts[aMappin.m_data.district];
            }
            break;
        case MappinUseCategory::Service:
            ++serviceCount;
            break;
        default:
            break;
        }
    };

    for (auto& mappin : aMappins)
    {
        if (selected.size() >= aLimit)
        {
            break;
        }

        if (!canSelect(mappin))
        {
            continue;
        }

        markSelected(mappin);
        selected.push_back(std::move(mappin.m_data));
    }

    return selected;
}
} // namespace Impl

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
    std::vector<Impl::ScoredMappinData> mappinCandidates{};

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
        const auto mappinVariant = shared::raw::Mappin::GetMappinVariant(mappin);

        mappinDataDto.tracked = shared::raw::Mappin::IsTrackedByPlayer::Ref(mappin);
        mappinDataDto.type = shared::util::EnumValueToString(MappinVariantEnum, (int64_t)mappinVariant).ToString();

        mappinCandidates.push_back(
            Impl::ScoreMappinData(std::move(mappinDataDto), mappinVariant, mappinCandidates.size()));
    }

    auto serializableData = Impl::SelectUsefulMappins(mappinCandidates);

    auto data = glz::write_json(serializableData).value_or("Failed to serialize waypoint data to JSON");
    // Note: maybe just return std::string? Or work some magic with REDengine JSON implementation
    return {data.c_str(), (uint32_t)data.size()};
}
