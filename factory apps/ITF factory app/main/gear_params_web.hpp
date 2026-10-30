/**
 * @file gear_params_web.hpp
 * @brief REST endpoints to read, edit and store the vx-gear-estimator parameters.
 *
 * The parameters live in the default NVS partition (namespace "vx_gear"), written and read only through the
 * vx-gear-estimator API, so the interface board firmware picks them up on its next boot.
 *
 * Endpoints:
 * - GET  /api/gear          : stored parameters, their source and the field table (names, groups, ranges).
 * - GET  /api/gear/defaults : tuned defaults compiled into the component, nothing stored.
 * - POST /api/gear          : {"params":{...}}, range-checked, validated with gear_params_valid(), then stored.
 * - POST /api/gear/erase    : deletes the stored copy; the next load re-initialises it from the tuned defaults.
 */

#pragma once

#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include "cJSON.h"
#include "esp_err.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "gear_estimator.h"
#include "gear_params_nvs.h"

/** Maximum accepted POST /api/gear body, in bytes. */
#define GEAR_WEB_MAX_BODY (4096)

/** Kind of value a field holds. */
typedef enum
{
	GEAR_FIELD_FLOAT = 0, /**< One float, or GEAR_NUM_FORWARD floats when count > 1. */
	GEAR_FIELD_GATE_MODE, /**< gear_gate_mode_t. */
} gear_field_kind_t;

/** One editable field of gear_params_t, with the range enforced by gear_params_valid(). */
typedef struct
{
	const char *name;  /**< JSON key, same as the struct member. */
	const char *group; /**< UI group. */
	size_t offset;     /**< offsetof(gear_params_t, member). */
	uint8_t count;     /**< 1, or GEAR_NUM_FORWARD for the per-gear arrays. */
	gear_field_kind_t kind;
	double min; /**< Kept as double so the JSON shows the exact literal; compared as float like gear_params_valid(). */
	double max;
	const char *unit;
} gear_field_t;

/**
 * @brief Field table, kept in step with gear_params_valid() in vx-gear-estimator.
 *
 * Drives the JSON encoder, the decoder and the web form. gear_params_valid() is still called before saving, so a
 * drift between this table and the component can never store an invalid block.
 */
static const gear_field_t GEAR_FIELDS[] = {
	{"ratio_mean", "Ratio model", offsetof(gear_params_t, ratio_mean), GEAR_NUM_FORWARD, GEAR_FIELD_FLOAT, 0.1, 20.0, "speed Hz / RPM Hz"},
	{"ratio_var", "Ratio model", offsetof(gear_params_t, ratio_var), GEAR_NUM_FORWARD, GEAR_FIELD_FLOAT, 1e-5, 10.0, ""},
	{"min_speed_hz", "Gating", offsetof(gear_params_t, min_speed_hz), 1, GEAR_FIELD_FLOAT, 0.0, 1000.0, "Hz"},
	{"min_rpm_hz", "Gating", offsetof(gear_params_t, min_rpm_hz), 1, GEAR_FIELD_FLOAT, 0.0, 1000.0, "Hz"},
	{"gate_mode", "Gating", offsetof(gear_params_t, gate_mode), 1, GEAR_FIELD_GATE_MODE, 0.0, 1.0, ""},
	{"decay", "Filter", offsetof(gear_params_t, decay), 1, GEAR_FIELD_FLOAT, 0.5, 0.999, ""},
	{"inertia", "Filter", offsetof(gear_params_t, inertia), 1, GEAR_FIELD_FLOAT, 0.3, 0.999, ""},
	{"p0", "Filter", offsetof(gear_params_t, p0), 1, GEAR_FIELD_FLOAT, 1e-4, 1.0, ""},
	{"conf_thresh", "Filter", offsetof(gear_params_t, conf_thresh), 1, GEAR_FIELD_FLOAT, 0.05, 0.95, ""},
	{"latch_ms", "Filter", offsetof(gear_params_t, latch_ms), 1, GEAR_FIELD_FLOAT, 0.0, 2000.0, "ms"},
	{"trans_neutral_stay", "Transitions", offsetof(gear_params_t, trans_neutral_stay), 1, GEAR_FIELD_FLOAT, 0.0, 1.0, ""},
	{"trans_neutral_exit", "Transitions", offsetof(gear_params_t, trans_neutral_exit), 1, GEAR_FIELD_FLOAT, 0.0, 1.0, ""},
	{"trans_floor", "Transitions", offsetof(gear_params_t, trans_floor), 1, GEAR_FIELD_FLOAT, 0.0, 1.0, ""},
	{"trans_shift_likely", "Transitions", offsetof(gear_params_t, trans_shift_likely), 1, GEAR_FIELD_FLOAT, 0.0, 1.0, ""},
	{"trans_shift_blocked", "Transitions", offsetof(gear_params_t, trans_shift_blocked), 1, GEAR_FIELD_FLOAT, 0.0, 1.0, ""},
	{"trans_shift_neutral", "Transitions", offsetof(gear_params_t, trans_shift_neutral), 1, GEAR_FIELD_FLOAT, 0.0, 1.0, ""},
	{"accel_rpm_min_hz", "Transitions", offsetof(gear_params_t, accel_rpm_min_hz), 1, GEAR_FIELD_FLOAT, 0.0, 1000.0, "Hz"},
	{"decel_speed_rate", "Transitions", offsetof(gear_params_t, decel_speed_rate), 1, GEAR_FIELD_FLOAT, -1000.0, 0.0, "Hz/s"},
	{"clutch_rpm_rate", "Clutch drop", offsetof(gear_params_t, clutch_rpm_rate), 1, GEAR_FIELD_FLOAT, -1000.0, 0.0, "Hz/s"},
	{"clutch_speed_rate_min", "Clutch drop", offsetof(gear_params_t, clutch_speed_rate_min), 1, GEAR_FIELD_FLOAT, -1000.0, 1000.0, "Hz/s"},
	{"clutch_neutral_boost", "Clutch drop", offsetof(gear_params_t, clutch_neutral_boost), 1, GEAR_FIELD_FLOAT, 1.0, 100.0, "x"},
	{"clutch_density_floor", "Clutch drop", offsetof(gear_params_t, clutch_density_floor), 1, GEAR_FIELD_FLOAT, 0.0, 1.0, "x"},
};

