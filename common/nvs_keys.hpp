/**
 * @file nvs_keys.hpp
 * @brief Every NVS namespace and key used by the VX Binocle apps, in one place.
 *
 * Convention:
 * - Namespaces are `vx_<domain>`, keys are short snake_case. The namespace says what the setting belongs to, so the key
 *   does not repeat it.
 * - A value with a unit ends in the unit: m, km, ohm, pct, degc, rpm, ms, bp (basis points), ckph (0.01 km/h).
 * - A feature switch ends in `_en`. A choice between two states names the state that 1 means (`theme_light`).
 * - ESP-IDF limits namespaces and keys to 15 characters. Every name below is checked at compile time.
 *
 * Exceptions kept on purpose:
 * - The odometer and trip live in the `nvs_odo` partition, namespace `storage`, keys `odometer_m` and `trip_m`. They are
 *   frozen so the car's distance is never put at risk by a rename.
 * - The gear estimator blob (`vx_gear` / `params`) belongs to the vx-gear-estimator component and is not declared here.
 *
 * Before this layout, settings sat in the default partition, namespace `storage`, under other names. The table in
 * vx::nvs::legacy lists them; nvs_settings.hpp copies them to the new names once at boot.
 *
 * Pure C++ with no ESP-IDF dependency, so it builds on the host for unit tests.
 */

#pragma once

#include <cstddef>
#include <cstdint>

namespace vx::nvs
{
    /** Longest namespace or key name ESP-IDF accepts (NVS_KEY_NAME_MAX_SIZE - 1). */
    inline constexpr std::size_t NAME_MAX_LEN = 15;

    /** @return true when @p s is a non-empty name of at most NAME_MAX_LEN characters. */
    constexpr bool fits(const char *s)
    {
        std::size_t n = 0;
        while (s[n] != '\0')
            ++n;
        return n > 0 && n <= NAME_MAX_LEN;
    }

    /** Boot bookkeeping shared by every board and its factory app. */
    namespace sys
    {
        inline constexpr const char *NS = "vx_sys";
        /** i8: last app slot booted, -1 factory, 0 ota_0, 1 ota_1. Read by the factory apps. */
        inline constexpr const char *BOOT_SLOT = "boot_slot";
        static_assert(fits(NS) && fits(BOOT_SLOT));
    }

    /** Interface board: fuel sender calibration. */
    namespace fuel
    {
        inline constexpr const char *NS = "vx_fuel";
        /** u8: 1 when the full-tank resistance self-learning is on. */
        inline constexpr const char *LEARN_EN = "learn_en";
        /** u16: full-tank sender resistance in ohm, learned or calibrated. */
        inline constexpr const char *FULL_OHM = "full_ohm";
        /** u16: low fuel warning threshold in %. */
        inline constexpr const char *LOW_PCT = "low_pct";
        static_assert(fits(NS) && fits(LEARN_EN) && fits(FULL_OHM) && fits(LOW_PCT));
    }

    /** Interface board: coolant warning. */
    namespace coolant
    {
        inline constexpr const char *NS = "vx_coolant";
        /** u16: overtemperature warning threshold in degrees C. */
        inline constexpr const char *HOT_DEGC = "hot_degc";
        static_assert(fits(NS) && fits(HOT_DEGC));
    }

    /** Interface board: ECE R39 speed compliance mode (see r39_compliance.hpp for the meaning and limits). */
    namespace r39
    {
        inline constexpr const char *NS = "vx_r39";
        /** u8: 1 when the compliance mode is on. */
        inline constexpr const char *EN = "en";
        /** u16: A as basis points added to 1. */
        inline constexpr const char *A_BP = "a_bp";
        /** i16: B in 0.01 km/h. */
        inline constexpr const char *B_CKPH = "b_ckph";
        static_assert(fits(NS) && fits(EN) && fits(A_BP) && fits(B_CKPH));
    }

    /** Interface board: odometer, in the nvs_odo partition. Frozen names, see the file comment. */
    namespace odo
    {
        inline constexpr const char *PARTITION = "nvs_odo";
        inline constexpr const char *NS = "storage";
        /** u32: odometer in metres. */
        inline constexpr const char *ODOMETER_M = "odometer_m";
        /** u32: trip in metres. */
        inline constexpr const char *TRIP_M = "trip_m";
        static_assert(fits(PARTITION) && fits(NS) && fits(ODOMETER_M) && fits(TRIP_M));
    }

