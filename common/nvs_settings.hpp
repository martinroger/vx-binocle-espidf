/**
 * @file nvs_settings.hpp
 * @brief Helpers around the default NVS partition shared by every VX Binocle app: typed load-or-initialise, the
 *        one-time copy of settings from their legacy names, and the boot slot record read by the factory apps.
 *
 * Key names live in nvs_keys.hpp. Call nvs_flash_init() before any function here.
 */

#pragma once

#include <cstdint>

#include "esp_err.h"
#include "esp_log.h"
#include "nvs.h"
#include "nvs_keys.hpp"

namespace vx::nvs
{
    namespace detail
    {
        inline constexpr const char *LOG_TAG_NVS = "vx_nvs";

        /** Typed nvs_get_* / nvs_set_* for each stored integer type. */
        template <typename T>
        struct Io;
        template <>
        struct Io<uint8_t>
        {
            static esp_err_t get(nvs_handle_t h, const char *k, uint8_t *v) { return nvs_get_u8(h, k, v); }
            static esp_err_t set(nvs_handle_t h, const char *k, uint8_t v) { return nvs_set_u8(h, k, v); }
        };
        template <>
        struct Io<int8_t>
        {
            static esp_err_t get(nvs_handle_t h, const char *k, int8_t *v) { return nvs_get_i8(h, k, v); }
            static esp_err_t set(nvs_handle_t h, const char *k, int8_t v) { return nvs_set_i8(h, k, v); }
        };
        template <>
        struct Io<uint16_t>
        {
            static esp_err_t get(nvs_handle_t h, const char *k, uint16_t *v) { return nvs_get_u16(h, k, v); }
            static esp_err_t set(nvs_handle_t h, const char *k, uint16_t v) { return nvs_set_u16(h, k, v); }
        };
        template <>
        struct Io<int16_t>
        {
            static esp_err_t get(nvs_handle_t h, const char *k, int16_t *v) { return nvs_get_i16(h, k, v); }
            static esp_err_t set(nvs_handle_t h, const char *k, int16_t v) { return nvs_set_i16(h, k, v); }
        };
        template <>
        struct Io<uint32_t>
        {
            static esp_err_t get(nvs_handle_t h, const char *k, uint32_t *v) { return nvs_get_u32(h, k, v); }
            static esp_err_t set(nvs_handle_t h, const char *k, uint32_t v) { return nvs_set_u32(h, k, v); }
        };

        /**
         * @brief Copies one legacy value to its new name unless the new name already holds a value.
         * @return true when a value was copied.
         */
        template <typename T>
        bool copyOne(nvs_handle_t legacyHandle, const legacy::Rename &r)
        {
            T value;
            if (Io<T>::get(legacyHandle, r.oldKey, &value) != ESP_OK)
                return false;
            nvs_handle_t h;
            if (nvs_open(r.newNs, NVS_READWRITE, &h) != ESP_OK)
                return false;
            T existing;
            bool copied = false;
            if (Io<T>::get(h, r.newKey, &existing) == ESP_ERR_NVS_NOT_FOUND && Io<T>::set(h, r.newKey, value) == ESP_OK &&
                nvs_commit(h) == ESP_OK)
            {
                ESP_LOGI(LOG_TAG_NVS, "Migrated %s/%s -> %s/%s", legacy::NS, r.oldKey, r.newNs, r.newKey);
                copied = true;
            }
            nvs_close(h);
            return copied;
        }
    }

    /**
     * @brief Copies every setting still stored under its legacy name (namespace "storage") to its vx_* name.
     *
     * A value is copied only when the new name is empty, so the call is cheap and harmless on every boot, and a value
     * set under the new name is never overwritten. Legacy entries are left in place so an older image (after an OTA
     * rollback, or an older factory app) still finds them.
     *
     * @return Number of values copied.
     */
    inline int migrateLegacyKeys()
    {
        nvs_handle_t legacyHandle;
        if (nvs_open(legacy::NS, NVS_READONLY, &legacyHandle) != ESP_OK)
            return 0; // Nothing was ever stored under the old layout
        int copied = 0;
        for (const legacy::Rename &r : legacy::RENAMES)
        {
            bool done = false;
            switch (r.type)
            {
            case legacy::Type::U8:
                done = detail::copyOne<uint8_t>(legacyHandle, r);
                break;
            case legacy::Type::I8:
                done = detail::copyOne<int8_t>(legacyHandle, r);
                break;
            case legacy::Type::U16:
                done = detail::copyOne<uint16_t>(legacyHandle, r);
                break;
            case legacy::Type::I16:
                done = detail::copyOne<int16_t>(legacyHandle, r);
                break;
            case legacy::Type::U32:
                done = detail::copyOne<uint32_t>(legacyHandle, r);
                break;
            }
            if (done)
                copied++;
        }
        nvs_close(legacyHandle);
        if (copied > 0)
            ESP_LOGW(detail::LOG_TAG_NVS, "Copied %d setting(s) from the legacy NVS layout", copied);
        return copied;
    }