#define GEAR_FIELD_COUNT (sizeof(GEAR_FIELDS) / sizeof(GEAR_FIELDS[0]))

/**
 * @brief Checks @p v against the field range, in float precision like gear_params_valid().
 * @param[in] f Field.
 * @param[in] v Value from the JSON body.
 * @return true when in range.
 */
static bool gear_field_in_range(const gear_field_t *f, double v)
{
	float x = (float)v;
	return x >= (float)f->min && x <= (float)f->max;
}

/**
 * @brief Adds every field of @p p to @p obj as numbers (arrays for the per-gear fields).
 * @param[in,out] obj JSON object to fill.
 * @param[in] p Parameter block.
 */
static void gear_params_to_json(cJSON *obj, const gear_params_t *p)
{
	const uint8_t *base = (const uint8_t *)p;
	for (size_t i = 0; i < GEAR_FIELD_COUNT; i++)
	{
		const gear_field_t *f = &GEAR_FIELDS[i];
		if (f->kind == GEAR_FIELD_GATE_MODE)
		{
			cJSON_AddNumberToObject(obj, f->name, (double)p->gate_mode);
		}
		else if (f->count > 1)
		{
			const float *v = (const float *)(base + f->offset);
			cJSON *arr = cJSON_AddArrayToObject(obj, f->name);
			for (uint8_t k = 0; k < f->count; k++)
				cJSON_AddItemToArray(arr, cJSON_CreateNumber((double)v[k]));
		}
		else
		{
			cJSON_AddNumberToObject(obj, f->name, (double)*(const float *)(base + f->offset));
		}
	}
}

/**
 * @brief Overlays the fields present in @p obj onto @p p, checking each value against the field table.
 *
 * Missing fields keep the value already in @p p.
 * @param[in] obj JSON object holding the fields.
 * @param[in,out] p Parameter block, pre-filled with the current values.
 * @param[out] err Buffer for a human-readable error.
 * @param[in] err_len Size of @p err.
 * @return true when every present field was a number in range.
 */
