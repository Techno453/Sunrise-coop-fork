#include "kill_incident_codec.h"

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>

#include "../../content/packages/schema/generated/tag_classes_80804c.h"
#include "../../content/packages/schema/generated/tag_classes_808064.h"
#include "../../content/packages/schema/generated/tag_classes_808087.h"
#include "../../content/packages/schema/generated/tag_classes_808088.h"
#include "../../content/packages/schema/generated/tag_classes_808095.h"
#include "../../content/packages/schema/generated/tag_classes_80809c.h"
#include "../../content/packages/schema/generated/tag_classes_80809f.h"
#include "../../encoding/bit_reader.h"
#include "../../encoding/bit_writer.h"
#include "auth_fields.h"
#include "incident.h"
#include "incident_player_roles.h"

namespace sunrise::middleware::bap::activity_message::incident {
namespace {
namespace targets = content::packages::tables::incident_targets;
namespace generated = content::packages::schema::generated;

/** Native object storage of one payload, which the largest schema must fit. */
constexpr std::size_t kMemoryBytes = 1024;
static_assert(kMemoryBytes >= generated::C80806447::kSize
              && kMemoryBytes >= generated::C80806442::kSize);
using Memory = std::array<std::byte, kMemoryBytes>;
using Reader = encoding::bits::Reader;
using Writer = encoding::bits::Writer;

struct Field {
    std::uint16_t offset;
    std::uint16_t opcode;
    std::uint8_t width;
    std::uint8_t storage;
    std::uint16_t child;
    std::uint32_t argument0;
    std::uint32_t argument1;
};
struct Schema {
    std::span<const Field> fields;
    std::uint16_t size;
    std::uint16_t count;
    std::uint16_t stride;
};
struct DefaultWord {
    std::uint16_t offset;
    std::uint64_t value;
};

#include "kill_incident_schema_data.inc"

/** Field opcodes of the native schema walker (4C3390): a kind, plus a presence-bit flag. */
constexpr std::uint16_t kOptionalFlag = 0x100;
constexpr std::uint16_t kKindMask = 0xFF;
/** A nested schema; argument0 one takes the element count from the field at argument1. */
constexpr unsigned kNestedKind = 1;
constexpr std::uint32_t kCountedByField = 1;
/** A nested schema without a count field repeats its declared count. */
constexpr std::int64_t kUncounted = -1;
/** Integers stored with a bias of argument0. */
constexpr unsigned kFirstBiasedKind = 3;
constexpr unsigned kLastBiasedKind = 10;
/** A float, quantized between argument0 and argument1 below 32 bits. */
constexpr unsigned kFloatKind = 11;
/** Three raw 32-bit coordinates. */
constexpr unsigned kVectorKind = 13;
/** An actor handle; F8E790 leaves it invalid and 0x9F9610 encodes kind 22 with no tag. */
constexpr unsigned kHandleKind = 25;
/** A participant key (C80809acf -> C80809f7b): eight bytes, not a native integer. */
constexpr unsigned kKeyKind = 35;
/** Deepest nesting of the installed schemas, with margin. */
constexpr unsigned kMaximumDepth = 12;
constexpr std::size_t kVectorComponents = 3;
constexpr std::uint8_t kWordWidth = 32;
constexpr std::size_t kWordBytes = 4;
constexpr std::size_t kKeyBytes = 8;
constexpr std::size_t kLargestStorage = 8;
constexpr std::uint8_t kPresenceWidth = 1;
constexpr std::uint8_t kByteWidth = 8;
constexpr std::uint64_t kByteMask = 0xFFU;
constexpr std::uint8_t kHandleKindWidth = 6;
constexpr std::uint64_t kAbsentHandleKind = 22;
constexpr std::uint8_t kHandleTagWidth = 1;
/** Trailing padding completes the last payload byte. */
constexpr std::size_t kMaximumPaddingBits = 7;

// Tag layout: the common incident base `C80809512` is one unnamed gap, so its offsets are native's.
/** Common incident base: sequence, then a source kind and the source identity. */
constexpr std::size_t kSequenceOffset = 0;
constexpr std::size_t kSourceKindOffset = 8;
constexpr std::size_t kSourcePlayerOffset = 0x10;
constexpr std::size_t kBaseBytes = generated::C80809512::kSize;
/** Source kind one names a player source. */
constexpr std::uint64_t kPlayerSource = 1;
/** Damage participants one kill carries. */
constexpr std::size_t kParticipantCapacity = 9;
/** Actors one kill carries: the killer, then the victim. */
constexpr std::size_t kActorCount = 2;

#pragma pack(push, 1)
/** An actor's three classification bytes; the middle one is its class. */
struct ActorClassification final {
    std::uint8_t leading;
    std::uint8_t actorClass;
    std::uint8_t trailing;
};
/** Canonical actor `C80808801`: its named words are the scalar and the word before its key. */
struct CanonicalActor final {
    std::uint64_t handle;
    ActorClassification classification;
    std::byte unused0B;
    std::uint32_t scalar;
    std::uint64_t preIdentity;
    std::uint64_t key;
    std::uint8_t faction;
    std::array<std::byte, 7> unused21;
    std::uint64_t postFaction;
    std::array<std::byte, 2> descriptor;
    std::array<std::byte, 6> unused32;
    std::uint64_t tailWord;
    std::uint32_t tailScalar;
    std::array<std::byte, 4> unused44;
};
/** Compact actor `C8080644d`: the same fields in its own order. */
struct CompactActor final {
    ActorClassification classification;
    std::byte unused03;
    std::uint32_t scalar;
    std::array<std::byte, 2> descriptor;
    std::uint8_t faction;
    std::byte unused0B;
    std::uint64_t preIdentity;
    std::uint64_t identity;
    std::array<std::byte, 4> unused1C;
    std::uint64_t postFaction;
    std::uint64_t tailWord;
    std::uint32_t tailScalar;
    std::array<std::byte, 4> unused34;
};
/** The fixed trailer both kill forms carry, after the base or after the participants. */
struct Trailer final {
    std::array<std::byte, 2> descriptor;
    std::array<std::byte, 6> unused02;
    std::array<std::byte, 16> identity;
    std::array<std::byte, 2> secondDescriptor;
    std::array<std::byte, 2> unused1A;
    std::array<std::byte, 12> scalars;
    std::uint64_t word;
};
/** Canonical kill `C8080645a`: two actors, the damage participants, the trailer, the reason. */
struct CanonicalKill final {
    std::array<std::byte, kBaseBytes> base;
    std::array<CanonicalActor, kActorCount> actors;
    std::uint32_t participantCount;
    std::array<std::byte, 4> unusedAC;
    std::array<std::array<std::byte, generated::C808087eb::kSize>, kParticipantCapacity>
        participants;
    Trailer trailer;
    std::array<std::byte, 0x78> unused2D8;
    std::uint64_t shiftedWord;
    std::uint32_t shiftedScalar;
    std::uint8_t killReason;
    std::array<std::byte, 3> unused35D;
};
/** Compact kill `C80806447`: the trailer first, then two actors and the participants. */
struct CompactKill final {
    std::array<std::byte, kBaseBytes> base;
    Trailer trailer;
    std::array<std::byte, 0x18> unused48;
    std::array<CompactActor, kActorCount> actors;
    std::uint32_t participantCount;
    std::array<std::byte, 4> unusedD4;
    std::array<std::array<std::byte, generated::C8080644c::kSize>, kParticipantCapacity>
        participants;
    std::uint32_t shiftedScalar;
    std::array<std::byte, 0xC0> unused31C;
    std::uint64_t shiftedWord;
    std::array<std::byte, 12> unused3E4;
};
/** The fields of cinematic report `C808087bf` in `gap4C`, after its named group word. */
struct CinematicTail final {
    std::uint8_t type;
    std::byte unused1;
    std::uint16_t slot;
    std::uint64_t clock;
    std::uint32_t duration;
    std::array<std::byte, 4> unused10;
};
/** Capture report `C80806470` up to its named zone word. */
struct CaptureReport final {
    std::array<std::byte, kBaseBytes> base;
    std::uint8_t losingFaction;
    std::uint8_t capturingFaction;
    std::array<std::byte, 6> unused1A;
    std::array<std::byte, generated::C808087bb::kSize> participants;
};
#pragma pack(pop)
static_assert(sizeof(CanonicalActor) == generated::C80808801::kSize);
static_assert(offsetof(CanonicalActor, scalar) == offsetof(generated::C80808801, unkC));
static_assert(offsetof(CanonicalActor, preIdentity) == offsetof(generated::C80808801, unk10));
static_assert(sizeof(CompactActor) == generated::C8080644d::kSize);
static_assert(offsetof(CompactActor, scalar) == offsetof(generated::C8080644d, unk4));
static_assert(offsetof(CompactActor, preIdentity) == offsetof(generated::C8080644d, unkC));
static_assert(sizeof(CanonicalKill) == generated::C8080645a::kSize);
static_assert(offsetof(CanonicalKill, actors) + offsetof(CanonicalActor, scalar)
              == offsetof(generated::C8080645a, unk24));
static_assert(offsetof(CanonicalKill, actors) + sizeof(CanonicalActor)
                  + offsetof(CanonicalActor, scalar)
              == offsetof(generated::C8080645a, unk6c));
static_assert(sizeof(CompactKill) == generated::C80806447::kSize);
static_assert(offsetof(CompactKill, actors) + offsetof(CompactActor, scalar)
              == offsetof(generated::C80806447, unk64));
static_assert(offsetof(CompactKill, actors) + sizeof(CompactActor) + offsetof(CompactActor, scalar)
              == offsetof(generated::C80806447, unk9c));
static_assert(sizeof(CinematicTail) == sizeof(generated::C808087bf::gap4C));
static_assert(sizeof(CaptureReport) == offsetof(generated::C80806470, unk328));

/** Canonical kill: the two actors, the damage participants, then the kill reason. */
constexpr std::size_t kCanonicalActorStride = sizeof(CanonicalActor);
constexpr std::size_t kActorHandleOffset = offsetof(CanonicalActor, handle);
constexpr std::size_t kActorClassOffset =
    offsetof(CanonicalActor, classification) + offsetof(ActorClassification, actorClass);
constexpr std::size_t kActorKeyOffset = offsetof(CanonicalActor, key);
constexpr std::size_t kActorFactionOffset = offsetof(CanonicalActor, faction);
constexpr std::size_t kKillingActor = offsetof(CanonicalKill, actors);
constexpr std::size_t kDeadActor = kKillingActor + kCanonicalActorStride;
constexpr std::size_t kCanonicalParticipantCount = offsetof(CanonicalKill, participantCount);
constexpr std::size_t kCanonicalParticipants = offsetof(CanonicalKill, participants);
constexpr std::size_t kCanonicalParticipantStride = generated::C808087eb::kSize;
constexpr std::size_t kKillReasonOffset = offsetof(CanonicalKill, killReason);
// Tag layout: the canonical participant `C808087eb` is one unnamed gap.
/** Participant row: its kind byte, then its key at +8. Kinds 0 and 1 bind the player roles. */
constexpr std::size_t kParticipantKeyOffset = 8;
constexpr std::uint64_t kFirstNonPlayerParticipant = 2;

/** One field copy from a compact report to the canonical one, relative to its block. */
struct FieldCopy final {
    std::uint16_t from;
    std::uint16_t to;
    std::uint8_t bytes;
};
/** Compact kill: two actors, then the participants. */
constexpr std::size_t kCompactActors = offsetof(CompactKill, actors);
constexpr std::size_t kCompactActorStride = sizeof(CompactActor);
constexpr std::size_t kCompactParticipantCount = offsetof(CompactKill, participantCount);
constexpr std::size_t kCompactParticipants = offsetof(CompactKill, participants);
constexpr std::size_t kCompactParticipantStride = generated::C8080644c::kSize;
/** `C80806442` inserts role 3's two named words there, which moves what follows. */
constexpr std::size_t kCompact3Shift =
    sizeof(generated::C80806442::unkD0) + sizeof(generated::C80806442::unkD4);
static_assert(offsetof(generated::C80806442, unkD0) == kCompactParticipantCount);
/** Common base: the sequence word is unchanged. */
constexpr FieldCopy kSequenceCopy{kSequenceOffset, kSequenceOffset, sizeof(std::uint32_t)};
/** Source kind and identity retain their native two-word layout. */
constexpr FieldCopy kSourceCopy{kSourceKindOffset, kSourceKindOffset, 2 * sizeof(std::uint64_t)};
/** Actor classification bytes move past the canonical handle field. */
constexpr FieldCopy kActorClassificationCopy{offsetof(CompactActor, classification),
                                             offsetof(CanonicalActor, classification),
                                             sizeof(ActorClassification)};
constexpr FieldCopy kActorScalarCopy{
    offsetof(CompactActor, scalar), offsetof(CanonicalActor, scalar), sizeof(std::uint32_t)};
constexpr FieldCopy kActorDescriptorCopy{offsetof(CompactActor, descriptor),
                                         offsetof(CanonicalActor, descriptor),
                                         sizeof(CanonicalActor::descriptor)};
constexpr FieldCopy kActorFactionCopy{
    offsetof(CompactActor, faction), offsetof(CanonicalActor, faction), sizeof(std::uint8_t)};
constexpr FieldCopy kActorPreIdentityCopy{offsetof(CompactActor, preIdentity),
                                          offsetof(CanonicalActor, preIdentity),
                                          sizeof(std::uint64_t)};
constexpr FieldCopy kActorIdentityCopy{
    offsetof(CompactActor, identity), offsetof(CanonicalActor, key), sizeof(std::uint64_t)};
constexpr FieldCopy kActorPostFactionCopy{offsetof(CompactActor, postFaction),
                                          offsetof(CanonicalActor, postFaction),
                                          sizeof(std::uint64_t)};
constexpr FieldCopy kActorTailWordCopy{
    offsetof(CompactActor, tailWord), offsetof(CanonicalActor, tailWord), sizeof(std::uint64_t)};
constexpr FieldCopy kActorTailScalarCopy{offsetof(CompactActor, tailScalar),
                                         offsetof(CanonicalActor, tailScalar),
                                         sizeof(std::uint32_t)};
// Tag layout: the compact participant `C8080644c` is one unnamed gap.
/** Damage-participant kind and its six-word payload keep their offsets. */
constexpr FieldCopy kParticipantKindCopy{0, 0, 1};
constexpr FieldCopy kParticipantPayloadCopy{8, 8, 48};
/** Compact participant's trailing flag joins the canonical kind at +1. */
constexpr FieldCopy kParticipantFlagCopy{0x38, 1, 1};
/** The fixed trailer moves whole, field by field, from after the base to after the participants. */
constexpr std::size_t kCompactTrailer = offsetof(CompactKill, trailer);
constexpr std::size_t kCanonicalTrailer = offsetof(CanonicalKill, trailer);
constexpr FieldCopy kTrailerDescriptorCopy{kCompactTrailer + offsetof(Trailer, descriptor),
                                           kCanonicalTrailer + offsetof(Trailer, descriptor),
                                           sizeof(Trailer::descriptor)};
constexpr FieldCopy kTrailerIdentityCopy{kCompactTrailer + offsetof(Trailer, identity),
                                         kCanonicalTrailer + offsetof(Trailer, identity),
                                         sizeof(Trailer::identity)};
constexpr FieldCopy kTrailerSecondDescriptorCopy{
    kCompactTrailer + offsetof(Trailer, secondDescriptor),
    kCanonicalTrailer + offsetof(Trailer, secondDescriptor),
    sizeof(Trailer::secondDescriptor)};
constexpr FieldCopy kTrailerScalarsCopy{kCompactTrailer + offsetof(Trailer, scalars),
                                        kCanonicalTrailer + offsetof(Trailer, scalars),
                                        sizeof(Trailer::scalars)};
constexpr FieldCopy kTrailerWordCopy{kCompactTrailer + offsetof(Trailer, word),
                                     kCanonicalTrailer + offsetof(Trailer, word),
                                     sizeof(std::uint64_t)};
/** These compact offsets move by kCompact3Shift when role 3 is present. */
constexpr FieldCopy kShiftedScalarCopy{offsetof(CompactKill, shiftedScalar),
                                       offsetof(CanonicalKill, shiftedScalar),
                                       sizeof(std::uint32_t)};
constexpr FieldCopy kShiftedWordCopy{offsetof(CompactKill, shiftedWord),
                                     offsetof(CanonicalKill, shiftedWord),
                                     sizeof(std::uint64_t)};
constexpr std::array kBaseCopies{kSequenceCopy, kSourceCopy};
constexpr std::array kActorCopies{kActorClassificationCopy,
                                  kActorScalarCopy,
                                  kActorDescriptorCopy,
                                  kActorFactionCopy,
                                  kActorPreIdentityCopy,
                                  kActorIdentityCopy,
                                  kActorPostFactionCopy,
                                  kActorTailWordCopy,
                                  kActorTailScalarCopy};
constexpr std::array kParticipantCopies{
    kParticipantKindCopy, kParticipantPayloadCopy, kParticipantFlagCopy};
constexpr std::array kTrailerCopies{kTrailerDescriptorCopy,
                                    kTrailerIdentityCopy,
                                    kTrailerSecondDescriptorCopy,
                                    kTrailerScalarsCopy,
                                    kTrailerWordCopy};
constexpr std::array kShiftedTrailerCopies{kShiftedScalarCopy, kShiftedWordCopy};

// Tag layout: the score notice `C80806436` is one unnamed gap.
/** Score notice: the player's key at +0x18 and the amount at +0x78. */
constexpr std::size_t kNoticePlayerOffset = 0x18;
constexpr std::size_t kNoticeAmountOffset = 0x78;

/** Cinematic report `C808087bf`: group, type, slot, component clock and clip duration. */
constexpr std::size_t kCinematicGroupOffset = offsetof(generated::C808087bf, unk48);
constexpr std::size_t kCinematicTail = offsetof(generated::C808087bf, gap4C);
constexpr std::size_t kCinematicTypeOffset = kCinematicTail + offsetof(CinematicTail, type);
constexpr std::size_t kCinematicSlotOffset = kCinematicTail + offsetof(CinematicTail, slot);
constexpr std::size_t kCinematicClockOffset = kCinematicTail + offsetof(CinematicTail, clock);
constexpr std::size_t kCinematicDurationOffset = kCinematicTail + offsetof(CinematicTail, duration);

// Tag layout: the player events `C808087a5` (type 3) and `C8080880c` (type 20) are one unnamed
// gap each.
/** Player events: one `player` role at +0x18. */
constexpr std::size_t kEventPlayerOffset = 0x18;
// Tag layout: the spawn `C80806437` (type 13) is one unnamed gap.
/** Spawn: `dead_player` at +0x20 and `resurrecting_player` at +0x50. */
constexpr std::size_t kSpawnDeadOffset = 0x20;
constexpr std::size_t kSpawnResurrectingOffset = 0x50;

/** Roles each payload interface binds, in its binding order. */
constexpr std::uint8_t kPlayerEventRoleCount = 1;
constexpr std::uint8_t kSpawnRoleCount = 2;
constexpr std::uint8_t kKillRoleCount = 4;

std::uint64_t load(const Memory& memory, std::size_t offset, std::size_t width) noexcept {
    std::uint64_t value = 0;
    for (std::size_t i = 0; i < width; ++i) {
        value |= static_cast<std::uint64_t>(std::to_integer<std::uint8_t>(memory[offset + i]))
                 << (i * kByteWidth);
    }
    return value;
}
/**
 * Reads one native faction byte as the team it names: a team faction folds to its native
 * partition (`auth_fields::faction_team`), so the match compares it with assigned partitions.
 */
std::int8_t load_team(const Memory& memory, std::size_t offset) noexcept {
    return auth_fields::faction_team(static_cast<std::int8_t>(load(memory, offset, 1)));
}
void store(Memory& memory, std::size_t offset, std::size_t width, std::uint64_t value) noexcept {
    for (std::size_t i = 0; i < width; ++i) {
        memory[offset + i] = static_cast<std::byte>((value >> (i * kByteWidth)) & kByteMask);
    }
}
void initialize(Memory& memory, std::span<const DefaultWord> defaults) noexcept {
    memory = {};
    for (const auto& word : defaults) {
        store(memory, word.offset, sizeof(word.value), word.value);
    }
}

/** @return The scalar SOID of a participant key, which travels as eight wire-order bytes. */
std::uint64_t participant_key(const Memory& memory, std::size_t offset) noexcept {
    std::uint64_t value{};
    for (std::size_t i = 0; i < kKeyBytes; ++i) {
        value = (value << kByteWidth) | load(memory, offset + i, 1);
    }
    return value;
}

/** The native 11-bit float reader (0x351340 -> 0x365E80) includes exact endpoints. */
std::uint32_t expand_float(std::uint64_t value, const Field& field) noexcept {
    const float low = std::bit_cast<float>(field.argument0);
    const float high = std::bit_cast<float>(field.argument1);
    const std::uint32_t levels = std::uint32_t{1} << field.width;
    // Code 0 reads as low and the top code as high; the codes between split the range into
    // levels - 2 equal steps, each read at its midpoint.
    float result = low;
    if (value == levels - 1U) {
        result = high;
    } else if (value != 0) {
        const float step = (high - low) / static_cast<float>(levels - 2U);
        const float whole = static_cast<float>(value - 1U) * step;
        const float half = step * 0.5F + low;
        result = whole + half;
    }
    return std::bit_cast<std::uint32_t>(result);
}

/**
 * @return The count word a counted nested schema names, read unsigned: a negative native count
 * lands past every schema's capacity and is rejected, never taken as the declared count.
 */
std::int64_t counted(const Memory& memory, std::size_t offset) noexcept {
    return static_cast<std::int64_t>(load(memory, offset, kWordBytes));
}

bool read_schema(Reader& reader,
                 Memory& memory,
                 std::size_t base,
                 std::size_t index,
                 std::int64_t requested = kUncounted,
                 unsigned depth = 0,
                 bool requireFullMask = false) noexcept {
    if (index >= std::size(kSchemas) || depth > kMaximumDepth) {
        return false;
    }
    const Schema& schema = kSchemas[index];
    if (base + schema.size > memory.size()) {
        return false;
    }
    const std::size_t count = schema.count == 0         ? 1U
                              : requested == kUncounted ? schema.count
                                                        : static_cast<std::size_t>(requested);
    if ((schema.count == 0 && requested != kUncounted)
        || (schema.count != 0 && count > schema.count)) {
        return false;
    }
    for (std::size_t element = 0; element < count; ++element) {
        const std::size_t elementBase = base + element * schema.stride;
        for (const Field& field : schema.fields) {
            std::uint64_t value = 0;
            if ((field.opcode & kOptionalFlag) != 0) {
                if (!reader.read(kPresenceWidth, value)) {
                    return false;
                }
                if (value == 0) {
                    if (requireFullMask) {
                        return false;
                    }
                    continue;
                }
            }
            const unsigned kind = field.opcode & kKindMask;
            const std::size_t offset = elementBase + field.offset;
            if (kind == kNestedKind) {
                const auto repeated = field.argument0 == kCountedByField
                                          ? counted(memory, elementBase + field.argument1)
                                          : kUncounted;
                if (!read_schema(reader,
                                 memory,
                                 offset,
                                 field.child,
                                 repeated,
                                 depth + 1,
                                 requireFullMask)) {
                    return false;
                }
            } else if (kind == kVectorKind) {
                for (std::size_t coordinate = 0; coordinate < kVectorComponents; ++coordinate) {
                    if (!reader.read(kWordWidth, value)) {
                        return false;
                    }
                    store(memory, offset + coordinate * kWordBytes, kWordBytes, value);
                }
            } else if (kind == kKeyKind) {
                for (std::size_t byte = 0; byte < kKeyBytes; ++byte) {
                    if (!reader.read(kByteWidth, value)) {
                        return false;
                    }
                    store(memory, offset + byte, 1, value);
                }
            } else if (kind == kHandleKind) {
                if (!reader.read(kHandleKindWidth, value) || value != kAbsentHandleKind
                    || !reader.read(kHandleTagWidth, value) || value != 0) {
                    return false;
                }
                store(memory, offset, sizeof(std::uint64_t), UINT64_MAX);
            } else {
                if (field.storage == 0 || field.storage > kLargestStorage
                    || !reader.read(field.width, value)) {
                    return false;
                }
                if (kind >= kFirstBiasedKind && kind <= kLastBiasedKind) {
                    value -= field.argument0;
                }
                if (kind == kFloatKind && field.width != kWordWidth) {
                    value = expand_float(value, field);
                }
                store(memory, offset, field.storage, value);
            }
        }
    }
    return true;
}

bool padding_is_zero(Reader& reader) noexcept {
    if (reader.remaining_bits() > kMaximumPaddingBits) {
        return false;
    }
    std::uint64_t padding = 0;
    return reader.read(static_cast<std::uint8_t>(reader.remaining_bits()), padding) && padding == 0;
}

bool write_schema(Writer& writer,
                  const Memory& memory,
                  std::size_t base,
                  std::size_t index,
                  std::int64_t requested = kUncounted,
                  unsigned depth = 0) noexcept {
    if (index >= std::size(kSchemas) || depth > kMaximumDepth) {
        return false;
    }
    const Schema& schema = kSchemas[index];
    if (base + schema.size > memory.size()) {
        return false;
    }
    const std::size_t count = schema.count == 0         ? 1U
                              : requested == kUncounted ? schema.count
                                                        : static_cast<std::size_t>(requested);
    if (schema.count != 0 && count > schema.count) {
        return false;
    }
    for (std::size_t element = 0; element < count; ++element) {
        const std::size_t elementBase = base + element * schema.stride;
        for (const Field& field : schema.fields) {
            if ((field.opcode & kOptionalFlag) != 0 && !writer.write(1, kPresenceWidth)) {
                return false;
            }
            const unsigned kind = field.opcode & kKindMask;
            const std::size_t offset = elementBase + field.offset;
            if (kind == kNestedKind) {
                const auto repeated = field.argument0 == kCountedByField
                                          ? counted(memory, elementBase + field.argument1)
                                          : kUncounted;
                if (!write_schema(writer, memory, offset, field.child, repeated, depth + 1)) {
                    return false;
                }
            } else if (kind == kVectorKind) {
                for (std::size_t coordinate = 0; coordinate < kVectorComponents; ++coordinate) {
                    if (!writer.write(load(memory, offset + coordinate * kWordBytes, kWordBytes),
                                      kWordWidth)) {
                        return false;
                    }
                }
            } else if (kind == kKeyKind) {
                for (std::size_t byte = 0; byte < kKeyBytes; ++byte) {
                    if (!writer.write(load(memory, offset + byte, 1), kByteWidth)) {
                        return false;
                    }
                }
            } else if (kind == kHandleKind) {
                if (load(memory, offset, sizeof(std::uint64_t)) != UINT64_MAX
                    || !writer.write(kAbsentHandleKind, kHandleKindWidth)
                    || !writer.write(0, kHandleTagWidth)) {
                    return false;
                }
            } else {
                if (field.storage == 0 || field.storage > kLargestStorage
                    || (kind == kFloatKind && field.width != kWordWidth)) {
                    return false;
                }
                std::uint64_t value = load(memory, offset, field.storage);
                if (kind >= kFirstBiasedKind && kind <= kLastBiasedKind) {
                    value += field.argument0;
                }
                if (!writer.write(value, field.width)) {
                    return false;
                }
            }
        }
    }
    return true;
}

/** Port of F8E3A0/F8E480, A63E80 and F901B0/F902F0 for the serialized fields. */
bool expand(const Memory& input, KillSourceRole role, Memory& full) noexcept {
    initialize(full, kDefaults8080645A);
    const auto copy = [&](std::size_t from, std::size_t to, const FieldCopy& field) {
        std::copy_n(input.data() + from + field.from, field.bytes, full.data() + to + field.to);
    };
    for (const auto& field : kBaseCopies) {
        copy(0, 0, field);
    }
    for (std::size_t i = 0; i < kActorCount; ++i) {
        for (const auto& field : kActorCopies) {
            copy(kCompactActors + i * kCompactActorStride,
                 kKillingActor + i * kCanonicalActorStride,
                 field);
        }
    }
    const std::size_t shift = role == KillSourceRole::compact3 ? kCompact3Shift : 0U;
    const auto count = load(input, kCompactParticipantCount + shift, kWordBytes);
    if (count > kParticipantCapacity) {
        return false;
    }
    store(full, kCanonicalParticipantCount, kWordBytes, count);
    for (std::size_t i = 0; i < count; ++i) {
        for (const auto& field : kParticipantCopies) {
            copy(kCompactParticipants + shift + i * kCompactParticipantStride,
                 kCanonicalParticipants + i * kCanonicalParticipantStride,
                 field);
        }
    }
    for (const auto& field : kTrailerCopies) {
        copy(0, 0, field);
    }
    for (const auto& field : kShiftedTrailerCopies) {
        copy(shift, 0, field);
    }
    // Native expanded bitsets at 2D8/300/328 are not serialized by C8080645a.
    return true;
}
} // namespace

bool encode_score_notice(const targets::Catalog& catalog,
                         std::uint16_t target,
                         std::uint32_t sequence,
                         std::uint64_t player,
                         std::int32_t amount,
                         std::span<std::byte> output,
                         std::size_t& written) noexcept {
    written = 0;
    if (catalog.targetCount > catalog.targets.size() || target >= catalog.targetCount
        || catalog.targets[target].type != targets::kAmountType || sequence == 0 || player == 0
        || player == UINT64_MAX) {
        return false;
    }
    Memory payload{};
    initialize(payload, kDefaults80806436);
    store(payload, kSequenceOffset, kWordBytes, sequence);
    // F97530 copies a participant KEY, whose C80809f7b storage is eight wire-order bytes, the
    // representation of the native killer role; the scalar source identity at +0x10 differs.
    for (std::size_t i = 0; i < kKeyBytes; ++i) {
        store(payload, kNoticePlayerOffset + i, 1, player >> ((kKeyBytes - 1 - i) * kByteWidth));
    }
    store(payload, kNoticeAmountOffset, kWordBytes, static_cast<std::uint32_t>(amount));
    std::array<std::byte, kPayloadMaximum> body{};
    Writer bodyWriter(body);
    std::size_t bodyBytes{};
    if (!write_schema(bodyWriter, payload, 0, kSchema80806436) || !bodyWriter.finish(bodyBytes)) {
        return false;
    }
    std::array<std::byte, kMaximumBodyBytes> staged{};
    Writer writer(staged);
    if (!writer.write(target, kTargetWidth) || !writer.write(0, kExtraCountWidth)
        || !writer.write(0, kSelectorPresenceWidth) || !writer.write(0, kOptionalPresenceWidth)
        || !writer.write(bodyBytes, kPayloadLengthWidth)) {
        return false;
    }
    for (std::size_t i = 0; i < bodyBytes; ++i) {
        if (!writer.write(std::to_integer<std::uint8_t>(body[i]), kByteWidth)) {
            return false;
        }
    }
    std::size_t size{};
    if (!writer.finish(size) || size > output.size()) {
        return false;
    }
    std::copy_n(staged.begin(), size, output.begin());
    written = size;
    return true;
}

bool transcode_kill_payload(std::span<const std::byte> input,
                            KillSourceRole role,
                            std::span<std::byte> output,
                            std::size_t& written,
                            std::size_t& meaningfulBits) noexcept {
    written = 0;
    meaningfulBits = 0;
    if (input.empty() || input.size() > kPayloadMaximum || output.empty()
        || (role != KillSourceRole::compact2 && role != KillSourceRole::compact3)) {
        return false;
    }
    Memory compact{};
    initialize(compact,
               role == KillSourceRole::compact2 ? std::span<const DefaultWord>(kDefaults80806447)
                                                : std::span<const DefaultWord>(kDefaults80806442));
    Reader reader(input);
    // Native incident sender 0x4D2940 calls 0x4C3390 with mode 1 and a full field mask.
    // Sparse reads can make a role-2 precision kill also look like role 3; that second
    // interpretation is not an encoding the sender can produce.
    if (!read_schema(reader,
                     compact,
                     0,
                     role == KillSourceRole::compact2 ? kSchema80806447 : kSchema80806442,
                     kUncounted,
                     0,
                     true)
        || !padding_is_zero(reader)) {
        return false;
    }
    Memory full{};
    if (!expand(compact, role, full)) {
        return false;
    }
    std::array<std::byte, kPayloadMaximum> staged{};
    Writer writer(staged);
    std::size_t bytes = 0;
    if (!write_schema(writer, full, 0, kSchema8080645A) || !writer.finish(bytes)
        || bytes > output.size()) {
        return false;
    }
    std::copy_n(staged.begin(), bytes, output.begin());
    written = bytes;
    meaningfulBits = writer.bit_count();
    return true;
}

bool canonical_kill_payload(std::span<const std::byte> input) noexcept {
    if (input.empty() || input.size() > kPayloadMaximum) {
        return false;
    }
    Memory full{};
    initialize(full, kDefaults8080645A);
    Reader reader(input);
    return read_schema(reader, full, 0, kSchema8080645A) && padding_is_zero(reader);
}

bool read_kill_facts(std::span<const std::byte> input, KillFacts& output) noexcept {
    if (input.empty() || input.size() > kPayloadMaximum) {
        return false;
    }
    Memory full{};
    initialize(full, kDefaults8080645A);
    Reader reader(input);
    if (!read_schema(reader, full, 0, kSchema8080645A) || !padding_is_zero(reader)
        || load(full, kSourceKindOffset, kWordBytes) != kPlayerSource) {
        return false;
    }
    // The codec stores opcode 35 participant keys as their eight wire-order bytes; native
    // participant keys are scalar SOIDs, and the base actor identity is already a scalar.
    KillFacts result{};
    result.sequence = static_cast<std::uint32_t>(load(full, kSequenceOffset, kWordBytes));
    result.sourcePlayer = load(full, kSourcePlayerOffset, sizeof(std::uint64_t));
    result.killingPlayer = participant_key(full, kKillingActor + kActorKeyOffset);
    result.deadPlayer = participant_key(full, kDeadActor + kActorKeyOffset);
    result.killerFaction = load_team(full, kKillingActor + kActorFactionOffset);
    result.victimFaction = load_team(full, kDeadActor + kActorFactionOffset);
    result.reason = static_cast<std::uint8_t>(load(full, kKillReasonOffset, 1));
    result.ordinaryVictim =
        load(full, kDeadActor + kActorClassOffset, 1) == 0
        && load(full, kDeadActor + kActorHandleOffset, sizeof(std::uint64_t)) == UINT64_MAX;
    output = result;
    return true;
}

namespace {
/** Capture report `C80806470`: the two factions, the participants, then the zone. */
constexpr std::size_t kLosingFactionOffset = offsetof(CaptureReport, losingFaction);
constexpr std::size_t kCapturingFactionOffset = offsetof(CaptureReport, capturingFaction);
// Tag layout: the participant block `C808087bb` and its `C8080880d` entries are unnamed gaps.
/** Participant block: its count word, then the `C808087ba` array of `C8080880d` entries. */
constexpr std::size_t kParticipantCountOffset = offsetof(CaptureReport, participants);
constexpr std::size_t kParticipantsOffset = kParticipantCountOffset + sizeof(std::uint64_t);
constexpr std::size_t kParticipantStride = generated::C8080880d::kSize;
/** Participant entry: its eight key bytes, then its faction. */
constexpr std::size_t kParticipantFactionOffset = 8;
constexpr std::size_t kZoneOffset = offsetof(generated::C80806470, unk328);

/** Decodes a complete capture body with a player source. */
bool read_capture(std::span<const std::byte> input, Memory& full) noexcept {
    if (input.empty() || input.size() > kPayloadMaximum) {
        return false;
    }
    full = {};
    Reader reader(input);
    return read_schema(reader, full, 0, kSchema80806470) && padding_is_zero(reader)
           && load(full, kSourceKindOffset, kWordBytes) == kPlayerSource;
}
} // namespace

bool read_capture_facts(std::span<const std::byte> input, CaptureFacts& output) noexcept {
    Memory full{};
    if (!read_capture(input, full)) {
        return false;
    }
    CaptureFacts facts{};
    facts.sequence = static_cast<std::uint32_t>(load(full, kSequenceOffset, kWordBytes));
    facts.sourcePlayer = load(full, kSourcePlayerOffset, sizeof(std::uint64_t));
    facts.losingFaction = load_team(full, kLosingFactionOffset);
    facts.capturingFaction = load_team(full, kCapturingFactionOffset);
    facts.zone = static_cast<std::uint32_t>(load(full, kZoneOffset, kWordBytes));
    const auto count = load(full, kParticipantCountOffset, kWordBytes);
    if (count > facts.players.size()) {
        return false;
    }
    facts.count = static_cast<std::uint8_t>(count);
    for (std::size_t i = 0; i < facts.count; ++i) {
        const auto entry = kParticipantsOffset + i * kParticipantStride;
        facts.players[i] = participant_key(full, entry);
        facts.factions[i] = load_team(full, entry + kParticipantFactionOffset);
    }
    output = facts;
    return true;
}

bool read_cinematic_facts(std::span<const std::byte> input, CinematicFacts& output) noexcept {
    if (input.empty() || input.size() > kPayloadMaximum) {
        return false;
    }
    Memory full{};
    initialize(full, kDefaults808087BF);
    Reader reader(input);
    if (!read_schema(reader, full, 0, kSchema808087BF) || !padding_is_zero(reader)
        || load(full, kSourceKindOffset, kWordBytes) != kPlayerSource) {
        return false;
    }
    CinematicFacts facts{};
    facts.sequence = static_cast<std::uint32_t>(load(full, kSequenceOffset, kWordBytes));
    facts.sourcePlayer = load(full, kSourcePlayerOffset, sizeof(std::uint64_t));
    facts.group = static_cast<std::uint32_t>(load(full, kCinematicGroupOffset, kWordBytes));
    facts.type = static_cast<std::uint8_t>(load(full, kCinematicTypeOffset, 1));
    facts.slot =
        static_cast<std::uint16_t>(load(full, kCinematicSlotOffset, sizeof(std::uint16_t)));
    facts.timestampMilliseconds = load(full, kCinematicClockOffset, sizeof(std::uint64_t));
    facts.duration = std::bit_cast<float>(
        static_cast<std::uint32_t>(load(full, kCinematicDurationOffset, kWordBytes)));
    if (!std::isfinite(facts.duration)) {
        return false;
    }
    output = facts;
    return true;
}

namespace {
/** The capture report's one player role, holding every participant in native order. */
bool capture_roles(std::span<const std::byte> input, PlayerRoles& output) noexcept {
    CaptureFacts facts{};
    if (!read_capture_facts(input, facts)) {
        return false;
    }
    PlayerRoles result{};
    result.sequence = facts.sequence;
    result.sourcePlayer = facts.sourcePlayer;
    result.count = 1;
    auto& role = result.roles[0];
    role.key = kParticipantPlayerRole;
    for (std::size_t i = 0; i < facts.count; ++i) {
        if (facts.players[i] != 0 && role.count < role.players.size()) {
            role.players[role.count++] = facts.players[i];
        }
    }
    output = result;
    return true;
}
} // namespace

bool read_player_roles(std::span<const std::byte> input,
                       std::int8_t type,
                       PlayerRoles& output) noexcept {
    if (type == targets::kCaptureType) {
        return capture_roles(input, output);
    }
    if (input.empty() || input.size() > kPayloadMaximum
        || (type != targets::kKillType && type != targets::kPlayerEventType
            && type != targets::kSpawnType && type != targets::kFollowUpType)) {
        return false;
    }
    Memory full{};
    if (type == targets::kKillType) {
        initialize(full, kDefaults8080645A);
    } else if (type == targets::kPlayerEventType) {
        initialize(full, kDefaults808087A5);
    } else if (type == targets::kSpawnType) {
        initialize(full, kDefaults80806437);
    }
    Reader reader(input);
    const auto schema = type == targets::kKillType          ? kSchema8080645A
                        : type == targets::kPlayerEventType ? kSchema808087A5
                        : type == targets::kSpawnType       ? kSchema80806437
                                                            : kSchema8080880C;
    if (!read_schema(reader, full, 0, schema) || !padding_is_zero(reader)
        || load(full, kSourceKindOffset, kWordBytes) != kPlayerSource) {
        return false;
    }
    const auto add = [](PlayerRole& role, std::uint64_t player) noexcept {
        if (player != 0 && role.count < role.players.size()) {
            role.players[role.count++] = player;
        }
    };
    PlayerRoles result{};
    result.sequence = static_cast<std::uint32_t>(load(full, kSequenceOffset, kWordBytes));
    result.sourcePlayer = load(full, kSourcePlayerOffset, sizeof(std::uint64_t));
    if (type == targets::kFollowUpType || type == targets::kPlayerEventType) {
        result.count = kPlayerEventRoleCount;
        result.roles[0].key = kPlayerRole;
        add(result.roles[0], participant_key(full, kEventPlayerOffset));
    } else if (type == targets::kSpawnType) {
        result.count = kSpawnRoleCount;
        result.roles[0].key = kDeadPlayerRole;
        add(result.roles[0], participant_key(full, kSpawnDeadOffset));
        result.roles[1].key = kResurrectingPlayerRole;
        add(result.roles[1], participant_key(full, kSpawnResurrectingOffset));
    } else {
        result.count = kKillRoleCount;
        result.roles[0].key = kKillingPlayerRole;
        add(result.roles[0], participant_key(full, kKillingActor + kActorKeyOffset));
        result.roles[1].key = kDeadPlayerRole;
        add(result.roles[1], participant_key(full, kDeadActor + kActorKeyOffset));
        result.roles[2].key = kAssistRole;
        result.roles[3].key = kDamageParticipantRole;
        add(result.roles[3], participant_key(full, kKillingActor + kActorKeyOffset));
        const auto count = load(full, kCanonicalParticipantCount, kWordBytes);
        if (count > kParticipantCapacity) {
            return false;
        }
        for (std::size_t i = 0; i < count; ++i) {
            const auto entry = kCanonicalParticipants + i * kCanonicalParticipantStride;
            if (load(full, entry, 1) < kFirstNonPlayerParticipant) {
                add(result.roles[2], participant_key(full, entry + kParticipantKeyOffset));
                add(result.roles[3], participant_key(full, entry + kParticipantKeyOffset));
            }
        }
    }
    output = result;
    return true;
}
} // namespace sunrise::middleware::bap::activity_message::incident
