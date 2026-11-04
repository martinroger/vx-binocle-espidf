// Host stub of the ESP-IDF NVS API over an in-memory store, for common/test only.
#pragma once
#include <cstdint>
#include <map>
#include <string>
#include <utility>
#include "esp_err.h"

typedef uint32_t nvs_handle_t;
typedef enum { NVS_READONLY, NVS_READWRITE } nvs_open_mode_t;

namespace fake_nvs
{
    struct Entry { char type; int64_t value; };
    inline std::map<std::pair<std::string, std::string>, Entry> store;
    inline std::map<nvs_handle_t, std::string> handles;
    inline nvs_handle_t next = 1;
    inline bool nsExists(const std::string &ns)
    {
        for (auto &kv : store)
            if (kv.first.first == ns) return true;
        return false;
    }
    template <typename T>
    esp_err_t get(nvs_handle_t h, const char *k, T *v, char type)
    {
        auto it = store.find({handles[h], k});
        if (it == store.end()) return ESP_ERR_NVS_NOT_FOUND;
        if (it->second.type != type) return ESP_ERR_NVS_TYPE_MISMATCH;
        *v = (T)it->second.value;
        return ESP_OK;
    }
    template <typename T>
    esp_err_t set(nvs_handle_t h, const char *k, T v, char type)
    {
        store[{handles[h], k}] = {type, (int64_t)v};
        return ESP_OK;
    }
}

inline esp_err_t nvs_open(const char *ns, nvs_open_mode_t mode, nvs_handle_t *h)
{
    if (mode == NVS_READONLY && !fake_nvs::nsExists(ns)) return ESP_ERR_NVS_NOT_FOUND;
    *h = fake_nvs::next++;
    fake_nvs::handles[*h] = ns;
    return ESP_OK;
}
inline void nvs_close(nvs_handle_t h) { fake_nvs::handles.erase(h); }
inline esp_err_t nvs_commit(nvs_handle_t) { return ESP_OK; }
#define FAKE_NVS_IO(sfx, T, c) \
    inline esp_err_t nvs_get_##sfx(nvs_handle_t h, const char *k, T *v) { return fake_nvs::get(h, k, v, c); } \
    inline esp_err_t nvs_set_##sfx(nvs_handle_t h, const char *k, T v) { return fake_nvs::set(h, k, v, c); }
FAKE_NVS_IO(u8, uint8_t, 'a')
FAKE_NVS_IO(i8, int8_t, 'b')
FAKE_NVS_IO(u16, uint16_t, 'c')
FAKE_NVS_IO(i16, int16_t, 'd')
FAKE_NVS_IO(u32, uint32_t, 'e')
