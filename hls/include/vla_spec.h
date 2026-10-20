// vla_spec.h — Interface specification for the vegetation-loss-alerting (VLA) 
// pipeline, H1a. Single source of truth for types, bit widths, number formats,
// template parameters and the runtime register map.
// Every arithmetic rule marked "EXACT RULE" must be reproduced bit-for-bit by
// the reference model (model/reference_model.py).


#ifndef VLA_SPEC_H
#define VLA_SPEC_H
#include <ap_int.h>
#include <ap_fixed.h>
#include <hls_stream.h>

namespace vla {

static const int SPEC_VERSION = 2;

// 1. TILE GEOMETRY
static const int TILE_W   = 256;               // pixels per row (10 m bands)
static const int TILE_H   = 256;               // rows per tile
static const int TILE_PIX = TILE_W * TILE_H;   // 65,536
static const int SCL_W    = TILE_W / 2;        // 128 (SCL is 20 m)
static const int SCL_H    = TILE_H / 2;        // 128

// Indices for [date][slot] arrays
static const int T1 = 0, T2 = 1;               // date: earlier, later
static const int SLOT_A = 0, SLOT_B = 1;       // band slot: ND = (A-B)/(A+B)

// 2. BASE TYPES (independent of configuration)
typedef ap_uint<16> dn_t;      // raw Sentinel-2 L2A digital number
typedef ap_uint<4>  scl_t;     // Scene Classification Layer class code (0..11)
typedef ap_int<17>  band_t;    // DN minus L2A offset; range [-65536, 65535]
typedef ap_uint<17> cnt_t;     // pixel counter; max 65,536
typedef ap_ufixed<16, 2> gain_t;  // harmonisation gain, Q2.14, range [0, 4)
typedef ap_int<16>  boff_t;    // harmonisation offset, DN units
typedef ap_ufixed<16, 0> frac_t;  // tile fraction P, Q0.16, range [0, 1)
typedef ap_uint<12> clsmask_t; // one bit per SCL class; 1 = invalid
typedef ap_uint<2>  flag_t;    // tile decision

// 3. ENUMERATION
enum PostOp { POST_IDENTITY = 0,   // index = ND            (NDVI, NDWI)
              POST_MUL_A    = 1,   // index = ND * band A   (NIRv when A = NIR)
              POST_MUL_B    = 2 }; // index = ND * band B

enum Dir    { DIR_LOSS = 0,        // change if  delta < -T
              DIR_GAIN = 1 };      // change if  delta >  T

enum TileFlag { FLAG_NO_CHANGE   = 0,
                FLAG_CHANGE      = 1,
                FLAG_INSUFFICIENT = 2 };  // valid_count < V_min

// Sentinel-2 SCL class codes
enum SclClass { SCL_NO_DATA = 0, SCL_SATURATED = 1, SCL_DARK = 2,
                SCL_CLOUD_SHADOW = 3, SCL_VEGETATION = 4, SCL_NOT_VEGETATED = 5,
                SCL_WATER = 6, SCL_UNCLASSIFIED = 7, SCL_CLOUD_MED = 8,
                SCL_CLOUD_HIGH = 9, SCL_CIRRUS = 10, SCL_SNOW = 11 };

// Default invalid set: no-data, saturated, cloud shadow, cloud med/high, cirrus
// bits 0,1,3,8,9,10  ->  0b0111_0000_1011
static const unsigned DEFAULT_INVALID_CLASSES = 0x70B;

// 4. INDEX NUMBER FORMAT, chosen by post-operation
//    IDENTITY : ap_fixed<W_ND, 2>          (ND value in [-1, 1])
//    MUL_A/B  : ap_fixed<W_ND + 16, 18>    (exact product ND x band_t,
//                                           |value| <= 65,536)
template <PostOp P, int W_ND> struct IdxFmt;
template <int W_ND> struct IdxFmt<POST_IDENTITY, W_ND> { static const int W = W_ND;      static const int I = 2;  };
template <int W_ND> struct IdxFmt<POST_MUL_A,    W_ND> { static const int W = W_ND + 16; static const int I = 18; };
template <int W_ND> struct IdxFmt<POST_MUL_B,    W_ND> { static const int W = W_ND + 16; static const int I = 18; };

// 5. CONFIGURATION (all compile-time / template parameters)
//   W_ND        total width of the ND output (2 integer bits incl. sign)
//   POST        post-operation mode (C5)
//   DIR         temporal comparison direction (C6)
//   HARM_EN     include harmonisation stage C2 (false = stage removed)
//   L2A_OFFSET  radiometric offset subtracted in C1 (1000 for PB >= 04.00)
//   ND_LATENCY  divider pipeline latency in C4 (0 = let the tool choose)
template <int W_ND_, PostOp POST_, Dir DIR_,
          bool HARM_EN_ = true, int L2A_OFFSET_ = 1000, int ND_LATENCY_ = 0>
struct VlaCfg {
    static_assert(W_ND_ >= 6 && W_ND_ <= 18, "W_ND out of supported range");
    static_assert(L2A_OFFSET_ >= 0 && L2A_OFFSET_ <= 65535, "bad L2A offset");

    static const int    W_ND       = W_ND_;
    static const int    F_ND       = W_ND_ - 2;          // fractional bits
    static const PostOp POST       = POST_;
    static const Dir    DIR        = DIR_;
    static const bool   HARM_EN    = HARM_EN_;
    static const int    L2A_OFFSET = L2A_OFFSET_;
    static const int    ND_LATENCY = ND_LATENCY_;

