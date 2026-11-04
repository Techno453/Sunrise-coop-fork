#include <bit>

#include "../../content/packages/schema/generated/tag_classes_808000.h"
#include "auth_schema_catalog.h"
#include "map_generator_auth.h"
#include "sensor_auth_update.h"

namespace sunrise::middleware::bap::activity_message::sensor_auth_update {
namespace {

namespace generated = middleware::content::packages::schema::generated;

namespace bits = encoding::bits;

/** Slot types whose auth body this module fills. Every other block is seed-only. */
constexpr std::uint8_t kSlotTypeParticipation = 13;
constexpr std::uint8_t kSlotTypeLifetime = 17;
constexpr std::uint8_t kSlotTypeConfiguration = 8;
constexpr std::uint8_t kSlotTypePackage = 16;
constexpr std::uint8_t kSlotTypeQueues = 41;
constexpr std::uint8_t kSlotTypeSpawnKeys = 67;
/** Map generator, announced on every region. */
constexpr std::uint8_t kSlotTypeMapGenerator = 37;
/** Player monitor, which a published room object brings into the stream. */
constexpr std::uint8_t kSlotTypePlayerMonitor = 30;
/** Hard-wipe globals. Its body is what an encounter bubble's script objects come from. */
constexpr std::uint8_t kSlotTypeHardWipeGlobals = 35;
/** Activity timer, which ships beside the hard-wipe globals in the same group. */
constexpr std::uint8_t kSlotTypeActivityTimer = 18;

/** Body widths, each checked against the writer after the body is written. */
constexpr std::size_t kParticipationBits = 192;
constexpr std::size_t kParticipationRegionBits = 32;
/** One raw activity-flag id; the list's 6-bit count is part of the fixed width. */
constexpr std::size_t kActivityFlagBits = 32;
constexpr std::size_t kLifetimeBits = 520;
constexpr std::size_t kConfigurationBits = 35;
/**
 * Hud_sensor (type 8, `C80804f10`): a bool, two optional arrays and a u32. Native 103DE20 copies
 * the first array's named floats (`C80804f16`, an optional key and an optional raw float each) from
 * the first type-8 object into the general HUD.
 */
constexpr std::uint8_t kHudFloatCountWidth = 6;
constexpr std::uint8_t kHudFloatWidth = 32;
constexpr std::size_t kHudFloatBits = kPresenceWidth + kKeyWidth + kPresenceWidth + kHudFloatWidth;
constexpr std::uint8_t kHudSensorTailWidth = 32;
/**
 * The packaged PvP timer group (markups 80BC6FAE and 80BC6FB1) is visible only while the general
 * HUD holds this named float at one (13E0250); its name is unknown.
 */
constexpr std::uint32_t kMatchClockVisibleKey = 0xEF07C38BU;
constexpr float kMatchClockShown = 1.0F;
/**
 * The second array (`C80802db3`, descriptor 0x38ECC20) is the objective's zone list: a 5-bit
 * count, then `C80804f15` entries (descriptor 0x37A5C50) of an optional name hash and an optional
 * ClientRef (descriptor 0x3937D20). The Control record store 0xBEAF30 resolves each
 * entry's ClientRef to its zone; neither it nor 0xED4500 reads the hash, which keeps its
 * constructed no-name value.
 */
constexpr std::uint8_t kHudZoneCountWidth = 5;
constexpr std::size_t kHudZoneBits =
    kPresenceWidth + kKeyWidth + kPresenceWidth + auth_fields::kClientRefBits;
constexpr std::size_t kPackageBits = 7;
constexpr std::size_t kQueueBits = 12;
constexpr std::size_t kSpawnKeyBits = 32 * 32 + 1 + 32;
/** Optional participation revive delay, a 16-bit half float: 30 s under darkness, else 3 s. */
constexpr std::uint8_t kHalfWidth = 16;
constexpr std::uint32_t kDarknessReviveDelayHalf = 0x4F80U;
constexpr std::uint32_t kDefaultReviveDelayHalf = 0x4200U;
/** binary16 -1: native 12E9330 keeps the selected setting for a negative override. */
constexpr std::uint32_t kInheritTimerHalf = 0xBC00U;
/** Encoded participation control scalars after its five leading presence bits. */
constexpr std::uint8_t kParticipationControlWidth = 3;
constexpr std::uint8_t kParticipationStateWidth = 2;
constexpr std::uint32_t kParticipationControlValue = 1;
constexpr std::uint32_t kParticipationStateValue = 1;
/** Zero is the absent/default code in the participation's optional scalar/list fields. */
constexpr std::uint32_t kEmptyParticipationField = 0;
constexpr std::uint8_t kParticipationIndexWidth = 5;
constexpr std::uint8_t kParticipationListCountWidth = 6;
constexpr std::uint8_t kPlayerIdentityWidth = 64;
/** Lifetime's local setting list contains the objective HUD override only. */
constexpr std::uint32_t kLifetimeSettingCount = 1;
constexpr std::uint8_t kLifetimeSettingCountWidth = 6;
/** A packed region is bubble times eight plus the state ordinal. */
constexpr std::uint32_t kStatesPerBubble = 8;
/**
 * Lifetime field `.2`: whether the activity admits fireteam joins and invites. The client stores
 * the bit at roster object `+0x180+2`; a zero locks its fireteam session (closed flag 8), so a
 * member who returns to orbit is refused `not-joinable` on the way back in.
 */
constexpr std::uint32_t kActivityAllowsFireteamJoin = 1;
/**
 * Empty map-generator body.
 * Two 475-bit records, a u32, then fixed arrays of 32 and 64 u8. The fixed array lengths apply
 * even with both dynamic arrays empty; counting one byte each truncates the body by 752 bits.
 */
constexpr std::size_t kMapGeneratorBits = map_generator_auth::kBitCount;
/**
 * Player-monitor body: a slot reference then a biased i32.
 * Fixed width: no presence bit and no array, so there is exactly one legal length.
 */
constexpr std::size_t kPlayerMonitorBits = 32 + 7 + 16 + 32;
/** Clock window: a bool, five u64 and a raw u32, every field unbiased. */
constexpr std::size_t kClockWindowBits = 1 + 5 * 64 + 32;
/** Words in the clock window. */
constexpr std::size_t kClockWindowWords = 5;
/** Each clock window word is 64 bits; the rate is a raw 32-bit float. */
constexpr std::uint8_t kClockWordWidth = 64;
constexpr std::uint8_t kClockRateWidth = 32;
/** Hard-wipe globals body: two bools, two bias-1 selectors, then the clock window. */
constexpr std::size_t kHardWipeGlobalsBits = 1 + 1 + 2 + 2 + kClockWindowBits;
/** Activity-timer body: the clock window, a bool, then one biased signed word. */
constexpr std::size_t kActivityTimerBits = kClockWindowBits + 1 + 32;
/** Width of the hard-wipe selectors, each a signed byte biased by one. */
constexpr std::uint8_t kHardWipeSelectorWidth = 2;
/** Wire value for a zero selector. A wire zero decodes to -1, the none sentinel. */
constexpr std::uint32_t kHardWipeSelectorZero = 1;
/** Type 30's 7-bit field carries a bias of one, so this wire value decodes to zero. */
constexpr std::uint32_t kPlayerMonitorSelectorZero = 1;
/** Type 30's 16-bit field carries a bias of 0x8000, so this wire value decodes to zero. */
constexpr std::uint32_t kUnsignedShortZero = 0x8000;

/** @return Least legal wire width of one slot type, or zero when the census has no row. */
[[nodiscard]] constexpr std::size_t census_minimum_bits(std::uint8_t slotType) noexcept {
    for (const auth_schema_catalog::Type& type : auth_schema_catalog::kTypes) {
        if (type.slotType == slotType) {
            return type.minimumBits;
        }
    }
    return 0;
}

// A composed width that drifts from the census truncates the block and desynchronises the stream.
static_assert(kMapGeneratorBits == census_minimum_bits(kSlotTypeMapGenerator));
static_assert(kPlayerMonitorBits == census_minimum_bits(kSlotTypePlayerMonitor));
static_assert(kHardWipeGlobalsBits == census_minimum_bits(kSlotTypeHardWipeGlobals));
static_assert(kActivityTimerBits == census_minimum_bits(kSlotTypeActivityTimer));

/** Signed fields in these bodies carry a -2^31 bias, so this wire value stores zero. */
constexpr std::uint32_t kSignedZero = 0x80000000;
/** The same bias wraps at the top of the field, so this wire value stores -1. */
constexpr std::uint32_t kSignedMinusOne = 0x7FFFFFFF;
/** The region index rides the same bias, so its wire value is the bias plus the index. */
constexpr std::uint32_t kRegionBias = 0x80000000;
/** Message 52's team-state byte 1, where bit 1 is `awaiting_client_sync`. */
constexpr std::uint32_t kAwaitingClientSync = 2;
/**
 * Runtime-i32 setting the type-17 body overrides: FNV-1 of `pvp_objective_hud`, which no installed
 * mode row authors. 0xBEA020 selects the objective HUD from it; zero selects none.
 */
constexpr std::uint32_t kWaitingSwitchKey = 0xB3C1251B;
constexpr std::uint32_t kWaitingSwitchClass = generated::SI32Element::kClass;
/** Type 17 carries 3 spawn overrides. Wire zero stores index -1 and disables one. */
constexpr std::size_t kSpawnOverrideCount = 3;
constexpr std::uint8_t kSpawnOverrideIndexWidth = 10;
constexpr std::uint32_t kSpawnOverrideIndexBias = 1;
/** Type 67 maps the 32 spawn-key ordinals to themselves, matching its constructor. */
constexpr std::size_t kSpawnKeyCount = 32;
/** The participation record's activity-flag count is six bits wide. */
constexpr std::uint8_t kActivityFlagCountWidth = 6;
static_assert(kActivityFlagCapacity < (std::size_t{1} << kActivityFlagCountWidth));

/** Writes the participation record's activity-flag list: its count, then each raw id. */
[[nodiscard]] bool write_activity_flags(bits::Writer& writer, const Snapshot& snapshot) noexcept {
    const auto keys = std::span(snapshot.activityFlags.keys).first(snapshot.activityFlags.count);
    bool encoded = writer.write(snapshot.activityFlags.count, kActivityFlagCountWidth);
    for (const std::uint32_t key : keys) {
        encoded = encoded && writer.write(key, kActivityFlagBits);
    }
    return encoded;
}

/**
 * Writes the participation body, which binds the player and latches the region.
 * Every biased field must carry its bias; a zero-filled field decodes to the smallest signed value.
 * @param writer Body writer.
 * @param snapshot Message input.
 * @return True when the body fits.
 */
[[nodiscard]] bool write_participation(bits::Writer& writer,
                                       const Snapshot& snapshot,
                                       std::uint64_t playerKey) noexcept {
    std::int8_t partition = 0;
    for (const auto& seat : snapshot.participationSeats) {
        if (seat.playerKey == playerKey && seat.hasAssignedPartition) {
            partition = seat.assignedPartition;
        }
    }
    if (partition < kUnassignedPartition || partition > kLastPartition) {
        return false;
    }
    // A match player's faction is its team's (0xFDAF80 reads it at identity +9 to find the
    // player's scoreboard team); outside a match every player shares faction 0.
    const auto faction = snapshot.partitionFactions ? auth_fields::team_faction(partition) : 0;
    // An optional field's value follows its presence bit, so sending +0 shifts everything below.
    bool encoded = writer.write(snapshot.hasRegion ? 1U : 0U, kPresenceWidth);
    if (encoded && snapshot.hasRegion) {
        encoded = writer.write(kRegionBias + snapshot.region, kParticipationRegionBits);
    }
    // The participation record is this body's head, so struct +8 and +10 are record +8 and +10.
    // Record +8 is step 36 task 9's own term and +10 is the spawn gate's.
    // Record +56's assignment, 5 bits at bias 1, must equal the membership blob's A.P2 value.
    encoded =
        encoded && writer.write(0, kPresenceWidth) && writer.write(1, kPresenceWidth)
        && writer.write(1, kPresenceWidth) && writer.write(1, kPresenceWidth)
        && writer.write(0, kPresenceWidth)
        && writer.write(kParticipationControlValue, kParticipationControlWidth)
        && writer.write(kParticipationStateValue, kParticipationStateWidth)
        && writer.write(kEmptyParticipationField, kParticipationControlWidth)
        && writer.write(kEmptyParticipationField, auth_fields::kSigned32Width)
        && writer.write(static_cast<std::uint32_t>(partition + kPartitionBias), kPartitionWidth)
        && writer.write(0, kPresenceWidth)
        && writer.write(kEmptyParticipationField, kParticipationControlWidth)
        && writer.write(1, kPresenceWidth)
        && writer.write(playerKey != 0 ? playerKey : snapshot.playerKey, kPlayerIdentityWidth)
        && writer.write(kEmptyParticipationField, kParticipationIndexWidth)
        && writer.write(static_cast<std::uint32_t>(faction + auth_fields::kFactionBias),
                        auth_fields::kFactionWidth)
        && write_activity_flags(writer, snapshot)
        && writer.write(kEmptyParticipationField, kParticipationListCountWidth)
        // Byte 736 bypasses the respawn countdown. Byte 737 holds the spawn while the client
        // loads. The revive delay stays authored unless a darkness policy is set.
        && writer.write(1, kPresenceWidth)
        && writer.write(snapshot.awaitClientSync ? kAwaitingClientSync : 0U, 4);
    if (snapshot.inheritRespawnTimers) {
        encoded = encoded && writer.write(1, kPresenceWidth)
                  && writer.write(kInheritTimerHalf, kHalfWidth) && writer.write(1, kPresenceWidth)
                  && writer.write(kInheritTimerHalf, kHalfWidth);
    } else {
        encoded = encoded && writer.write(0, kPresenceWidth)
                  && writer.write(snapshot.hasDarknessPolicy ? 1U : 0U, kPresenceWidth)
                  && (!snapshot.hasDarknessPolicy
                      || writer.write(snapshot.darknessEnabled ? kDarknessReviveDelayHalf
                                                               : kDefaultReviveDelayHalf,
                                      kHalfWidth));
    }
    return encoded && writer.write(0, kPresenceWidth) && writer.write(0, kPresenceWidth)
           && writer.write(128, 8) && writer.write(kSignedZero, 32);
}

/**
 * Writes the lifetime body, which is the activity state the roster reports.
 * @param writer Body writer.
 * @param snapshot Message input.
 * @return True when the body fits.
 */
[[nodiscard]] bool write_lifetime(bits::Writer& writer, const Snapshot& snapshot) noexcept {
    // Field `.4` names an authored spawn entry. No host model owns one, so it carries the empty
    // name hash; zero is a hash that no row matches.
    bool encoded =
        writer.write(std::uint32_t{snapshot.lifetime} + kLifetimeBias, kLifetimeWidth)
        && writer.write(1, 3) && writer.write(kActivityAllowsFireteamJoin, kPresenceWidth)
        && writer.write(kSignedZero, 32)
        && writer.write(kEmptyNameHash, 32)
        // `.5` names the published region's bubble; a disabled darkness policy sends -1.
        && writer.write(snapshot.hasDarknessPolicy && !snapshot.darknessEnabled ? kSignedMinusOne
                        : snapshot.hasRegion ? kSignedZero + snapshot.region / kStatesPerBubble
                        : snapshot.hasDarknessPolicy ? kSignedMinusOne
                                                     : kSignedZero,
                        32)
        && writer.write(kLifetimeSettingCount, kLifetimeSettingCountWidth)
        && writer.write(kWaitingSwitchKey, 32) && writer.write(1, kPresenceWidth)
        && writer.write(kWaitingSwitchClass, 32)
        && writer.write(kSignedZero + snapshot.matchObjectiveHud, 32);
    encoded = encoded && writer.write(kSignedZero, 32);
    for (std::size_t index = 0; encoded && index < kSpawnOverrideCount; ++index) {
        const std::uint32_t slice =
            snapshot.hasSpawnOverride ? snapshot.spawnSliceSet + kSpawnOverrideIndexBias : 0U;
        const std::uint32_t hash =
            snapshot.hasSpawnOverride ? snapshot.spawnSetHash : kAbsentSpawnSetHash;
        encoded = writer.write(slice, kSpawnOverrideIndexWidth) && writer.write(hash, 32);
    }
    // Struct `+1256` is the out-of-bounds `activity_quarantine` selector. The reader arms the
    // quarantine at or below 0x3F unsigned, so minus one leaves it clear and teleports nobody.
    return encoded && writer.write(0, kPresenceWidth) && writer.write(0, 32)
           && writer.write(kSignedMinusOne, 32) && writer.write(0, 32)
           && writer.write(kSlotTypeBias, kSlotTypeWidth)
           && writer.write(kSlotIndexBias, kSlotIndexWidth) && writer.write(0, 32)
           && writer.write(0, 3);
}

/**
 * Writes the clock window shared by the hard-wipe globals and activity-timer bodies.
 * Every field is unbiased, so zeros store literal zeros and leave the timer stopped.
 * @param writer Body writer.
 * @return True when the record fits.
 */
[[nodiscard]] bool write_clock_window(bits::Writer& writer) noexcept {
    bool encoded = writer.write(0, kPresenceWidth);
    for (std::size_t word = 0; encoded && word < kClockWindowWords; ++word) {
        encoded = writer.write(0, 64);
    }
    return encoded && writer.write(0, 32);
}

/**
 * Writes the hard-wipe globals body.
 * Fixed width: no presence bit, no array and no variant field, so one legal length.
 * @param writer Body writer.
 * @return True when the body fits.
 */
[[nodiscard]] bool write_hard_wipe_globals(bits::Writer& writer) noexcept {
    // TODO: the client seeds the second selector to -1; confirm zero is the value to assert.
    return writer.write(0, kPresenceWidth) && writer.write(0, kPresenceWidth)
           && writer.write(kHardWipeSelectorZero, kHardWipeSelectorWidth)
           && writer.write(kHardWipeSelectorZero, kHardWipeSelectorWidth)
           && write_clock_window(writer);
}

/**
 * Writes the activity-timer body.
 * The trailing signed word carries the same `+2^31` bias as the other signed fields here.
 * @param writer Body writer.
 * @return True when the body fits.
 */
[[nodiscard]] bool write_activity_timer(bits::Writer& writer) noexcept {
    return write_clock_window(writer) && writer.write(0, kPresenceWidth)
           && writer.write(kSignedZero, 32);
}

[[nodiscard]] bool write_activity_timer(bits::Writer& writer, const Snapshot& snapshot) noexcept {
    // Running, then minimum, maximum, current and remaining at the epoch, the epoch and the rate;
    // the tail flag is clear and the tail value is a biased signed word.
    return writer.write(1, kPresenceWidth) && writer.write(0, kClockWordWidth)
           && writer.write(snapshot.activityTimerMaximum, kClockWordWidth)
           && writer.write(snapshot.activityTimerCurrentAtEpoch, kClockWordWidth)
           && writer.write(snapshot.activityTimerRemaining, kClockWordWidth)
           && writer.write(snapshot.activityTimerEpoch, kClockWordWidth)
           && writer.write(std::bit_cast<std::uint32_t>(snapshot.activityTimerRate),
                           kClockRateWidth)
           && writer.write(0, kPresenceWidth)
           && writer.write(kSignedZero
                               + static_cast<std::uint32_t>(snapshot.activityTimerTailValue),
                           auth_fields::kSigned32Width);
}

/**
 * Writes the spawn-key body, which maps the 32 ordinals to themselves.
 * @param writer Body writer.
 * @return True when the body fits.
 */
[[nodiscard]] bool write_spawn_keys(bits::Writer& writer) noexcept {
    bool encoded = true;
    for (std::size_t index = 0; encoded && index < kSpawnKeyCount; ++index) {
        encoded = writer.write(kSignedZero + index, 32);
    }
    return encoded && writer.write(0, kPresenceWidth) && writer.write(kSignedMinusOne, 32);
}

/** @return True when this type-8 slot is the player-key group's, which carries the match clock. */
[[nodiscard]] bool shows_match_clock(const Snapshot& snapshot, std::uint32_t key) noexcept {
    return snapshot.matchClockVisible && key == snapshot.roster.playerKeyGroup;
}

/** @return Zones this type-8 slot lists: the player-key group's lists the mode's, others none. */
[[nodiscard]] std::size_t hud_zone_count(const Snapshot& snapshot, std::uint32_t key) noexcept {
    return key == snapshot.roster.playerKeyGroup ? snapshot.hudZoneCount : 0U;
}

/**
 * Writes the hud_sensor body. The leading bool and the u32 keep their constructed values; a shown
 * clock adds its one named float to the first array, and the mode's zones fill the second.
 */
[[nodiscard]] bool
write_hud_sensor(bits::Writer& writer, const Snapshot& snapshot, std::uint32_t key) noexcept {
    const bool showsClock = shows_match_clock(snapshot, key);
    const unsigned floats = static_cast<unsigned>(showsClock);
    const std::size_t zones = hud_zone_count(snapshot, key);
    if (zones > snapshot.hudZones.size()) {
        return false;
    }
    bool encoded =
        writer.write(0, kPresenceWidth) && writer.write(floats != 0 ? 1U : 0U, kPresenceWidth);
    if (encoded && floats != 0) {
        encoded = writer.write(floats, kHudFloatCountWidth);
    }
    if (encoded && showsClock) {
        encoded = writer.write(1, kPresenceWidth) && writer.write(kMatchClockVisibleKey, kKeyWidth)
                  && writer.write(1, kPresenceWidth)
                  && writer.write(std::bit_cast<std::uint32_t>(kMatchClockShown), kHudFloatWidth);
    }
    encoded = encoded && writer.write(zones != 0 ? 1U : 0U, kPresenceWidth)
              && (zones == 0 || writer.write(zones, kHudZoneCountWidth));
    for (std::size_t i = 0; encoded && i < zones; ++i) {
        const HudZone& zone = snapshot.hudZones[i];
        encoded = writer.write(1, kPresenceWidth) && writer.write(kEmptyNameHash, kKeyWidth)
                  && writer.write(1, kPresenceWidth)
                  && auth_fields::write_client_ref(
                      writer, zone.group, match_object::kSlotType, zone.slot);
    }
    return encoded && writer.write(0, kHudSensorTailWidth);
}

/** @return The spawned match object this exact type-4 slot carries, or null. */
const match_object::Object* match_object_at(const Snapshot& snapshot,
                                            std::uint8_t slotType,
                                            std::uint32_t key,
                                            std::uint16_t slotIndex) noexcept {
    if (slotType != match_object::kSlotType
        || snapshot.matchObjectCount > snapshot.matchObjects.size()) {
        return nullptr;
    }
    for (std::size_t i = 0; i < snapshot.matchObjectCount; ++i) {
        const auto& object = snapshot.matchObjects[i];
        if (object.group == key && object.slot == slotIndex) {
            return &object;
        }
    }
    return nullptr;
}

} // namespace

/** Reports how many bits of auth body one slot carries. */
std::size_t auth_body_bits(const Snapshot& snapshot,
                           std::uint8_t slotType,
                           bool carriesPlayerKey,
                           std::uint32_t key,
                           std::uint16_t slotIndex) noexcept {
    if (slotType == kSlotTypeParticipation) {
        return carriesPlayerKey
                   ? kParticipationBits + (snapshot.hasRegion ? kParticipationRegionBits : 0)
                         + ((snapshot.hasDarknessPolicy || snapshot.inheritRespawnTimers)
                                ? kHalfWidth
                                : 0U)
                         + (snapshot.inheritRespawnTimers ? kHalfWidth : 0U)
                         + snapshot.activityFlags.count * kActivityFlagBits
                   : 0;
    }
    if (const auto* object = match_object_at(snapshot, slotType, key, slotIndex);
        object != nullptr) {
        return match_object::body_bits(*object);
    }
    for (const auto& command : snapshot.matchCinematics) {
        if (slotType == cinematic_record::kSlotType && command.generation != 0
            && command.group == key && command.slot == slotIndex) {
            return cinematic_record::kBodyBits;
        }
    }
    if (slotType == scoreboard_record::kSlotType && snapshot.hasScoreboard) {
        return scoreboard_record::record_body_bits(snapshot.scoreboard);
    }
    if (slotType == kSlotTypeLifetime) {
        return kLifetimeBits;
    }
    if (slotType == kSlotTypeConfiguration) {
        const std::size_t zones = hud_zone_count(snapshot, key);
        const unsigned floats = static_cast<unsigned>(shows_match_clock(snapshot, key));
        return kConfigurationBits
               + (floats != 0 ? kHudFloatCountWidth + floats * kHudFloatBits : 0U)
               + (zones != 0 ? kHudZoneCountWidth + zones * kHudZoneBits : 0U);
    }
    if (slotType == kSlotTypePackage) {
        return kPackageBits;
    }
    if (slotType == kSlotTypeQueues) {
        return kQueueBits;
    }
    if (slotType == kSlotTypeSpawnKeys) {
        return kSpawnKeyBits;
    }
    if (slotType == kSlotTypeHardWipeGlobals) {
        return kHardWipeGlobalsBits;
    }
    if (slotType == kSlotTypeActivityTimer) {
        return kActivityTimerBits;
    }
    if (slotType == kSlotTypeMapGenerator) {
        return kMapGeneratorBits;
    }
    if (slotType == kSlotTypePlayerMonitor) {
        return kPlayerMonitorBits;
    }
    return 0;
}

/** Writes one slot's auth body. */
bool write_auth_body(bits::Writer& writer,
                     const Snapshot& snapshot,
                     std::uint8_t slotType,
                     bool carriesPlayerKey,
                     std::uint64_t playerKey,
                     std::uint32_t key,
                     std::uint16_t slotIndex) noexcept {
    const std::size_t start = writer.bit_count();
    const std::size_t expected =
        auth_body_bits(snapshot, slotType, carriesPlayerKey, key, slotIndex);
    bool encoded = true;
    if (const auto* object = match_object_at(snapshot, slotType, key, slotIndex);
        object != nullptr) {
        return match_object::write_body(writer, *object) && writer.bit_count() == start + expected;
    }
    for (const auto& command : snapshot.matchCinematics) {
        if (slotType == cinematic_record::kSlotType && command.generation != 0
            && command.group == key && command.slot == slotIndex) {
            return cinematic_record::write_body(writer, command)
                   && writer.bit_count() == start + expected;
        }
    }
    if (slotType == kSlotTypeActivityTimer && snapshot.activityTimerActive) {
        return write_activity_timer(writer, snapshot) && writer.bit_count() == start + expected;
    }
    if (slotType == kSlotTypeParticipation && carriesPlayerKey) {
        encoded = write_participation(writer, snapshot, playerKey);
    } else if (slotType == scoreboard_record::kSlotType && snapshot.hasScoreboard) {
        encoded = scoreboard_record::write_record_body(writer, snapshot.scoreboard);
    } else if (slotType == kSlotTypeLifetime) {
        encoded = write_lifetime(writer, snapshot);
    } else if (slotType == kSlotTypeConfiguration) {
        encoded = write_hud_sensor(writer, snapshot, key);
    } else if (slotType == kSlotTypePackage) {
        // 7 absent top-level fields keep the package-owned configuration.
        encoded = pad_bits(writer, kPackageBits);
    } else if (slotType == kSlotTypeQueues) {
        encoded = writer.write(0, 7) && writer.write(0, 5);
    } else if (slotType == kSlotTypeSpawnKeys) {
        encoded = write_spawn_keys(writer);
    } else if (slotType == kSlotTypeHardWipeGlobals) {
        encoded = write_hard_wipe_globals(writer);
    } else if (slotType == kSlotTypeActivityTimer) {
        encoded = write_activity_timer(writer);
    } else if (slotType == kSlotTypePlayerMonitor) {
        // Three of the four fields carry a bias, so a neutral body writes each bias, not a zero.
        encoded = writer.write(0, 32) && writer.write(kPlayerMonitorSelectorZero, 7)
                  && writer.write(kUnsignedShortZero, 16) && writer.write(kSignedZero, 32);
    } else if (slotType == kSlotTypeMapGenerator) {
        encoded = map_generator_auth::write_body(writer, {});
    }
    return encoded && writer.bit_count() == start + expected;
}

} // namespace sunrise::middleware::bap::activity_message::sensor_auth_update