static bool gear_params_from_json(const cJSON *obj, gear_params_t *p, char *err, size_t err_len)
{
	uint8_t *base = (uint8_t *)p;
	for (size_t i = 0; i < GEAR_FIELD_COUNT; i++)
	{
		const gear_field_t *f = &GEAR_FIELDS[i];
		const cJSON *item = cJSON_GetObjectItemCaseSensitive(obj, f->name);
		if (item == NULL)
			continue;

		if (f->count > 1)
		{
			if (!cJSON_IsArray(item) || cJSON_GetArraySize(item) != f->count)
			{
				snprintf(err, err_len, "%s must be an array of %u numbers", f->name, f->count);
				return false;
			}
			float *v = (float *)(base + f->offset);
			for (uint8_t k = 0; k < f->count; k++)
			{
				const cJSON *e = cJSON_GetArrayItem(item, k);
				if (!cJSON_IsNumber(e) || !gear_field_in_range(f, e->valuedouble))
				{
					snprintf(err, err_len, "%s[%u] must be a number in %g..%g", f->name, k, f->min, f->max);
					return false;
				}
				v[k] = (float)e->valuedouble;
			}
			continue;
		}

		if (!cJSON_IsNumber(item) || !gear_field_in_range(f, item->valuedouble))
		{
			snprintf(err, err_len, "%s must be a number in %g..%g", f->name, f->min, f->max);
			return false;
		}
		if (f->kind == GEAR_FIELD_GATE_MODE)
		{
			if (item->valuedouble != (double)GEAR_GATE_NEUTRAL_ON_ANY && item->valuedouble != (double)GEAR_GATE_UNCERTAIN_ABOVE_SPEED_CUTOFF)
			{
				snprintf(err, err_len, "%s must be 0 or 1", f->name);
				return false;
			}
			p->gate_mode = (gear_gate_mode_t)item->valueint;
		}
		else
			*(float *)(base + f->offset) = (float)item->valuedouble;
	}
	return true;
}

/**
 * @brief Sends @p root as a JSON response and frees it.
 * @param[in] req HTTP request.
 * @param[in] root JSON document, freed here.
 * @return ESP_OK, or ESP_FAIL when it could not be printed.
 */
static esp_err_t gear_send_json(httpd_req_t *req, cJSON *root)
{
	char *out = cJSON_PrintUnformatted(root);
	cJSON_Delete(root);
	if (out == NULL)
	{
		httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Out of memory");
		return ESP_FAIL;
	}
	httpd_resp_set_type(req, "application/json");
	esp_err_t err = httpd_resp_sendstr(req, out);
	free(out);
	return err;
}

/**
 * @brief GET /api/gear: stored parameters, their source and the field table.
 *
 * Side effects: when nothing valid is stored, gear_params_nvs_load() writes the tuned defaults.
 * @param[in] req HTTP request.
 * @return ESP_OK or ESP_FAIL.
 */
static esp_err_t gear_get_handler(httpd_req_t *req)
{
	gear_params_t p;
	gear_params_source_t src;
	esp_err_t nvs_err = gear_params_nvs_load(&p, &src);

	cJSON *root = cJSON_CreateObject();
	cJSON_AddStringToObject(root, "source", (src == GEAR_PARAMS_FROM_NVS) ? "nvs" : (src == GEAR_PARAMS_FROM_DEFAULT) ? "default" : "default_unsaved");
	if (nvs_err != ESP_OK)
		cJSON_AddStringToObject(root, "nvs_error", esp_err_to_name(nvs_err));
	gear_params_to_json(cJSON_AddObjectToObject(root, "params"), &p);

	cJSON *fields = cJSON_AddArrayToObject(root, "fields");
	for (size_t i = 0; i < GEAR_FIELD_COUNT; i++)
	{
		const gear_field_t *f = &GEAR_FIELDS[i];
		cJSON *o = cJSON_CreateObject();
		cJSON_AddStringToObject(o, "name", f->name);
		cJSON_AddStringToObject(o, "group", f->group);
		cJSON_AddNumberToObject(o, "count", f->count);
		cJSON_AddStringToObject(o, "kind", (f->kind == GEAR_FIELD_GATE_MODE) ? "gate_mode" : "float");
		cJSON_AddNumberToObject(o, "min", f->min);
		cJSON_AddNumberToObject(o, "max", f->max);
		cJSON_AddStringToObject(o, "unit", f->unit);
		cJSON_AddItemToArray(fields, o);
	}
	ESP_LOGI(__func__, "Gear parameters served (source %d)", (int)src);
	return gear_send_json(req, root);
}

/**
 * @brief GET /api/gear/defaults: the tuned defaults compiled into vx-gear-estimator. Nothing is stored.
 * @param[in] req HTTP request.
 * @return ESP_OK or ESP_FAIL.
 */
static esp_err_t gear_defaults_get_handler(httpd_req_t *req)
{
	gear_params_t p;
	gear_params_default_tuned(&p);
	cJSON *root = cJSON_CreateObject();
	gear_params_to_json(cJSON_AddObjectToObject(root, "params"), &p);
	return gear_send_json(req, root);
}