    static const int IDX_W = IdxFmt<POST_, W_ND_>::W;
    static const int IDX_I = IdxFmt<POST_, W_ND_>::I;

    typedef ap_fixed<W_ND_, 2>               nd_t;    // C4 output
    typedef ap_fixed<IDX_W, IDX_I>           idx_t;   // C5 output
    typedef ap_fixed<IDX_W + 1, IDX_I + 1>   delta_t; // C6 internal, exact
    typedef idx_t                            thr_t;   // T register, >= 0

    // Inter-stage stream words
    // C1 -> C2 -> C4 : offset-corrected bands, both dates
    struct band_px_t {
        band_t b[2][2];      // [date][slot]
        bool   nodata[2];    // DN == 0 in either slot of that date
    };
    // C4 -> C5 : normalized difference per date, plus the post-op operand
    struct nd_px_t {
        nd_t   nd[2];        // [date]
        band_t mul[2];       // band selected by POST (unused for IDENTITY)
        bool   valid[2];     // den > 0 && !nodata
    };
    // C5 -> C6 : final index per date
    struct idx_px_t {
        idx_t  idx[2];
        bool   valid[2];
    };
    // C6 -> C7 : 2 bits per pixel (H2 partition boundary)
    struct chg_px_t {
        bool change;
        bool valid;
    };
    // C7 output
    struct tile_result_t {
        flag_t flag;
        cnt_t  change_count;
        cnt_t  valid_count;
    };

    // Runtime registers (AXI4-Lite)
    struct regs_t {
        gain_t    gain[2][2];       // C2, [date][slot], default 1.0
        boff_t    offset[2][2];     // C2, [date][slot], default 0
        clsmask_t invalid_classes;  // C3, default DEFAULT_INVALID_CLASSES
        thr_t     T;                // C6, per-pixel change threshold
        frac_t    P;                // C7, min change fraction of valid pixels
        cnt_t     V_min;            // C7, min valid pixels for a decision
    };
};

// 6. NAMED CONFIGURATIONS
typedef VlaCfg<16, POST_IDENTITY, DIR_LOSS> CfgNdvi16;   // R0 base
typedef VlaCfg< 8, POST_IDENTITY, DIR_LOSS> CfgNdvi8;    // R3 precision variant
typedef VlaCfg<16, POST_MUL_A,    DIR_LOSS> CfgNirv16;   // R1 (A = NIR)
typedef VlaCfg<16, POST_IDENTITY, DIR_GAIN> CfgNdwi16;   // R2 (A = Green, B = NIR)

// 7. DEFAULT REGISTER VALUES
template <class Cfg>
inline void default_regs(typename Cfg::regs_t &r) {
    for (int d = 0; d < 2; d++)
        for (int s = 0; s < 2; s++) {
            r.gain[d][s]   = 1.0;
            r.offset[d][s] = 0;
        }
    r.invalid_classes = DEFAULT_INVALID_CLASSES;
    r.T     = (Cfg::POST == POST_IDENTITY) ? 0.2 : 600.0; // NIRv in DN units: set per experiment
    r.P     = 0.05;
    r.V_min = TILE_PIX / 4;                               // 16,384 pixels
}

// 8. EXACT RULES (reference model must match bit-for-bit)
// C1  Band sync + L2A offset
//     nodata[d] = (dn[d][A] == 0) || (dn[d][B] == 0)
//     b[d][s]   = (int)dn[d][s] - L2A_OFFSET                 (no saturation needed)
//
// C2  Harmonisation (skipped entirely when HARM_EN == false)
//     y = (gain.raw * b) >> 14         arithmetic shift = floor
//     y = y + offset
//     b' = saturate(y, -65536, 65535)
//     nodata passes through unchanged.
//
// C3  Validity mask
//     scl(r, c)   = SCL[r >> 1][c >> 1]                      (2x nearest neighbour)
//     inv[d]      = invalid_classes bit[scl_d(r, c)]
//     mask_valid  = !inv[T1] && !inv[T2]
//     SCL codes 12..15 are treated as invalid.
//
// C4  Normalized difference, per date (F = F_ND)
//     num = A - B ; den = A + B
//     if (den <= 0 || nodata)  nd = 0, valid = false
//     else
//         q  = floor((2 * num * 2^F + den) / (2 * den))      round half up
//         q  = clamp(q, -2^F, +2^F)                           i.e. [-1.0, 1.0]
//         nd = q / 2^F,  valid = true
//     mul[d] = b[d][A] for POST_MUL_A, b[d][B] for POST_MUL_B, 0 otherwise
//
// C5  Post-operation
//     IDENTITY : idx = nd
//     MUL_A/B  : idx = nd * mul             exact, no rounding (fits idx_t)
//     valid passes through.
//
// C6  Temporal comparison
//     delta  = idx[T2] - idx[T1]                              exact (delta_t)
//     cmp    = (DIR == DIR_LOSS) ? (delta < -T) : (delta > T)
//     valid  = mask_valid && valid[T1] && valid[T2]
//     change = cmp && valid
//
// C7  Tile decision (once per tile, after TILE_PIX pixels)
//     change_count = sum(change) ; valid_count = sum(valid)
//     if (valid_count < V_min)                   flag = FLAG_INSUFFICIENT
//     else if (change_count > P * valid_count)   flag = FLAG_CHANGE   (exact compare)
//     else                                       flag = FLAG_NO_CHANGE
} 

#endif
