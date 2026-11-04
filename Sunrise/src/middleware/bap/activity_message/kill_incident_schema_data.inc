// Native wire schemas of the incident payloads the codec reads and writes, and the default words
// native F8E790/372A60 initialize them with. No runtime game addresses are used. Include from
// kill_incident_codec.cpp only.

// Field records of each schema: offset, opcode (kOptionalFlag marks a presence-bit field), width,
// storage bytes, child schema (65535 for a field without one), arguments.

constexpr Field kFields80806447[] = {
    {0, 0x1, 0, 0, 1, 0x0U, 0x0U},
    {96, 0x1, 0, 0, 7, 0x0U, 0x0U},
    {152, 0x1, 0, 0, 7, 0x0U, 0x0U},
    {208, 0x1, 0, 0, 9, 0x0U, 0x0U},
    {792, 0xB, 32, 4, 65535, 0x0U, 0x0U},
    {796, 0x1, 0, 0, 14, 0x0U, 0x0U},
    {860, 0x1, 0, 0, 14, 0x0U, 0x0U},
    {924, 0x1, 0, 0, 14, 0x0U, 0x0U},
    {988, 0x9, 32, 4, 65535, 0x0U, 0x20U},
    {992, 0x9, 32, 4, 65535, 0x0U, 0x20U},
};
constexpr Field kFields80806454[] = {
    {0, 0x1, 0, 0, 2, 0x0U, 0x0U},
    {64, 0x5, 32, 4, 65535, 0x80000000U, 0x20U},
    {68, 0x3, 3, 1, 65535, 0x1U, 0x3U},
    {69, 0x101, 0, 0, 5, 0x0U, 0x0U},
    {71, 0x2, 1, 1, 65535, 0x0U, 0x0U},
    {72, 0x1, 0, 0, 6, 0x0U, 0x0U},
    {80, 0x1, 0, 0, 6, 0x0U, 0x0U},
    {88, 0x1, 0, 0, 6, 0x0U, 0x0U},
};
constexpr Field kFields808087E2[] = {
    {0, 0x1, 0, 0, 3, 0x0U, 0x0U},
    {24, 0x103, 5, 1, 65535, 0x1U, 0x5U},
    {25, 0x103, 7, 1, 65535, 0x1U, 0x7U},
    {32, 0xD, 0, 12, 65535, 0x80U, 0x0U},
    {48, 0x104, 11, 2, 65535, 0x1U, 0xBU},
    {52, 0x10B, 32, 4, 65535, 0x0U, 0x0U},
    {56, 0xB, 11, 4, 65535, 0x0U, 0x44800000U},
    {60, 0x9, 32, 4, 65535, 0x0U, 0x20U},
};
constexpr Field kFields80809512[] = {
    {0, 0x9, 32, 4, 65535, 0x0U, 0x20U},
    {8, 0x1, 0, 0, 4, 0x0U, 0x0U},
};
constexpr Field kFields80809514[] = {
    {0, 0x3, 3, 1, 65535, 0x1U, 0x3U},
    {8, 0x6, 64, 8, 65535, 0x0U, 0x40U},
};
constexpr Field kFields80804C7C[] = {
    {0, 0x7, 8, 1, 65535, 0x0U, 0x8U},
    {1, 0x7, 3, 1, 65535, 0x0U, 0x3U},
};
constexpr Field kFields80809F7B[] = {
    {0, 0x7, 8, 1, 65535, 0x0U, 0x8U},
};
constexpr Field kFields8080644D[] = {
    {0, 0x1, 0, 0, 8, 0x0U, 0x0U},
    {12, 0x109, 32, 4, 65535, 0x0U, 0x20U},
    {16, 0x109, 32, 4, 65535, 0x0U, 0x20U},
    {20, 0x123, 0, 8, 65535, 0x0U, 0x0U},
    {32, 0x10A, 64, 8, 65535, 0x0U, 0x40U},
    {40, 0x10A, 64, 8, 65535, 0x0U, 0x40U},
    {48, 0x105, 32, 4, 65535, 0x80000000U, 0x20U},
};
constexpr Field kFields808087E3[] = {
    {0, 0x2, 1, 1, 65535, 0x0U, 0x0U},
    {1, 0x2, 1, 1, 65535, 0x0U, 0x0U},
    {2, 0x2, 1, 1, 65535, 0x0U, 0x0U},
    {4, 0x109, 32, 4, 65535, 0x0U, 0x20U},
    {8, 0x3, 3, 1, 65535, 0x1U, 0x3U},
    {9, 0x3, 2, 1, 65535, 0x1U, 0x2U},
    {10, 0x3, 6, 1, 65535, 0x3U, 0x6U},
};
constexpr Field kFields8080644B[] = {
    {0, 0x5, 4, 4, 65535, 0x0U, 0x4U},
    {8, 0x1, 0, 0, 10, 0x1U, 0x0U},
};
constexpr Field kFields80806449[] = {
    {0, 0x1, 0, 0, 11, 0x0U, 0x0U},
};
constexpr Field kFields8080644C[] = {
    {0, 0x3, 3, 1, 65535, 0x1U, 0x3U},
    {8, 0x1, 0, 0, 12, 0x0U, 0x0U},
    {56, 0x3, 8, 1, 65535, 0x80U, 0x8U},
};
constexpr Field kFields8080880D[] = {
    {0, 0x23, 0, 8, 65535, 0x0U, 0x0U},
    {8, 0x3, 6, 1, 65535, 0x3U, 0x6U},
    {16, 0xA, 64, 8, 65535, 0x0U, 0x40U},
    {24, 0x1, 0, 0, 13, 0x0U, 0x0U},
    {40, 0x2, 1, 1, 65535, 0x0U, 0x0U},
    {41, 0x3, 3, 1, 65535, 0x1U, 0x3U},
    {42, 0x3, 2, 1, 65535, 0x1U, 0x2U},
};
constexpr Field kFields8080880E[] = {
    {0, 0xA, 64, 8, 65535, 0x0U, 0x40U},
    {8, 0x5, 32, 4, 65535, 0x80000000U, 0x20U},
};
constexpr Field kFields8080644A[] = {
    {0, 0x5, 4, 4, 65535, 0x0U, 0x4U},
    {4, 0x1, 0, 0, 15, 0x1U, 0x0U},
};
constexpr Field kFields80806448[] = {
    {0, 0x9, 32, 4, 65535, 0x0U, 0x20U},
};
constexpr Field kFields80806442[] = {
    {0, 0x1, 0, 0, 1, 0x0U, 0x0U},
    {96, 0x1, 0, 0, 17, 0x0U, 0x0U},
    {152, 0x1, 0, 0, 17, 0x0U, 0x0U},
    {208, 0x109, 32, 4, 65535, 0x0U, 0x20U},
    {212, 0x109, 32, 4, 65535, 0x0U, 0x20U},
    {216, 0x1, 0, 0, 18, 0x0U, 0x0U},
    {800, 0xB, 32, 4, 65535, 0x0U, 0x0U},
    {804, 0x1, 0, 0, 14, 0x0U, 0x0U},
    {868, 0x1, 0, 0, 14, 0x0U, 0x0U},
    {932, 0x1, 0, 0, 14, 0x0U, 0x0U},
    {996, 0x9, 32, 4, 65535, 0x0U, 0x20U},
    {1000, 0x9, 32, 4, 65535, 0x0U, 0x20U},
};
constexpr Field kFields80806446[] = {
    {0, 0x1, 0, 0, 8, 0x0U, 0x0U},
    {12, 0x109, 32, 4, 65535, 0x0U, 0x20U},
    {16, 0x109, 32, 4, 65535, 0x0U, 0x20U},
    {20, 0x123, 0, 8, 65535, 0x0U, 0x0U},
    {32, 0x10A, 64, 8, 65535, 0x0U, 0x40U},
    {40, 0x10A, 64, 8, 65535, 0x0U, 0x40U},
    {48, 0x105, 32, 4, 65535, 0x80000000U, 0x20U},
};
constexpr Field kFields80806444[] = {
    {0, 0x5, 4, 4, 65535, 0x0U, 0x4U},
    {8, 0x1, 0, 0, 19, 0x1U, 0x0U},
};
constexpr Field kFields80806443[] = {
    {0, 0x1, 0, 0, 20, 0x0U, 0x0U},
};
constexpr Field kFields80806445[] = {
    {0, 0x3, 3, 1, 65535, 0x1U, 0x3U},
    {8, 0x1, 0, 0, 12, 0x0U, 0x0U},
    {56, 0x3, 8, 1, 65535, 0x80U, 0x8U},
};
constexpr Field kFields8080645A[] = {
    {0, 0x1, 0, 0, 22, 0x0U, 0x0U},
    {720, 0x5, 32, 4, 65535, 0x80000000U, 0x20U},
    {724, 0x3, 3, 1, 65535, 0x1U, 0x3U},
    {725, 0x1, 0, 0, 5, 0x0U, 0x0U},
    {727, 0x2, 1, 1, 65535, 0x0U, 0x0U},
    {848, 0x9, 32, 4, 65535, 0x0U, 0x20U},
    {852, 0x9, 32, 4, 65535, 0x0U, 0x20U},
    {856, 0xB, 32, 4, 65535, 0x0U, 0x0U},
    {860, 0x3, 2, 1, 65535, 0x1U, 0x2U},
};
constexpr Field kFields808087E5[] = {
    {0, 0x1, 0, 0, 3, 0x0U, 0x0U},
    {24, 0x1, 0, 0, 23, 0x0U, 0x0U},
    {96, 0x1, 0, 0, 23, 0x0U, 0x0U},
    {168, 0x1, 0, 0, 24, 0x0U, 0x0U},
    {680, 0x3, 8, 1, 65535, 0x80U, 0x8U},
    {681, 0x3, 8, 1, 65535, 0x80U, 0x8U},
    {688, 0xD, 0, 12, 65535, 0x80U, 0x0U},
    {704, 0x104, 11, 2, 65535, 0x1U, 0xBU},
    {708, 0xB, 32, 4, 65535, 0x0U, 0x0U},
    {712, 0xB, 32, 4, 65535, 0x0U, 0x0U},
    {716, 0x9, 32, 4, 65535, 0x0U, 0x20U},
};
constexpr Field kFields80808801[] = {
    {0, 0x19, 0, 8, 65535, 0x0U, 0x0U},
    {8, 0x2, 1, 1, 65535, 0x0U, 0x0U},
    {9, 0x2, 1, 1, 65535, 0x0U, 0x0U},
    {10, 0x2, 1, 1, 65535, 0x0U, 0x0U},
    {12, 0x9, 32, 4, 65535, 0x0U, 0x20U},
    {16, 0x9, 32, 4, 65535, 0x0U, 0x20U},
    {20, 0x9, 32, 4, 65535, 0x0U, 0x20U},
    {24, 0x23, 0, 8, 65535, 0x0U, 0x0U},
    {32, 0x3, 6, 1, 65535, 0x3U, 0x6U},
    {40, 0xA, 64, 8, 65535, 0x0U, 0x40U},
    {48, 0x3, 3, 1, 65535, 0x1U, 0x3U},
    {49, 0x3, 2, 1, 65535, 0x1U, 0x2U},
    {56, 0xA, 64, 8, 65535, 0x0U, 0x40U},
    {64, 0x5, 32, 4, 65535, 0x80000000U, 0x20U},
};
constexpr Field kFields808087E7[] = {
    {0, 0x5, 4, 4, 65535, 0x0U, 0x4U},
    {8, 0x1, 0, 0, 25, 0x1U, 0x0U},
};
constexpr Field kFields808087E6[] = {
    {0, 0x1, 0, 0, 26, 0x0U, 0x0U},
};
constexpr Field kFields808087EB[] = {
    {0, 0x3, 3, 1, 65535, 0x1U, 0x3U},
    {1, 0x3, 5, 1, 65535, 0x1U, 0x5U},
    {8, 0x1, 0, 0, 12, 0x0U, 0x0U},
};
constexpr Field kFields80806436[] = {
    {0, 0x1, 0, 0, 3, 0x0U, 0x0U},
    {24, 0x1, 0, 0, 12, 0x0U, 0x0U},
    {72, 0x1, 0, 0, 12, 0x0U, 0x0U},
    {120, 0x5, 32, 4, 65535, 0x80000000U, 0x20U},
};
constexpr Field kFields8080880C[] = {
    {0, 0x1, 0, 0, 3, 0x0U, 0x0U},
    {24, 0x23, 0, 8, 65535, 0x0U, 0x0U},
    {32, 0x3, 6, 1, 65535, 0x3U, 0x6U},
    {40, 0xA, 64, 8, 65535, 0x0U, 0x40U},
    {48, 0x1, 0, 0, 13, 0x0U, 0x0U},
};
constexpr Field kFields808087A5[] = {
    {0, 0x1, 0, 0, 3, 0x0U, 0x0U},
    {24, 0x1, 0, 0, 12, 0x0U, 0x0U},
};
constexpr Field kFields80806437[] = {
    {0, 0x1, 0, 0, 3, 0x0U, 0x0U},
    {24, 0x3, 3, 1, 65535, 0x1U, 0x3U},
    {32, 0x1, 0, 0, 12, 0x0U, 0x0U},
    {80, 0x1, 0, 0, 12, 0x0U, 0x0U},
    {128, 0x1, 0, 0, 31, 0x0U, 0x0U},
};
constexpr Field kFields80809FEC[] = {
    {0, 0xB, 32, 4, 65535, 0x0U, 0x0U},
};
constexpr Field kFields808087BF[] = {
    {0, 0x1, 0, 0, 3, 0x0U, 0x0U},
    {24, 0x1, 0, 0, 12, 0x0U, 0x0U},
    {72, 0x1, 0, 0, 33, 0x0U, 0x0U},
    {80, 0xA, 64, 8, 65535, 0x0U, 0x40U},
    {88, 0xB, 32, 4, 65535, 0x0U, 0x0U},
};
constexpr Field kFields80809C42[] = {
    {0, 0x9, 32, 4, 65535, 0x0U, 0x20U},
    {4, 0x3, 7, 1, 65535, 0x1U, 0x7U},
    {6, 0x4, 16, 2, 65535, 0x8000U, 0x10U},
};
constexpr Field kFields808087BA[] = {
    {0, 0x1, 0, 0, 12, 0x0U, 0x0U},
};
constexpr Field kFields808087BB[] = {
    {0, 0x5, 5, 4, 65535, 0x0U, 0x5U},
    {8, 0x1, 0, 0, 34, 0x1U, 0x0U},
};
constexpr Field kFields80806470[] = {
    {0, 0x1, 0, 0, 3, 0x0U, 0x0U},
    {24, 0x3, 6, 1, 65535, 0x3U, 0x6U},
    {25, 0x3, 6, 1, 65535, 0x3U, 0x6U},
    {32, 0x1, 0, 0, 35, 0x0U, 0x0U},
    {808, 0x9, 32, 4, 65535, 0x0U, 0x20U},
};
// Every schema: its fields, byte size, element count and stride. A fixed array schema's size is the
// whole array.
constexpr Schema kSchemas[] = {
    {kFields80806447, generated::C80806447::kSize, 0, 0},
    {kFields80806454, generated::C80806454::kSize, 0, 0},
    {kFields808087E2, generated::C808087e2::kSize, 0, 0},
    {kFields80809512, generated::C80809512::kSize, 0, 0},
    {kFields80809514, generated::C80809514::kSize, 0, 0},
    {kFields80804C7C, generated::C80804c7c::kSize, 0, 0},
    {kFields80809F7B, generated::C80809f7b::kSize, 8, 1},
    {kFields8080644D, generated::C8080644d::kSize, 0, 0},
    {kFields808087E3, generated::C808087e3::kSize, 0, 0},
    {kFields8080644B, generated::C8080644b::kSize, 0, 0},
    {kFields80806449, generated::C80806449::kSize, 9, 64},
    {kFields8080644C, generated::C8080644c::kSize, 0, 0},
    {kFields8080880D, generated::C8080880d::kSize, 0, 0},
    {kFields8080880E, generated::C8080880e::kSize, 0, 0},
    {kFields8080644A, generated::C8080644a::kSize, 0, 0},
    {kFields80806448, generated::C80806448::kSize, 15, 4},
    {kFields80806442, generated::C80806442::kSize, 0, 0},
    {kFields80806446, generated::C80806446::kSize, 0, 0},
    {kFields80806444, generated::C80806444::kSize, 0, 0},
    {kFields80806443, generated::C80806443::kSize, 9, 64},
    {kFields80806445, generated::C80806445::kSize, 0, 0},
    {kFields8080645A, generated::C8080645a::kSize, 0, 0},
    {kFields808087E5, generated::C808087e5::kSize, 0, 0},
    {kFields80808801, generated::C80808801::kSize, 0, 0},
    {kFields808087E7, generated::C808087e7::kSize, 0, 0},
    {kFields808087E6, generated::C808087e6::kSize, 9, 56},
    {kFields808087EB, generated::C808087eb::kSize, 0, 0},
    {kFields80806436, generated::C80806436::kSize, 0, 0},
    {kFields8080880C, generated::C8080880c::kSize, 0, 0},
    {kFields808087A5, generated::C808087a5::kSize, 0, 0},
    {kFields80806437, generated::C80806437::kSize, 0, 0},
    {kFields80809FEC, generated::C80809fec::kSize, 3, 4},
    {kFields808087BF, generated::C808087bf::kSize, 0, 0},
    {kFields80809C42, generated::ClientRef::kSize, 0, 0},
    {kFields808087BA, generated::C808087ba::kSize, 16, 48},
    {kFields808087BB, generated::C808087bb::kSize, 0, 0},
    {kFields80806470, generated::C80806470::kSize, 0, 0},
};
// Index into kSchemas of each schema the codec starts from.
constexpr std::size_t kSchema80806447 = 0;
constexpr std::size_t kSchema80806442 = 16;
constexpr std::size_t kSchema8080645A = 21;
constexpr std::size_t kSchema80806436 = 27;
constexpr std::size_t kSchema8080880C = 28;
constexpr std::size_t kSchema808087A5 = 29;
constexpr std::size_t kSchema80806437 = 30;
constexpr std::size_t kSchema808087BF = 32;
constexpr std::size_t kSchema80806470 = 36;
// Default words of each payload schema: byte offset, then eight little-endian bytes.
constexpr DefaultWord kDefaults80806447[] = {
    {24, 0x000000000000FFFFULL},  {40, 0x3F80000000000000ULL},  {48, 0x000000000000FFFFULL},
    {56, 0x811C9DC500000000ULL},  {64, 0x00000000FFFFFFFFULL},  {96, 0x811C9DC500000000ULL},
    {104, 0x811C9DC500000000ULL}, {112, 0x00000000811C9DC5ULL}, {152, 0x811C9DC500000000ULL},
    {160, 0x811C9DC500000000ULL}, {168, 0x00000000811C9DC5ULL}, {232, 0x00000000000000FFULL},
    {248, 0xFFFFFFFFFFFFFFFFULL}, {272, 0x00000000000000FFULL}, {296, 0x00000000000000FFULL},
    {312, 0xFFFFFFFFFFFFFFFFULL}, {336, 0x00000000000000FFULL}, {360, 0x00000000000000FFULL},
    {376, 0xFFFFFFFFFFFFFFFFULL}, {400, 0x00000000000000FFULL}, {424, 0x00000000000000FFULL},
    {440, 0xFFFFFFFFFFFFFFFFULL}, {464, 0x00000000000000FFULL}, {488, 0x00000000000000FFULL},
    {504, 0xFFFFFFFFFFFFFFFFULL}, {528, 0x00000000000000FFULL}, {552, 0x00000000000000FFULL},
    {568, 0xFFFFFFFFFFFFFFFFULL}, {592, 0x00000000000000FFULL}, {616, 0x00000000000000FFULL},
    {632, 0xFFFFFFFFFFFFFFFFULL}, {656, 0x00000000000000FFULL}, {680, 0x00000000000000FFULL},
    {696, 0xFFFFFFFFFFFFFFFFULL}, {720, 0x00000000000000FFULL}, {744, 0x00000000000000FFULL},
    {760, 0xFFFFFFFFFFFFFFFFULL}, {784, 0x00000000000000FFULL},
};
constexpr DefaultWord kDefaults80806442[] = {
    {24, 0x000000000000FFFFULL},  {40, 0x3F80000000000000ULL},  {48, 0x000000000000FFFFULL},
    {56, 0x811C9DC500000000ULL},  {64, 0x00000000FFFFFFFFULL},  {96, 0x811C9DC500000000ULL},
    {104, 0x811C9DC500000000ULL}, {112, 0x00000000811C9DC5ULL}, {152, 0x811C9DC500000000ULL},
    {160, 0x811C9DC500000000ULL}, {168, 0x00000000811C9DC5ULL}, {208, 0x811C9DC5811C9DC5ULL},
    {240, 0x00000000000000FFULL}, {256, 0xFFFFFFFFFFFFFFFFULL}, {280, 0x00000000000000FFULL},
    {304, 0x00000000000000FFULL}, {320, 0xFFFFFFFFFFFFFFFFULL}, {344, 0x00000000000000FFULL},
    {368, 0x00000000000000FFULL}, {384, 0xFFFFFFFFFFFFFFFFULL}, {408, 0x00000000000000FFULL},
    {432, 0x00000000000000FFULL}, {448, 0xFFFFFFFFFFFFFFFFULL}, {472, 0x00000000000000FFULL},
    {496, 0x00000000000000FFULL}, {512, 0xFFFFFFFFFFFFFFFFULL}, {536, 0x00000000000000FFULL},
    {560, 0x00000000000000FFULL}, {576, 0xFFFFFFFFFFFFFFFFULL}, {600, 0x00000000000000FFULL},
    {624, 0x00000000000000FFULL}, {640, 0xFFFFFFFFFFFFFFFFULL}, {664, 0x00000000000000FFULL},
    {688, 0x00000000000000FFULL}, {704, 0xFFFFFFFFFFFFFFFFULL}, {728, 0x00000000000000FFULL},
    {752, 0x00000000000000FFULL}, {768, 0xFFFFFFFFFFFFFFFFULL}, {792, 0x00000000000000FFULL},
};
constexpr DefaultWord kDefaults8080645A[] = {
    {24, 0xFFFFFFFFFFFFFFFFULL},  {32, 0x811C9DC500000000ULL},  {40, 0x811C9DC5811C9DC5ULL},
    {96, 0xFFFFFFFFFFFFFFFFULL},  {104, 0x811C9DC500000000ULL}, {112, 0x811C9DC5811C9DC5ULL},
    {176, 0x000000000000FF00ULL}, {192, 0x00000000000000FFULL}, {208, 0xFFFFFFFFFFFFFFFFULL},
    {232, 0x000000000000FF00ULL}, {248, 0x00000000000000FFULL}, {264, 0xFFFFFFFFFFFFFFFFULL},
    {288, 0x000000000000FF00ULL}, {304, 0x00000000000000FFULL}, {320, 0xFFFFFFFFFFFFFFFFULL},
    {344, 0x000000000000FF00ULL}, {360, 0x00000000000000FFULL}, {376, 0xFFFFFFFFFFFFFFFFULL},
    {400, 0x000000000000FF00ULL}, {416, 0x00000000000000FFULL}, {432, 0xFFFFFFFFFFFFFFFFULL},
    {456, 0x000000000000FF00ULL}, {472, 0x00000000000000FFULL}, {488, 0xFFFFFFFFFFFFFFFFULL},
    {512, 0x000000000000FF00ULL}, {528, 0x00000000000000FFULL}, {544, 0xFFFFFFFFFFFFFFFFULL},
    {568, 0x000000000000FF00ULL}, {584, 0x00000000000000FFULL}, {600, 0xFFFFFFFFFFFFFFFFULL},
    {624, 0x000000000000FF00ULL}, {640, 0x00000000000000FFULL}, {656, 0xFFFFFFFFFFFFFFFFULL},
    {680, 0x000000000000FFFFULL}, {696, 0x3F80000000000000ULL}, {704, 0x000000000000FFFFULL},
    {712, 0x811C9DC500000000ULL}, {720, 0x00000000FFFFFFFFULL},
};
constexpr DefaultWord kDefaults80806436[] = {
    {8, 0x0000000000000001ULL},
    {16, 0xFFFFFFFFFFFFFFFFULL},
    {32, 0x00000000000000FFULL},
    {48, 0xFFFFFFFFFFFFFFFFULL},
    {80, 0x00000000000000FFULL},
    {96, 0xFFFFFFFFFFFFFFFFULL},
};
constexpr DefaultWord kDefaults808087A5[] = {
    {32, 0x00000000000000FFULL},
    {48, 0xFFFFFFFFFFFFFFFFULL},
};
constexpr DefaultWord kDefaults80806437[] = {
    {40, 0x00000000000000FFULL},
    {56, 0xFFFFFFFFFFFFFFFFULL},
    {88, 0x00000000000000FFULL},
    {104, 0xFFFFFFFFFFFFFFFFULL},
};
constexpr DefaultWord kDefaults808087BF[] = {
    {32, 0x00000000000000FFULL},
    {48, 0xFFFFFFFFFFFFFFFFULL},
    {72, 0xFFFF00FF811C9DC5ULL},
};
