#pragma once

#include <aurex/base/integer.hpp>

#include <array>
#include <cstdint>
#include <sstream>
#include <string>
#include <string_view>

namespace aurex::test::randomized {

constexpr std::uint64_t RANDOM_SOURCE_LEGAL_PROGRAM_SEED = 0xA11CE5EED1234567ULL;
constexpr std::uint64_t RANDOM_SOURCE_PARSER_RECOVERY_SEED = 0xC0FFEEBADF00D123ULL;
constexpr std::uint64_t RANDOM_SOURCE_LEXER_NOISE_SEED = 0xBAD5EED105EED042ULL;
constexpr std::uint64_t RANDOM_SOURCE_SPLITMIX_INCREMENT = 0x9E3779B97F4A7C15ULL;
constexpr std::uint64_t RANDOM_SOURCE_SPLITMIX_FIRST_MULTIPLIER = 0xBF58476D1CE4E5B9ULL;
constexpr std::uint64_t RANDOM_SOURCE_SPLITMIX_SECOND_MULTIPLIER = 0x94D049BB133111EBULL;
constexpr int RANDOM_SOURCE_SPLITMIX_FIRST_SHIFT = 30;
constexpr int RANDOM_SOURCE_SPLITMIX_SECOND_SHIFT = 27;
constexpr int RANDOM_SOURCE_SPLITMIX_FINAL_SHIFT = 31;
constexpr base::usize RANDOM_SOURCE_DEFAULT_LEGAL_PROGRAM_COUNT = 48;
constexpr base::usize RANDOM_SOURCE_DEFAULT_PARSER_RECOVERY_COUNT = 96;
constexpr base::usize RANDOM_SOURCE_DEFAULT_LEXER_NOISE_COUNT = 96;
constexpr base::usize RANDOM_SOURCE_INTEGRATION_PROGRAM_COUNT = 16;
constexpr base::usize RANDOM_SOURCE_MIN_EXTRA_HELPERS = 1;
constexpr base::usize RANDOM_SOURCE_MAX_EXTRA_HELPERS = 4;
constexpr base::usize RANDOM_SOURCE_MIN_BODY_STATEMENTS = 5;
constexpr base::usize RANDOM_SOURCE_BODY_STATEMENT_SPAN = 9;
constexpr base::usize RANDOM_SOURCE_MIN_RECOVERY_FRAGMENT_COUNT = 12;
constexpr base::usize RANDOM_SOURCE_RECOVERY_FRAGMENT_SPAN = 32;
constexpr base::usize RANDOM_SOURCE_MIN_LEXER_FRAGMENT_COUNT = 8;
constexpr base::usize RANDOM_SOURCE_LEXER_FRAGMENT_SPAN = 40;
constexpr base::usize RANDOM_SOURCE_MUTATION_SPAN = 5;
constexpr base::usize RANDOM_SOURCE_IDENTIFIER_SPAN = 10'000;
constexpr base::usize RANDOM_SOURCE_SMALL_INTEGER_SPAN = 23;
constexpr base::usize RANDOM_SOURCE_MAX_ARRAY_LENGTH = 5;
constexpr base::usize RANDOM_SOURCE_LITERAL_FORMAT_COUNT = 3;
constexpr base::usize RANDOM_SOURCE_TYPE_SHAPE_COUNT = 5;
constexpr base::usize RANDOM_SOURCE_I32_EXPR_SHAPE_COUNT = 11;
constexpr base::usize RANDOM_SOURCE_BOOL_EXPR_SHAPE_COUNT = 5;
constexpr base::usize RANDOM_SOURCE_STATEMENT_SHAPE_COUNT = 11;
constexpr base::usize RANDOM_SOURCE_BINARY_DIGIT_SPAN = 2;
constexpr base::usize RANDOM_SOURCE_FUZZ_GENERATOR_KIND_COUNT = 3;
constexpr int RANDOM_SOURCE_BITS_PER_BYTE = 8;

class DeterministicRandom final {
public:
    explicit DeterministicRandom(const std::uint64_t seed) noexcept
        : state_(seed) {}