    /** Left and right display boards: user settings. */
    namespace disp
    {
        inline constexpr const char *NS = "vx_disp";
        /** u8: 1 shows mph, 0 km/h. */
        inline constexpr const char *UNIT_MPH = "unit_mph";
        /** u8: 1 shows the gear position instead of the RPM. */
        inline constexpr const char *GEAR_SHOW = "gear_show";
        /** u8: 1 when the vehicle overrides the RPM alarm. */
        inline constexpr const char *ALARM_OVR = "alarm_ovr";
        /** u32: RPM alarm threshold in rpm. */
        inline constexpr const char *ALARM_RPM = "alarm_rpm";
        /** u8: 1 when the RPM alarm blinks. */
        inline constexpr const char *ALARM_BLINK = "alarm_blink";
        /** u8: 1 when the shift indicator is on. */
        inline constexpr const char *SHIFT_EN = "shift_en";
        /** u32: orange shift threshold in rpm. */
        inline constexpr const char *SHIFT_MID_RPM = "shift_mid_rpm";
        /** u32: red shift threshold in rpm. */
        inline constexpr const char *SHIFT_TOP_RPM = "shift_top_rpm";
        /** u32: RPM display decimation factor. */
        inline constexpr const char *RPM_DECIM = "rpm_decim";
        /** u8: 1 when the overtemperature buzzer is on. */
        inline constexpr const char *HOT_BUZZ_EN = "hot_buzz_en";
        /** u8: backlight in the dark theme, 0..100 %. */
        inline constexpr const char *BL_DARK_PCT = "bl_dark_pct";
        /** u8: backlight in the light theme, 0..100 %. */
        inline constexpr const char *BL_LIGHT_PCT = "bl_light_pct";
        /** u8: 1 when the theme is locked against automatic light/dark switching. */
        inline constexpr const char *THEME_LOCKED = "theme_locked";
        /** u8: 1 when the light theme is active. */
        inline constexpr const char *THEME_LIGHT = "theme_light";
        static_assert(fits(NS) && fits(UNIT_MPH) && fits(GEAR_SHOW) && fits(ALARM_OVR) && fits(ALARM_RPM) &&
                      fits(ALARM_BLINK) && fits(SHIFT_EN) && fits(SHIFT_MID_RPM) && fits(SHIFT_TOP_RPM) &&
                      fits(RPM_DECIM) && fits(HOT_BUZZ_EN) && fits(BL_DARK_PCT) && fits(BL_LIGHT_PCT) &&
                      fits(THEME_LOCKED) && fits(THEME_LIGHT));
    }

    /** Settings stored before the vx_* layout: default partition, namespace "storage". */
    namespace legacy
    {
        inline constexpr const char *NS = "storage";

        /** Stored integer type of a setting. */
        enum class Type : uint8_t
        {
            U8,
            I8,
            U16,
            I16,
            U32,
        };

        /** One renamed setting. */
        struct Rename
        {
            const char *oldKey;
            const char *newNs;
            const char *newKey;
            Type type;
        };

        /** Every setting renamed by the vx_* layout. The odometer is not in it (frozen). */
        inline constexpr Rename RENAMES[] = {
            {"lastPart", sys::NS, sys::BOOT_SLOT, Type::I8},
            {"fuel_learn_en", fuel::NS, fuel::LEARN_EN, Type::U8},
            {"fuel_full_r", fuel::NS, fuel::FULL_OHM, Type::U16},
            {"lo_fuel_thr", fuel::NS, fuel::LOW_PCT, Type::U16},
            {"overtemp_th", coolant::NS, coolant::HOT_DEGC, Type::U16},
            {"r39_en", r39::NS, r39::EN, Type::U8},
            {"r39_a_bp", r39::NS, r39::A_BP, Type::U16},
            {"r39_b_ckph", r39::NS, r39::B_CKPH, Type::I16},
            {"mph_on", disp::NS, disp::UNIT_MPH, Type::U8},
            {"gear_on", disp::NS, disp::GEAR_SHOW, Type::U8},
            {"rpm_al_overr", disp::NS, disp::ALARM_OVR, Type::U8},
            {"rpm_al_thr", disp::NS, disp::ALARM_RPM, Type::U32},
            {"rpm_al_blnk", disp::NS, disp::ALARM_BLINK, Type::U8},
            {"shft_ind", disp::NS, disp::SHIFT_EN, Type::U8},
            {"shft_mid", disp::NS, disp::SHIFT_MID_RPM, Type::U32},
            {"shft_top", disp::NS, disp::SHIFT_TOP_RPM, Type::U32},
            {"rpm_dec", disp::NS, disp::RPM_DECIM, Type::U32},
            {"overt_bzz", disp::NS, disp::HOT_BUZZ_EN, Type::U8},
            {"dark_bg", disp::NS, disp::BL_DARK_PCT, Type::U8},
            {"light_bg", disp::NS, disp::BL_LIGHT_PCT, Type::U8},
            {"th_locked", disp::NS, disp::THEME_LOCKED, Type::U8},
            {"light_th", disp::NS, disp::THEME_LIGHT, Type::U8},
        };

        constexpr bool allFit()
        {
            for (const Rename &r : RENAMES)
                if (!fits(r.oldKey) || !fits(r.newNs) || !fits(r.newKey))
                    return false;
            return fits(NS);
        }
        static_assert(allFit(), "every legacy and new name must fit the NVS limit");
    }
}