    /**
     * @brief Reads one setting, writing the default when it is missing or out of range.
     * @tparam T Stored integer type
     * @param h Open read-write handle on the setting's namespace
     * @param key NVS key
     * @param[in,out] value Default on entry, stored value on return
     * @param valid Range check of the stored value, nullptr to accept any value
     * @return ESP_OK, or the error of the failed write or commit (value then holds the default)
     */
    template <typename T>
    esp_err_t loadOrInit(nvs_handle_t h, const char *key, T &value, bool (*valid)(T) = nullptr)
    {
        const T def = value;
        esp_err_t err = detail::Io<T>::get(h, key, &value);
        if (err == ESP_OK && (valid == nullptr || valid(value)))
            return ESP_OK;
        if (err == ESP_OK)
            ESP_LOGW(detail::LOG_TAG_NVS, "%s = %ld out of range, resetting to %ld", key, (long)value, (long)def);
        else
            ESP_LOGW(detail::LOG_TAG_NVS, "%s not found (%s), initialising to %ld", key, esp_err_to_name(err), (long)def);
        value = def;
        err = detail::Io<T>::set(h, key, value);
        if (err == ESP_OK)
            err = nvs_commit(h);
        return err;
    }

    /**
     * @brief Records the running app slot for the factory app, when it changed.
     *
     * Also written under the legacy name so a factory app built before the vx_* layout still returns to the right
     * slot. Drop the legacy write once every unit runs a current factory app.
     *
     * @param slot -1 factory, 0 ota_0, 1 ota_1
     * @return ESP_OK, or the first NVS error
     */
    inline esp_err_t recordBootSlot(int8_t slot)
    {
        nvs_handle_t h;
        esp_err_t err = nvs_open(sys::NS, NVS_READWRITE, &h);
        if (err != ESP_OK)
            return err;
        int8_t saved = -2;
        if (nvs_get_i8(h, sys::BOOT_SLOT, &saved) != ESP_OK || saved != slot)
        {
            err = nvs_set_i8(h, sys::BOOT_SLOT, slot);
            if (err == ESP_OK)
                err = nvs_commit(h);
        }
        nvs_close(h);
        if (err != ESP_OK)
            return err;

        if (nvs_open(legacy::NS, NVS_READWRITE, &h) == ESP_OK)
        {
            saved = -2;
            if (nvs_get_i8(h, "lastPart", &saved) != ESP_OK || saved != slot)
            {
                if (nvs_set_i8(h, "lastPart", slot) == ESP_OK)
                    nvs_commit(h);
            }
            nvs_close(h);
        }
        return ESP_OK;
    }

    /**
     * @brief Reads the app slot the main app last booted from.
     * @param[out] slot -1 factory, 0 ota_0, 1 ota_1; left unchanged when nothing is stored
     * @return ESP_OK, or the NVS error when neither the new nor the legacy record could be read
     */
    inline esp_err_t readBootSlot(int8_t &slot)
    {
        nvs_handle_t h;
        esp_err_t err = nvs_open(sys::NS, NVS_READONLY, &h);
        if (err == ESP_OK)
        {
            err = nvs_get_i8(h, sys::BOOT_SLOT, &slot);
            nvs_close(h);
            if (err == ESP_OK)
                return ESP_OK;
        }
        // Main app built before the vx_* layout
        if (nvs_open(legacy::NS, NVS_READONLY, &h) == ESP_OK)
        {
            err = nvs_get_i8(h, "lastPart", &slot);
            nvs_close(h);
        }
        return err;
    }
}
