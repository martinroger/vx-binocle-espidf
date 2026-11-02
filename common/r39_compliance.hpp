/**
 * @file r39_compliance.hpp
 * @brief ECE R39 speed compliance mode: NVS keys, defaults, limits and the speed transform.
 *
 * Shared by the interface board app (applies the transform in the fast metrics packaging task) and the ITF factory
 * app (edits the stored values and previews the curve), so both use the same keys, limits and formula.
 *
 * Stored values (default "nvs" partition, namespace vx::nvs::r39::NS, keys in nvs_keys.hpp):
 * - EN     (u8)  : 1 when the compliance mode is on.
 * - A_BP   (u16) : percentage added by A, in basis points (0.01 %). A = 1 + a_bp / 10000.
 * - B_CKPH (i16) : offset B in 0.01 km/h.
 *
 * Output when enabled, with v the true speed in km/h:
 *   s = clamp(v / blend_end_kph, 0, 1), w = s^2 (3 - 2 s)   (smoothstep)
 *   out = clamp(A v + B w, 0, SPEED_MAX_KPH)
 * so a stopped car reads 0, the curve is continuous and monotonic, and above blend_end_kph it is exactly A v + B.
 * With A in [1, 1.10] and B in [0, 4] km/h the result always satisfies R39: 0 <= out - v <= 0.1 v + 4 km/h.
 *
 * Pure C++ with no ESP-IDF dependency, so it builds on the host for unit tests.
 */

#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace r39
{
    inline constexpr bool DEFAULT_EN = false;
    inline constexpr uint16_t DEFAULT_A_BP = 400;  /**< +4.00 % */
    inline constexpr int16_t DEFAULT_B_CKPH = 200; /**< 2.00 km/h */

    inline constexpr uint16_t A_BP_MAX = 1000;  /**< +10.00 %, top of the R39 allowance */
    inline constexpr int16_t B_CKPH_MIN = 0;    /**< Negative B would under-read at low speed */
    inline constexpr int16_t B_CKPH_MAX = 400;  /**< 4.00 km/h, top of the R39 allowance */

    /** Top of the ITF_speed_kph signal range in km/h. */
    inline constexpr float SPEED_MAX_KPH = 400.0f;

    /** Default end of the blend band in km/h, used when no Kconfig value is given. */
    inline constexpr float DEFAULT_BLEND_END_KPH = 10.0f;

    /** Working parameters, ready to apply. */
    struct Params
    {
        bool enabled = DEFAULT_EN;
        float a = 1.0f + DEFAULT_A_BP / 10000.0f;
        float b_kph = DEFAULT_B_CKPH / 100.0f;
        float blend_end_kph = DEFAULT_BLEND_END_KPH;

        /**
         * @brief Speed to broadcast for a measured speed.
         * @param v_kph Measured (true) speed in km/h.
         * @return The compliant speed when enabled, otherwise @p v_kph. Always finite and within 0..SPEED_MAX_KPH.
         */
        float apply(float v_kph) const
        {
            if (!std::isfinite(v_kph) || v_kph <= 0.0f)
                return 0.0f;
            if (!enabled)
                return std::min(v_kph, SPEED_MAX_KPH);

            float w = 1.0f;
            if (blend_end_kph > 0.0f && v_kph < blend_end_kph)
            {
                const float s = v_kph / blend_end_kph;
                w = s * s * (3.0f - 2.0f * s);
            }
            const float out = a * v_kph + b_kph * w;
            if (!std::isfinite(out))
                return 0.0f;
            return std::clamp(out, 0.0f, SPEED_MAX_KPH);
        }
    };

    /** Values as stored in NVS. */
    struct RawParams
    {
        uint8_t en = DEFAULT_EN ? 1 : 0;
        uint16_t a_bp = DEFAULT_A_BP;
        int16_t b_ckph = DEFAULT_B_CKPH;

        /** @return true when every stored value is within the compliant limits. */
        constexpr bool in_range() const
        {
            return en <= 1 && a_bp <= A_BP_MAX && b_ckph >= B_CKPH_MIN && b_ckph <= B_CKPH_MAX;
        }

        /** @return A as a percentage, e.g. 4.0 for +4 %. */
        constexpr float a_pct() const { return a_bp / 100.0f; }

        /** @return B in km/h. */
        constexpr float b_kph() const { return b_ckph / 100.0f; }

        /**
         * @brief Working parameters. Out-of-range values are clamped to the limits.
         * @param blend_end_kph End of the low-speed blend band in km/h.
         */
        Params to_params(float blend_end_kph = DEFAULT_BLEND_END_KPH) const
        {
            Params p;
            p.enabled = (en == 1);
            p.a = 1.0f + std::min(a_bp, A_BP_MAX) / 10000.0f;
            p.b_kph = std::clamp(b_ckph, B_CKPH_MIN, B_CKPH_MAX) / 100.0f;
            p.blend_end_kph = blend_end_kph;
            return p;
        }
    };

    /** @return The default stored values (mode off, +4 %, 2 km/h). */
    constexpr RawParams defaults() { return RawParams{}; }

    /**
     * @brief Converts a user value with at most two decimals to hundredths, e.g. 4.25 -> 425.
     * @param v Value typed by the user (percent or km/h).
     * @param max_hundredths Upper limit in hundredths.
     * @param[out] out Hundredths when the call returns true.
     * @return false when @p v is not finite, negative, above the limit, or has more than two decimals.
     */
    inline bool hundredths_from_user(double v, int32_t max_hundredths, int32_t *out)
    {
        if (!std::isfinite(v) || v < 0.0)
            return false;
        const double scaled = v * 100.0;
        const double rounded = std::round(scaled);
        if (std::fabs(scaled - rounded) > 1e-6 || rounded > (double)max_hundredths)
            return false;
        *out = (int32_t)rounded;
        return true;
    }
} // namespace r39