    [[nodiscard]] std::uint64_t next() noexcept {
        this->state_ += RANDOM_SOURCE_SPLITMIX_INCREMENT;
        std::uint64_t value = this->state_;
        value = (value ^ (value >> RANDOM_SOURCE_SPLITMIX_FIRST_SHIFT)) *
                RANDOM_SOURCE_SPLITMIX_FIRST_MULTIPLIER;
        value = (value ^ (value >> RANDOM_SOURCE_SPLITMIX_SECOND_SHIFT)) *
                RANDOM_SOURCE_SPLITMIX_SECOND_MULTIPLIER;
        return value ^ (value >> RANDOM_SOURCE_SPLITMIX_FINAL_SHIFT);
    }

    [[nodiscard]] base::usize index(const base::usize size) noexcept {
        return size == 0 ? 0 : static_cast<base::usize>(this->next() % size);
    }

    [[nodiscard]] bool coin() noexcept {
        return (this->next() & 1ULL) != 0;
    }

private:
    std::uint64_t state_ = 0;
};

template <base::usize Size>
[[nodiscard]] std::string_view choose(
    DeterministicRandom& random,
    const std::array<std::string_view, Size>& values
) {
    return values[random.index(values.size())];
}

[[nodiscard]] inline std::string make_name(DeterministicRandom& random, const std::string_view prefix) {
    return std::string(prefix) + std::to_string(random.index(RANDOM_SOURCE_IDENTIFIER_SPAN));
}

[[nodiscard]] inline std::string integer_literal(DeterministicRandom& random) {
    const base::usize value = random.index(RANDOM_SOURCE_SMALL_INTEGER_SPAN);
    switch (random.index(RANDOM_SOURCE_LITERAL_FORMAT_COUNT)) {
    case 0:
        return std::to_string(value);
    case 1:
        return "0x" + std::to_string(value);
    default:
        return "0b" + std::to_string(value % RANDOM_SOURCE_BINARY_DIGIT_SPAN);
    }
}

[[nodiscard]] inline std::string type_name(DeterministicRandom& random) {
    static constexpr std::array PRIMITIVE_TYPES {
        std::string_view {"i32"},
        std::string_view {"u8"},
        std::string_view {"bool"},
        std::string_view {"usize"},
    };

    std::string type = std::string(choose(random, PRIMITIVE_TYPES));
    switch (random.index(RANDOM_SOURCE_TYPE_SHAPE_COUNT)) {
    case 0:
        return "*mut " + type;
    case 1:
        return "*const " + type;
    case 2:
        return "[" + std::to_string(random.index(RANDOM_SOURCE_MAX_ARRAY_LENGTH) + 1U) + "]" + type;
    default:
        return type;
    }
}

[[nodiscard]] inline std::string i32_expr(DeterministicRandom& random) {
    static constexpr std::array BINARY_OPS {
        std::string_view {" + "},
        std::string_view {" - "},
        std::string_view {" * "},
        std::string_view {" | "},
        std::string_view {" ^ "},
        std::string_view {" & "},
    };

    switch (random.index(RANDOM_SOURCE_I32_EXPR_SHAPE_COUNT)) {
    case 0:
        return integer_literal(random);
    case 1:
        return "helper_" + std::to_string(random.index(RANDOM_SOURCE_MAX_EXTRA_HELPERS)) + "(" +
               integer_literal(random) + ")";
    case 2:
        return "cast[i32](" + integer_literal(random) + ")";
    case 3:
        return "cast[i32](sizeof[" + type_name(random) + "])";
    case 4:
        return "cast[i32](alignof[" + type_name(random) + "])";
    case 5:
        return "- " + integer_literal(random);
    case 6:
        return "id(" + integer_literal(random) + ")";
    case 7:
        return "id::[i32](" + integer_literal(random) + ")";
    case 8:
        return "first(Holder[i32] { value: " + integer_literal(random) + " })";
    default:
        return "(" + integer_literal(random) + std::string(choose(random, BINARY_OPS)) +
               integer_literal(random) + ")";
    }
}

[[nodiscard]] inline std::string comparison_expr(DeterministicRandom& random) {
    static constexpr std::array COMPARISON_OPS {
        std::string_view {" == "},
        std::string_view {" != "},
        std::string_view {" < "},
        std::string_view {" <= "},
        std::string_view {" > "},
        std::string_view {" >= "},
    };

    return i32_expr(random) + std::string(choose(random, COMPARISON_OPS)) + i32_expr(random);
}

[[nodiscard]] inline std::string bool_expr(DeterministicRandom& random) {
    switch (random.index(RANDOM_SOURCE_BOOL_EXPR_SHAPE_COUNT)) {
    case 0:
        return random.coin() ? "true" : "false";
    case 1:
        return "!(" + comparison_expr(random) + ")";
    case 2:
        return "(" + comparison_expr(random) + ") && true";
    case 3:
        return "(" + comparison_expr(random) + ") || false";
    default:
        return comparison_expr(random);
    }
}

inline void append_statement(
    DeterministicRandom& random,
    std::ostringstream& out,
    const base::usize index
) {
    const std::string local = "v" + std::to_string(index);
    switch (random.index(RANDOM_SOURCE_STATEMENT_SHAPE_COUNT)) {
    case 0:
        out << "  let " << local << ": i32 = " << i32_expr(random) << ";\n";
        break;
    case 1:
        out << "  var " << local << ": i32 = " << i32_expr(random) << ";\n";
        out << "  " << local << " += " << integer_literal(random) << ";\n";
        break;
    case 2:
        out << "  if " << bool_expr(random) << " { total += " << i32_expr(random)
            << "; } else { total -= " << integer_literal(random) << "; }\n";
        break;
    case 3:
        out << "  while total < " << (random.index(RANDOM_SOURCE_SMALL_INTEGER_SPAN) + 1U)
            << " { total += 1; break; }\n";
        break;
    case 4:
        out << "  for var loop_" << index << ": i32 = 0; loop_" << index << " < "
            << (random.index(RANDOM_SOURCE_MAX_ARRAY_LENGTH) + 1U) << "; loop_" << index
            << " += 1 { total += loop_" << index << "; }\n";
        break;
    case 5:
        out << "  let " << local << ": bool = " << bool_expr(random) << ";\n";
        break;
    case 6:
        out << "  let " << local << ": i32 = match Mode_a { Mode_a => "
            << i32_expr(random) << ", Mode_b => " << i32_expr(random) << ", };\n";
        break;
    case 7:
        out << "  let " << local << ": Holder[i32] = Holder[i32] { value: " << i32_expr(random) << " };\n"
            << "  total += first(" << local << ");\n";
        break;
    case 8:
        out << "  let " << local << " = make_holder(" << i32_expr(random) << ");\n"
            << "  total += " << local << ".value;\n";
        break;
    case 9:
        out << "  let " << local << ": Holder[bool] = Holder[bool] { value: id::[bool]("
            << (random.coin() ? "true" : "false") << ") };\n";
        break;
    default:
        out << "  { total += " << i32_expr(random) << "; }\n";
        break;
    }
}

[[nodiscard]] inline std::string legal_program(DeterministicRandom& random, const base::usize case_index) {
    std::ostringstream out;
    out << "module randomized.case_" << case_index << ";\n"
        << "type Count = i32;\n"
        << "struct Pair { left: i32; right: i32; }\n"
        << "struct Holder[T] { value: T; }\n"
        << "enum Mode: u8 { a = 1, b = 2, }\n";

    out << "fn id[T](value: T) -> T { return value; }\n"
        << "fn first[T](holder: Holder[T]) -> T { return holder.value; }\n"
        << "fn make_holder[T](value: T) -> Holder[T] {\n"
        << "  return Holder[T] { value: id(value) };\n"
        << "}\n";

    const base::usize helper_count =
        RANDOM_SOURCE_MIN_EXTRA_HELPERS + random.index(RANDOM_SOURCE_MAX_EXTRA_HELPERS);
    for (base::usize helper = 0; helper < helper_count; ++helper) {
        out << "fn helper_" << helper << "(value: i32) -> i32 {\n"
            << "  let pair: Pair = Pair { left: value, right: " << integer_literal(random) << " };\n"
            << "  return pair.left + pair.right;\n"
            << "}\n";
    }
    for (base::usize helper = helper_count; helper < RANDOM_SOURCE_MAX_EXTRA_HELPERS; ++helper) {
        out << "fn helper_" << helper << "(value: i32) -> i32 { return value; }\n";
    }

    out << "fn main() -> i32 {\n"
        << "  var total: i32 = " << integer_literal(random) << ";\n";
    const base::usize statement_count =
        RANDOM_SOURCE_MIN_BODY_STATEMENTS + random.index(RANDOM_SOURCE_BODY_STATEMENT_SPAN);
    for (base::usize statement = 0; statement < statement_count; ++statement) {
        append_statement(random, out, statement);
    }
    out << "  let ptr: *mut i32 = &total;\n"
        << "  unsafe { *ptr = *ptr + helper_0(1); }\n"
        << "  return total - total;\n"
        << "}\n";
    return out.str();
}

[[nodiscard]] inline std::string parser_recovery_source(
    DeterministicRandom& random,
    const base::usize case_index
) {
    static constexpr std::array FRAGMENTS {
        std::string_view {"fn broken("},
        std::string_view {"let x ="},
        std::string_view {"if true {"},
        std::string_view {"match value {"},
        std::string_view {"struct S { field:"},
        std::string_view {"enum E: u8 { a ="},
        std::string_view {"type T = *mut "},
        std::string_view {"return"},
        std::string_view {"((((("},
        std::string_view {"}}}}"},
        std::string_view {"1 + * /"},
        std::string_view {"Result<Wrap<i32>>"},
        std::string_view {"ptrcast[*mut i32]("},
        std::string_view {"for var i: i32 = 0; i <"},
        std::string_view {"pub import missing."},
        std::string_view {"extern c { fn printf(format: *const u8, ...)"},
    };

    std::ostringstream out;
    out << "module randomized.recovery_" << case_index << ";\n";
    const base::usize fragment_count =
        RANDOM_SOURCE_MIN_RECOVERY_FRAGMENT_COUNT + random.index(RANDOM_SOURCE_RECOVERY_FRAGMENT_SPAN);
    for (base::usize index = 0; index < fragment_count; ++index) {
        out << choose(random, FRAGMENTS);
        if (random.index(RANDOM_SOURCE_MUTATION_SPAN) == 0) {
            out << ' ' << make_name(random, "name_");
        }
        out << (random.coin() ? '\n' : ' ');
    }
    return out.str();
}

[[nodiscard]] inline std::string lexer_noise_source(DeterministicRandom& random, const base::usize case_index) {
    static constexpr std::array FRAGMENTS {
        std::string_view {"module randomized.lexer_noise;"},
        std::string_view {"@#$"},
        std::string_view {"\"unterminated"},
        std::string_view {"c\"unterminated"},
        std::string_view {"b'wide'"},
        std::string_view {"/* unterminated"},
        std::string_view {"0x"},
        std::string_view {"0b102"},
        std::string_view {"1__2"},
        std::string_view {"1.e+"},
        std::string_view {"\x01\x02"},
        std::string_view {"fn ok() -> i32 { return 0; }"},
    };

    std::ostringstream out;
    out << "// lexer noise case " << case_index << "\n";
    const base::usize fragment_count =
        RANDOM_SOURCE_MIN_LEXER_FRAGMENT_COUNT + random.index(RANDOM_SOURCE_LEXER_FRAGMENT_SPAN);
    for (base::usize index = 0; index < fragment_count; ++index) {
        out << choose(random, FRAGMENTS);
        out << (random.coin() ? '\n' : ' ');
    }
    return out.str();
}

} // namespace aurex::test::randomized
