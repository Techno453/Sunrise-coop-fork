#include "statistic_program.h"

#include <cmath>

#include "../schema/generated/tag_classes_808000.h"
#include "definition_index_table.h"
#include "field_reader.h"

namespace sunrise::middleware::content::packages::tables::statistic_program {
namespace {
namespace generated = schema::generated;
using Bytes = std::span<const std::byte>;

/** Root arrays of computed-column and event expression rows. */
constexpr std::size_t kComputedList = offsetof(generated::C80809a12, functions);
constexpr std::uint32_t kComputedRowClass = generated::C80809a2a::kClass;
constexpr std::size_t kComputedRowStride = sizeof(generated::C80809a2a);
constexpr std::size_t kEventList = offsetof(generated::C80809a12, h18e11192);
constexpr std::uint32_t kEventRowClass = generated::C80809a29::kClass;
constexpr std::size_t kEventRowStride = sizeof(generated::C80809a29);

/** Where one expression row kind keeps its output key, flags, expression and variable keys. */
struct RowLayout final {
    std::size_t output;
    std::size_t flags;
    std::size_t expression;
    std::size_t variables;
};
// Tag layout: each row's flags word is the unnamed gap after its output key.
constexpr RowLayout kComputedLayout{offsetof(generated::C80809a2a, unk0),
                                    offsetof(generated::C80809a2a, gap4),
                                    offsetof(generated::C80809a2a, expression),
                                    offsetof(generated::C80809a2a, sourceStats)};
constexpr RowLayout kEventLayout{offsetof(generated::C80809a29, unk8),
                                 offsetof(generated::C80809a29, gapC),
                                 offsetof(generated::C80809a29, expression),
                                 offsetof(generated::C80809a29, sourceStats)};
/** An event row starts with its event and role keys. */
constexpr std::size_t kEventKeyOffset = offsetof(generated::C80809a29, unk0);
constexpr std::size_t kRoleKeyOffset = offsetof(generated::C80809a29, unk4);

/** Compiled expression: byte program, float4 constant pool, input and output counts. */
constexpr std::size_t kCodeList = offsetof(generated::SCompiledExpression, bytecode);
constexpr std::uint32_t kCodeClass = generated::ExpressionCode::kClass;
constexpr std::size_t kCodeStride = sizeof(generated::ExpressionCode);
constexpr std::size_t kConstantList = offsetof(generated::SCompiledExpression, constantBuffer);
constexpr std::uint32_t kConstantClass = generated::ExpressionConstant::kClass;
constexpr std::size_t kConstantStride = sizeof(generated::ExpressionConstant);
constexpr std::size_t kInputCountOffset = offsetof(generated::SCompiledExpression, unknown56One);
constexpr std::size_t kOutputCountOffset = offsetof(generated::SCompiledExpression, activeField);
constexpr std::size_t kSpecializationOffset =
    offsetof(generated::SCompiledExpression, evaluatorOverride);
/** The row's variable keys, which the compiled expression reads as its inputs. */
constexpr std::uint32_t kVariableClass = generated::SU32HashElement::kClass;
constexpr std::size_t kVariableStride = sizeof(generated::SU32HashElement);
/** Every installed expression writes one output and uses the generic, unspecialized path. */
constexpr std::uint32_t kOutputCount = 1;
constexpr std::uint32_t kGenericSpecialization = 0;

/** Native 3B7A80 opcodes of the installed corpus. */
constexpr std::uint8_t kAdd = 0x01;
constexpr std::uint8_t kDivide = 0x04;
constexpr std::uint8_t kMinimum = 0x08;
constexpr std::uint8_t kMaximum = 0x09;
constexpr std::uint8_t kPushConstant = 0x34;
constexpr std::uint8_t kPushInput = 0x3C;
constexpr std::uint8_t kPopOutput = 0x3E;
/** A binary opcode pops two vectors and pushes one. */
constexpr std::size_t kBinaryOperands = 2;
/** The only output a program writes, and the smallest divisor magnitude evaluated. */
constexpr std::uint8_t kOutputIndex = 0;
constexpr float kMinimumDivisor = 1.0F;

/** Reads one optional typed list through the shared `read_array` reader; empty has no rows. */
bool array(Bytes bytes,
           std::size_t descriptor,
           std::uint32_t cls,
           std::size_t stride,
           std::size_t maximum,
           std::size_t& start,
           std::size_t& count) noexcept {
    Array rows{};
    if (!read_array(bytes, descriptor, cls, stride, rows) || rows.count > maximum) {
        return false;
    }
    start = rows.count == 0 ? 0 : rows.dataOffset;
    count = static_cast<std::size_t>(rows.count);
    return true;
}

bool expression(Bytes bytes, std::size_t row, bool event, Program& output) noexcept {
    Program result{};
    if (event
        && (!read(bytes, row + kEventKeyOffset, result.eventKey)
            || !read(bytes, row + kRoleKeyOffset, result.roleKey))) {
        return false;
    }
    const RowLayout& layout = event ? kEventLayout : kComputedLayout;
    if (!read(bytes, row + layout.output, result.outputKey)
        || !read(bytes, row + layout.flags, result.flags)) {
        return false;
    }
    const auto descriptor = row + layout.expression;
    std::size_t start{}, count{};
    std::uint64_t inputCount{};
    std::uint32_t outputCount{}, specialization{};
    if (!read(bytes, descriptor + kInputCountOffset, inputCount)
        || !read(bytes, descriptor + kOutputCountOffset, outputCount) || outputCount != kOutputCount
        || !read(bytes, descriptor + kSpecializationOffset, specialization)
        || specialization != kGenericSpecialization
        || !array(bytes,
                  descriptor + kCodeList,
                  kCodeClass,
                  kCodeStride,
                  result.code.size(),
                  start,
                  count)) {
        return false;
    }
    result.codeCount = static_cast<std::uint8_t>(count);
    for (std::size_t i = 0; i < count; ++i) {
        result.code[i] = std::to_integer<std::uint8_t>(bytes[start + i]);
    }
    if (!array(bytes,
               descriptor + kConstantList,
               kConstantClass,
               kConstantStride,
               result.constants.size(),
               start,
               count)) {
        return false;
    }
    result.constantCount = static_cast<std::uint8_t>(count);
    for (std::size_t i = 0; i < count; ++i) {
        if (!read(bytes, start + (i * kConstantStride), result.constants[i])) {
            return false;
        }
    }
    if (!array(bytes,
               row + layout.variables,
               kVariableClass,
               kVariableStride,
               result.variables.size(),
               start,
               count)
        || inputCount != count) {
        return false;
    }
    result.variableCount = static_cast<std::uint8_t>(count);
    for (std::size_t i = 0; i < count; ++i) {
        if (!read(bytes, start + (i * kVariableStride), result.variables[i])) {
            return false;
        }
    }
    if (!valid(result)) {
        return false;
    }
    output = result;
    return true;
}
} // namespace

bool valid(const Program& program) noexcept {
    if (program.codeCount == 0 || program.codeCount > program.code.size()
        || program.constantCount > program.constants.size()
        || program.variableCount > program.variables.size()) {
        return false;
    }
    for (std::size_t i = 0; i < program.constantCount; ++i) {
        for (const float value : program.constants[i]) {
            if (!std::isfinite(value)) {
                return false;
            }
        }
    }
    std::size_t depth = 0;
    for (std::size_t pc = 0; pc < program.codeCount;) {
        const auto op = program.code[pc++];
        if (op == kPushConstant || op == kPushInput || op == kPopOutput) {
            if (pc == program.codeCount) {
                return false;
            }
            const auto index = program.code[pc++];
            if (op == kPopOutput) {
                return index == kOutputIndex && depth == 1 && pc == program.codeCount;
            }
            if (index >= (op == kPushConstant ? program.constantCount : program.variableCount)) {
                return false;
            }
            ++depth;
        } else if (op == kAdd || op == kDivide || op == kMinimum || op == kMaximum) {
            if (depth < kBinaryOperands) {
                return false;
            }
            --depth;
        } else {
            return false;
        }
    }
    return false;
}

bool parse(Bytes bytes, Catalog& output) noexcept {
    if (bytes.size() < kScoreboardRootSize) {
        return false;
    }
    Catalog result{};
    std::uint32_t players{}, teams{};
    if (!read(bytes, kPlayerStatisticCountOffset, players) || players == 0
        || players > result.playerColumns.size() || !read(bytes, kTeamStatisticCountOffset, teams)
        || teams == 0 || teams > result.teamColumns.size()) {
        return false;
    }
    result.playerColumnCount = static_cast<std::uint8_t>(players);
    result.teamColumnCount = static_cast<std::uint8_t>(teams);
    for (std::size_t i = 0; i < players; ++i) {
        const auto row = kPlayerStatisticsOffset + (i * kStatisticStride);
        if (!read(bytes, row + kStatisticKeyOffset, result.playerColumns[i])
            || !read(bytes, row + kStatisticMetadataOffset, result.playerColumnMetadata[i])) {
            return false;
        }
    }
    for (std::size_t i = 0; i < teams; ++i) {
        if (!read(bytes,
                  kTeamStatisticsOffset + (i * kStatisticStride) + kStatisticKeyOffset,
                  result.teamColumns[i])) {
            return false;
        }
    }
    std::size_t start{}, count{};
    if (!array(bytes,
               kComputedList,
               kComputedRowClass,
               kComputedRowStride,
               result.computed.size(),
               start,
               count)) {
        return false;
    }
    result.computedCount = static_cast<std::uint8_t>(count);
    for (std::size_t i = 0; i < count; ++i) {
        if (!expression(bytes, start + (i * kComputedRowStride), false, result.computed[i])) {
            return false;
        }
    }
    if (!array(bytes,
               kEventList,
               kEventRowClass,
               kEventRowStride,
               result.events.size(),
               start,
               count)) {
        return false;
    }
    result.eventCount = static_cast<std::uint8_t>(count);
    for (std::size_t i = 0; i < count; ++i) {
        if (!expression(bytes, start + (i * kEventRowStride), true, result.events[i])) {
            return false;
        }
    }
    output = result;
    return true;
}

bool evaluate(const Program& program, std::span<const Vector> inputs, Vector& output) noexcept {
    if (!valid(program) || inputs.size() != program.variableCount) {
        return false;
    }
    for (const auto& input : inputs) {
        for (const float value : input) {
            if (!std::isfinite(value)) {
                return false;
            }
        }
    }
    // A valid program pushes at most one vector per two code bytes.
    std::array<Vector, kCodeCapacity> stack{};
    std::size_t depth = 0;
    for (std::size_t pc = 0; pc < program.codeCount;) {
        const auto op = program.code[pc++];
        if (op == kPushConstant || op == kPushInput) {
            const auto index = program.code[pc++];
            stack[depth++] = op == kPushConstant ? program.constants[index] : inputs[index];
        } else if (op == kPopOutput) {
            output = stack[0];
            return true;
        } else {
            const auto right = stack[--depth];
            auto& left = stack[depth - 1];
            for (std::size_t lane = 0; lane < left.size(); ++lane) {
                const float a = left[lane], b = right[lane];
                if (op == kAdd) {
                    left[lane] = a + b;
                } else if (op == kDivide) {
                    if (std::fabs(b) < kMinimumDivisor) {
                        return false;
                    }
                    left[lane] = a / b;
                } else if (op == kMinimum) {
                    left[lane] = a < b ? a : b;
                } else {
                    left[lane] = a > b ? a : b;
                }
                if (!std::isfinite(left[lane])) {
                    return false;
                }
            }
        }
    }
    return false;
}

} // namespace sunrise::middleware::content::packages::tables::statistic_program
