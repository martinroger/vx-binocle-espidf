#include "nvs_storage.h"

esp_err_t nvs_init_odo_flash(void)
{
    esp_err_t nvs_err = nvs_flash_init_partition("nvs_odo");
    if (nvs_err == ESP_ERR_NVS_NO_FREE_PAGES || nvs_err == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        esp_err_t e = nvs_flash_erase_partition("nvs_odo");
        if (e != ESP_OK)
            return e;
        nvs_err = nvs_flash_init_partition("nvs_odo");
    }
    return nvs_err;
}

esp_err_t nvs_get_u32_optional(const char *ns, const char *key, uint32_t *out_val, bool *found)
{
    nvs_handle_t h;
    esp_err_t ret = nvs_open_from_partition("nvs_odo", ns, NVS_READONLY, &h);
    if (ret != ESP_OK)
        return ret;
    esp_err_t g = nvs_get_u32(h, key, out_val);
    nvs_close(h);
    if (g == ESP_OK)
    {
        if (found)
            *found = true;
        return ESP_OK;
    }
    else if (g == ESP_ERR_NVS_NOT_FOUND)
    {
        if (found)
            *found = false;
        return ESP_OK;
    }
    return g;
}

esp_err_t nvs_set_u32_value(const char *ns, const char *key, uint32_t val)
{
    nvs_handle_t h;
    esp_err_t ret = nvs_open_from_partition("nvs_odo", ns, NVS_READWRITE, &h);
    if (ret != ESP_OK)
        return ret;
    ret = nvs_set_u32(h, key, val);
    if (ret == ESP_OK)
        ret = nvs_commit(h);
    nvs_close(h);
    return ret;
}

esp_err_t nvs_update_u32_if_different(const char *ns, const char *key, uint32_t new_val, bool *updated_out)
{
    nvs_handle_t h;
    esp_err_t ret = nvs_open_from_partition("nvs_odo", ns, NVS_READWRITE, &h);
    if (ret != ESP_OK)
        return ret;

    uint32_t cur = 0;
    esp_err_t g = nvs_get_u32(h, key, &cur);
    if (g == ESP_OK)
    {
        if (cur == new_val)
        {
            if (updated_out)
                *updated_out = false;
            nvs_close(h);
            return ESP_OK; // nothing to do
        }
    }
    // either not found or different -> write
    ret = nvs_set_u32(h, key, new_val);
    if (ret == ESP_OK)
        ret = nvs_commit(h);
    if (updated_out)
        *updated_out = (ret == ESP_OK);
    nvs_close(h);
    return ret;
}
