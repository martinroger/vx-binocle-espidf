#pragma once
#include <stdio.h>
#include <string.h>
#include "esp_app_desc.h"
#include <stdlib.h>
#include "esp_log.h"

#ifdef TAG
#undef TAG
#endif
#define TAG "VERSION_PARSER"



typedef struct parsed_app_meta_t
{
    // Pointer to the raw commit id bytes (NOT null-terminated). Use commit_len for length.
    char *commitID;
    size_t commit_len;
    // Pointer to base version characters (NOT null-terminated). Use base_version_len for length.
    char *base_version;
    size_t base_version_len;
    bool is_dirty;
} parsed_app_meta_t;


/**
 * @brief Parse application metadata version string into components.
 *
 * Takes a pointer to a parsed_app_meta_t struct and fills it with parsed data.
 * The struct may contain heap-allocated buffers for `commitID` and `base_version` (not null-terminated).
 * The caller is responsible for freeing these buffers using parsed_app_meta_free when no longer needed.
 *
 * Behavior:
 * - commitID: the short SHA without leading 'g' and WITHOUT a trailing NUL; length in commit_len.
 * - base_version: characters from full_version starting at index 1 up to (but not including)
 *   the first '-' character; length in base_version_len. This omits the first character of full_version.
 * - is_dirty: true if the version contains "-dirty" after the gSHA.
 *
 * @return ESP_OK on success, ESP_FAIL on failure.
 */
esp_err_t parse_app_metadata(parsed_app_meta_t *out)
{
    if (!out)
        return ESP_FAIL;

    const esp_app_desc_t *app_metadata;

    // Initialize output struct
    out->commitID = NULL;
    out->commit_len = 0;
    out->base_version = NULL;
    out->base_version_len = 0;
    out->is_dirty = false;

    app_metadata = esp_app_get_description();
    if (!app_metadata)
    {
        ESP_LOGE(TAG, "No app metadata available");
        return ESP_FAIL;
    }

    const char *full_version = app_metadata->version;

    // 1) Extract base_version: all characters from full_version[1] up to the first hyphen (exclusive).
    const char *first_hyphen = strchr(full_version, '-');
    size_t base_len = 0;
    if (first_hyphen)
    {
        // number of chars before first hyphen
        size_t prefix_len = (size_t)(first_hyphen - full_version);
        if (prefix_len > 0)
        {
            // omit the first character
            if (prefix_len > 1)
            {
                base_len = prefix_len - 1;
                out->base_version = (char *)malloc(base_len);
                if (out->base_version)
                    memcpy(out->base_version, full_version + 1, base_len);
                out->base_version_len = base_len;
            }
        }
    }
    else
    {
        // No hyphen found: take whole string except first character
        size_t full_len = strlen(full_version);
        if (full_len > 1)
        {
            base_len = full_len - 1;
            out->base_version = (char *)malloc(base_len);
            if (out->base_version)
                memcpy(out->base_version, full_version + 1, base_len);
            out->base_version_len = base_len;
        }
    }

    // 2) Find the '-g' marker for git info
    const char *g_marker = strstr(full_version, "-g");
    if (!g_marker)
    {
        // not found: still return base_version information
        ESP_LOGW(TAG, "Could not find Git gSHA prefix ('-g') in version string: %s", full_version);
        return ESP_OK;
    }

    // Pointer to the 'g' (we want to skip the leading 'g' later)
    const char *g_start = g_marker + 1; // points to 'g'

    // Check for dirty suffix
    const char *dirty_suffix = strstr(g_start, "-dirty");
    out->is_dirty = (dirty_suffix != NULL);

    // Determine end of SHA (either the '-' before 'dirty' or end of string or next '-')
    const char *sha_end = dirty_suffix ? dirty_suffix : strchr(g_start, '-');
    if (!sha_end)
        sha_end = full_version + strlen(full_version);

    // g_start points at 'g'; commit bytes are from g_start+1 up to sha_end (exclusive)
    const char *commit_start = g_start + 1; // skip leading 'g'
    if (commit_start < sha_end)
    {
        size_t clen = (size_t)(sha_end - commit_start);
        out->commitID = (char *)malloc(clen); // intentionally not null-terminated
        if (out->commitID)
            memcpy(out->commitID, commit_start, clen);
        out->commit_len = clen;
    }

    // Log for debug (print safely by making temporary NUL-terminated copies)
    if (out->commitID && out->commit_len > 0)
    {
        char *tmp = (char *)malloc(out->commit_len + 1);
        if (tmp)
        {
            memcpy(tmp, out->commitID, out->commit_len);
            tmp[out->commit_len] = '\0';
            ESP_LOGI(TAG, "Stripped Short SHA: %s", tmp);
            free(tmp);
        }
    }

    if (out->base_version && out->base_version_len > 0)
    {
        char *tmp = (char *)malloc(out->base_version_len + 1);
        if (tmp)
        {
            memcpy(tmp, out->base_version, out->base_version_len);
            tmp[out->base_version_len] = '\0';
            ESP_LOGI(TAG, "Base version (stripped first char): %s", tmp);
            free(tmp);
        }
    }

    ESP_LOGI(TAG, "is_dirty: %s", out->is_dirty ? "true" : "false");

    return ESP_OK;
}

// Helper to free buffers allocated by parse_app_metadata
static inline void parsed_app_meta_free(parsed_app_meta_t *m)
{
    if (m == NULL)
        return;
    if (m->commitID)
    {
        free(m->commitID);
        m->commitID = NULL;
        m->commit_len = 0;
    }
    if (m->base_version)
    {
        free(m->base_version);
        m->base_version = NULL;
        m->base_version_len = 0;
    }
    m->is_dirty = false;
}