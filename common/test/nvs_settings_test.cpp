// Host unit test for common/nvs_keys.hpp and common/nvs_settings.hpp, over an in-memory NVS stub.
// Build and run from the repository root:
//   g++ -std=c++17 -Wall -Wextra -Werror -I common -I common/test/stubs common/test/nvs_settings_test.cpp -o /tmp/nvs_test && /tmp/nvs_test

#include "nvs_settings.hpp"

#include <cstdio>
#include <cstring>
#include <set>
#include <string>

static int failures = 0;

#define CHECK(cond)                                                     \
    do                                                                  \
    {                                                                   \
        if (!(cond))                                                    \
        {                                                               \
            std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
            failures++;                                                 \
        }                                                               \
    } while (0)

using fake_nvs::store;

static void put(const char *ns, const char *key, char type, int64_t v) { store[{ns, key}] = {type, v}; }
static bool has(const char *ns, const char *key) { return store.count({ns, key}) == 1; }
static int64_t val(const char *ns, const char *key) { return store.at({ns, key}).value; }

static void test_names_unique()
{
    std::set<std::string> oldKeys, newNames;
    for (const auto &r : vx::nvs::legacy::RENAMES)
    {
        CHECK(oldKeys.insert(r.oldKey).second);
        CHECK(newNames.insert(std::string(r.newNs) + "/" + r.newKey).second);
        CHECK(std::strcmp(r.newNs, vx::nvs::legacy::NS) != 0);
    }
    CHECK(!vx::nvs::fits(""));
    CHECK(vx::nvs::fits("123456789012345"));
    CHECK(!vx::nvs::fits("1234567890123456"));
}

static void test_migration()
{
    store.clear();
    put("storage", "fuel_full_r", 'c', 263);
    put("storage", "r39_b_ckph", 'd', -5);
    put("storage", "shft_mid", 'e', 5400);
    put("storage", "lastPart", 'b', 1);
    put("vx_r39", "b_ckph", 'd', 150); // already set under the new name: must win
    put("nvs_odo_dummy", "x", 'a', 0);

    CHECK(vx::nvs::migrateLegacyKeys() == 3);
    CHECK(val("vx_fuel", "full_ohm") == 263);
    CHECK(val("vx_disp", "shift_mid_rpm") == 5400);
    CHECK(val("vx_sys", "boot_slot") == 1);
    CHECK(val("vx_r39", "b_ckph") == 150);
    CHECK(has("storage", "fuel_full_r")); // legacy entries are kept
    CHECK(!has("vx_fuel", "learn_en"));   // nothing invented

    CHECK(vx::nvs::migrateLegacyKeys() == 0); // second boot: nothing to do
}

static void test_no_legacy()
{
    store.clear();
    CHECK(vx::nvs::migrateLegacyKeys() == 0);
    CHECK(store.empty());
}

static void test_load_or_init()
{
    store.clear();
    nvs_handle_t h;
    nvs_open("vx_r39", NVS_READWRITE, &h);
    uint16_t a = 400;
    CHECK(vx::nvs::loadOrInit<uint16_t>(h, "a_bp", a, [](uint16_t v) { return v <= 1000; }) == ESP_OK);
    CHECK(a == 400 && val("vx_r39", "a_bp") == 400); // missing: default written

    put("vx_r39", "a_bp", 'c', 700);
    a = 400;
    vx::nvs::loadOrInit<uint16_t>(h, "a_bp", a, [](uint16_t v) { return v <= 1000; });
    CHECK(a == 700); // stored and valid

    put("vx_r39", "a_bp", 'c', 5000);
    a = 400;
    vx::nvs::loadOrInit<uint16_t>(h, "a_bp", a, [](uint16_t v) { return v <= 1000; });
    CHECK(a == 400 && val("vx_r39", "a_bp") == 400); // out of range: reset
    nvs_close(h);
}

static void test_boot_slot()
{
    store.clear();
    int8_t slot = 5;
    CHECK(vx::nvs::readBootSlot(slot) != ESP_OK && slot == 5);

    put("storage", "lastPart", 'b', 0); // written by an older main app
    CHECK(vx::nvs::readBootSlot(slot) == ESP_OK && slot == 0);

    CHECK(vx::nvs::recordBootSlot(1) == ESP_OK);
    CHECK(val("vx_sys", "boot_slot") == 1);
    CHECK(val("storage", "lastPart") == 1); // kept in step for older factory apps
    CHECK(vx::nvs::readBootSlot(slot) == ESP_OK && slot == 1);
}

int main()
{
    test_names_unique();
    test_migration();
    test_no_legacy();
    test_load_or_init();
    test_boot_slot();
    if (failures == 0)
        std::printf("All NVS settings tests passed\n");
    return failures == 0 ? 0 : 1;
}