/**
 * @brief POST /api/gear: body {"params":{...}}; checks, validates and stores the block.
 *
 * Fields missing from the body keep their stored value. Replies 200, 400 with a message (bad JSON, out of range
 * or rejected by gear_params_valid()) or 500 (NVS error). Nothing is stored unless the whole block is valid.
 * @param[in] req HTTP request.
 * @return ESP_OK or ESP_FAIL.
 */
static esp_err_t gear_post_handler(httpd_req_t *req)
{
	if (req->content_len <= 0 || req->content_len > GEAR_WEB_MAX_BODY)
	{
		httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing or oversized body");
		return ESP_FAIL;
	}
	char *body = (char *)malloc(req->content_len + 1);
	if (body == NULL)
	{
		httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Out of memory");
		return ESP_FAIL;
	}
	int received = 0;
	while (received < (int)req->content_len)
	{
		int r = httpd_req_recv(req, body + received, req->content_len - received);
		if (r <= 0)
		{
			free(body);
			httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Body read failed");
			return ESP_FAIL;
		}
		received += r;
	}
	body[received] = '\0';

	cJSON *root = cJSON_Parse(body);
	free(body);
	const cJSON *params = cJSON_GetObjectItemCaseSensitive(root, "params");
	if (!cJSON_IsObject(params))
	{
		cJSON_Delete(root);
		httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Expected {\"params\":{...}}");
		return ESP_FAIL;
	}

	gear_params_t p;
	gear_params_nvs_load(&p, NULL); // current values, so missing fields are kept
	char err_msg[96] = {0};
	bool ok = gear_params_from_json(params, &p, err_msg, sizeof(err_msg));
	cJSON_Delete(root);
	if (!ok)
	{
		ESP_LOGW(__func__, "Rejected gear parameters: %s", err_msg);
		httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, err_msg);
		return ESP_FAIL;
	}
	if (!gear_params_valid(&p))
	{
		ESP_LOGW(__func__, "Rejected gear parameters: gear_params_valid() failed");
		httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Rejected by gear_params_valid() (ratio_mean must increase from gear 1 to 5)");
		return ESP_FAIL;
	}

	esp_err_t err = gear_params_nvs_save(&p);
	if (err != ESP_OK)
	{
		ESP_LOGE(__func__, "Could not store gear parameters: %s", esp_err_to_name(err));
		httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, esp_err_to_name(err));
		return ESP_FAIL;
	}
	ESP_LOGI(__func__, "Gear parameters stored in NVS");
	httpd_resp_sendstr(req, "OK");
	return ESP_OK;
}

/**
 * @brief POST /api/gear/erase: deletes the stored copy.
 * @param[in] req HTTP request.
 * @return ESP_OK or ESP_FAIL.
 */
static esp_err_t gear_erase_post_handler(httpd_req_t *req)
{
	esp_err_t err = gear_params_nvs_erase();
	if (err != ESP_OK)
	{
		ESP_LOGE(__func__, "Could not erase gear parameters: %s", esp_err_to_name(err));
		httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, esp_err_to_name(err));
		return ESP_FAIL;
	}
	ESP_LOGI(__func__, "Stored gear parameters erased");
	httpd_resp_sendstr(req, "OK");
	return ESP_OK;
}

/**
 * @brief Registers the four /api/gear handlers.
 * @param[in] server Running HTTP server. Needs 4 free URI handler slots.
 * @return ESP_OK, or the first registration error.
 */
static esp_err_t gear_params_web_register(httpd_handle_t server)
{
	static const struct
	{
		const char *uri;
		httpd_method_t method;
		esp_err_t (*handler)(httpd_req_t *r);
	} routes[] = {
		{"/api/gear", HTTP_GET, gear_get_handler},
		{"/api/gear/defaults", HTTP_GET, gear_defaults_get_handler},
		{"/api/gear", HTTP_POST, gear_post_handler},
		{"/api/gear/erase", HTTP_POST, gear_erase_post_handler},
	};
	for (size_t i = 0; i < sizeof(routes) / sizeof(routes[0]); i++)
	{
		httpd_uri_t u = {};
		u.uri = routes[i].uri;
		u.method = routes[i].method;
		u.handler = routes[i].handler;
		esp_err_t err = httpd_register_uri_handler(server, &u);
		if (err != ESP_OK)
		{
			ESP_LOGE(__func__, "Could not register %s: %s", u.uri, esp_err_to_name(err));
			return err;
		}
	}
	return ESP_OK;
}
