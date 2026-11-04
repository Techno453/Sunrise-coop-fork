#include "../../encoding/bit_reader.h"
#include "../../encoding/byte_order.h"
#include "incident.h"

namespace sunrise::middleware::bap::activity_message::incident {

bool resolve_targets(std::span<const std::byte> payload,
                     const content::packages::tables::incident_targets::Catalog& catalog,
                     content::packages::tables::incident_targets::Selection& output) noexcept {
    namespace target = content::packages::tables::incident_targets;
    Incident envelope{};
    if (!fully_framed(payload, envelope) || envelope.primaryTarget >= catalog.targetCount
        || catalog.targetCount > catalog.targets.size()) {
        return false;
    }
    const auto type = catalog.targets[envelope.primaryTarget].type;
    if (type < 0) {
        return false;
    }
    target::Selection result{};
    if (envelope.hasCompressedSelector) {
        if (envelope.extraTargetCount != 0) {
            return false;
        }
        encoding::bits::Reader reader(payload);
        std::array<std::byte, kSelectorMaximum> selector{};
        if (!reader.skip(kTargetWidth + kExtraCountWidth + kSelectorPresenceWidth
                         + kSelectorLengthWidth)) {
            return false;
        }
        for (std::size_t i = 0; i < envelope.selectorLength; ++i) {
            std::uint64_t value{};
            if (!reader.read(encoding::kBitsPerByte, value)) {
                return false;
            }
            selector[i] = static_cast<std::byte>(value);
        }
        if (!target::expand(catalog, std::span(selector).first(envelope.selectorLength), result)
            || result.count == 0 || result.targets[0] != envelope.primaryTarget) {
            return false;
        }
    } else {
        result.targets[0] = static_cast<std::uint16_t>(envelope.primaryTarget);
        result.count = static_cast<std::uint8_t>(envelope.extraTargetCount + 1);
        for (std::size_t i = 0; i < envelope.extraTargetCount; ++i) {
            result.targets[i + 1] = static_cast<std::uint16_t>(envelope.extraTargets[i]);
        }
    }
    for (std::size_t i = 0; i < result.count; ++i) {
        if (result.targets[i] >= catalog.targetCount
            || catalog.targets[result.targets[i]].type != type) {
            return false;
        }
    }
    output = result;
    return true;
}

} // namespace sunrise::middleware::bap::activity_message::incident
