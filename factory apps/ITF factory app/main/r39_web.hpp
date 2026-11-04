/**
 * @file r39_web.hpp
 * @brief REST endpoints to read and store the ECE R39 speed compliance settings, and the live compliant speed.
 *
 * The settings live in the default NVS partition, namespace "vx_r39" (keys from common/nvs_keys.hpp), beside
 * the other calibration namespaces. The interface board reads them, and rewrites any missing or out-of-range key
 * with its default, on its next boot.
 *
 * Endpoints:
 * - GET  /api/r39 : stored values (or defaults when missing, nothing written), limits and a preview of the curve.
 * - POST /api/r39 : {"enabled":bool,"a_pct":number,"b_kph":number}, any field optional. Range-checked, stored,
 *                   committed; replies with the same body as GET.
 *
 * r39_live_params() gives the stored settings to the WebSocket telemetry for the live readout.
 */

#pragma once

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include "cJSON.h"
#include "esp_err.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "nvs.h"
#include "sdkconfig.h"
#include "r39_compliance.hpp"
#include "nvs_keys.hpp"

/** Maximum accepted POST /api/r39 body, in bytes. */
#define R39_WEB_MAX_BODY (256)

namespace r39_web
{
	/** Speeds (km/h) at which the preview is computed. */
	inline constexpr float PREVIEW_KPH[] = {0.0f, 2.0f, 5.0f, 10.0f, 20.0f, 40.0f, 80.0f, 120.0f, 160.0f};

	/** Stored settings as last read or written, for the live readout. Guarded by live_mux. */
	inline r39::Params live = r39::defaults().to_params(CONFIG_R39_BLEND_END_KPH);
	inline portMUX_TYPE live_mux = portMUX_INITIALIZER_UNLOCKED;

	/** Where the values came from. */
	enum class Source
	{
		Nvs,     /**< Every key stored and in range. */
		Default, /**< At least one key missing or out of range; defaults used for those. */
	};

	/**
	 * @brief Reads the stored settings. Nothing is written.
	 * @param[out] raw Stored values; defaults for any key that is missing or out of range.
	 * @return Source::Nvs when all three keys were stored and in range.
	 */
	inline Source load(r39::RawParams &raw)
	{
		raw = r39::defaults();
		nvs_handle_t h;
		if (nvs_open(vx::nvs::r39::NS, NVS_READONLY, &h) != ESP_OK)
			return Source::Default;

		bool all = true;
		uint8_t en;
		uint16_t a_bp;
		int16_t b_ckph;
		if (nvs_get_u8(h, vx::nvs::r39::EN, &en) == ESP_OK && en <= 1)
			raw.en = en;
		else
			all = false;
		if (nvs_get_u16(h, vx::nvs::r39::A_BP, &a_bp) == ESP_OK && a_bp <= r39::A_BP_MAX)
			raw.a_bp = a_bp;
		else
			all = false;
		if (nvs_get_i16(h, vx::nvs::r39::B_CKPH, &b_ckph) == ESP_OK && b_ckph >= r39::B_CKPH_MIN && b_ckph <= r39::B_CKPH_MAX)
			raw.b_ckph = b_ckph;
		else
			all = false;
		nvs_close(h);
		return all ? Source::Nvs : Source::Default;
	}

	/** @brief Publishes @p raw to the live readout. */
	inline void set_live(const r39::RawParams &raw)
	{
		const r39::Params p = raw.to_params(CONFIG_R39_BLEND_END_KPH);
		taskENTER_CRITICAL(&live_mux);
		live = p;
		taskEXIT_CRITICAL(&live_mux);
	}

	/** @brief Re-reads NVS into the live readout (e.g. after an NVS wipe). */
	inline void reload_live()
	{
		r39::RawParams raw;
		load(raw);
		set_live(raw);
	}

	/**
	 * @brief Builds the GET / POST reply.
	 * @param raw Values to report.
	 * @param src Their source.
	 * @return New JSON object, owned by the caller.
	 */
	inline cJSON *to_json(const r39::RawParams &raw, Source src)
	{
		const r39::Params p = raw.to_params(CONFIG_R39_BLEND_END_KPH);
		cJSON *root = cJSON_CreateObject();
		cJSON_AddStringToObject(root, "source", (src == Source::Nvs) ? "nvs" : "default");
		cJSON_AddBoolToObject(root, "enabled", raw.en == 1);
		cJSON_AddNumberToObject(root, "a_pct", raw.a_bp / 100.0);
		cJSON_AddNumberToObject(root, "a_bp", raw.a_bp);
		cJSON_AddNumberToObject(root, "b_kph", raw.b_ckph / 100.0);
		cJSON_AddNumberToObject(root, "b_ckph", raw.b_ckph);
		cJSON_AddNumberToObject(root, "blend_end_kph", CONFIG_R39_BLEND_END_KPH);

		cJSON *lim = cJSON_AddObjectToObject(root, "limits");
		cJSON_AddNumberToObject(lim, "a_pct_max", r39::A_BP_MAX / 100.0);
		cJSON_AddNumberToObject(lim, "b_kph_min", r39::B_CKPH_MIN / 100.0);
		cJSON_AddNumberToObject(lim, "b_kph_max", r39::B_CKPH_MAX / 100.0);

		const r39::RawParams d = r39::defaults();
		cJSON *def = cJSON_AddObjectToObject(root, "defaults");
		cJSON_AddBoolToObject(def, "enabled", d.en == 1);
		cJSON_AddNumberToObject(def, "a_pct", d.a_bp / 100.0);
		cJSON_AddNumberToObject(def, "b_kph", d.b_ckph / 100.0);

		// Preview with the mode forced on, so the curve can be checked before enabling it.
		r39::Params on = p;
		on.enabled = true;
		cJSON *prev = cJSON_AddArrayToObject(root, "preview");
		for (float v : PREVIEW_KPH)
		{
			cJSON *o = cJSON_CreateObject();
			cJSON_AddNumberToObject(o, "v", v);
			cJSON_AddNumberToObject(o, "out", on.apply(v));
			cJSON_AddItemToArray(prev, o);
		}
		return root;
	}

	/**
	 * @brief Sends @p root as a JSON response and frees it.
	 * @return ESP_OK, or ESP_FAIL when it could not be printed.
	 */
	inline esp_err_t send_json(httpd_req_t *req, cJSON *root)
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

	/** @brief GET /api/r39. Refreshes the live readout too. */
	inline esp_err_t get_handler(httpd_req_t *req)
	{
		r39::RawParams raw;
		const Source src = load(raw);
		set_live(raw);
		return send_json(req, to_json(raw, src));
	}

	/**
	 * @brief POST /api/r39: checks every present field, then stores the three keys and commits.
	 *
	 * Replies 400 with a message naming the field when a value is not a number (or bool), is negative, above its
	 * limit or has more than two decimals; nothing is stored then. 500 on an NVS error.
	 */
	inline esp_err_t post_handler(httpd_req_t *req)
	{
		if (req->content_len <= 0 || req->content_len > R39_WEB_MAX_BODY)
		{
			httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing or oversized body");
			return ESP_FAIL;
		}
		char body[R39_WEB_MAX_BODY + 1] = {0};
		int received = 0;
		while (received < (int)req->content_len)
		{
			int r = httpd_req_recv(req, body + received, req->content_len - received);
			if (r <= 0)
			{
				httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Body read failed");
				return ESP_FAIL;
			}
			received += r;
		}
		body[received] = '\0';

		cJSON *root = cJSON_Parse(body);
		if (!cJSON_IsObject(root))
		{
			cJSON_Delete(root);
			httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Expected a JSON object");
			return ESP_FAIL;
		}

		r39::RawParams raw;
		load(raw); // current values, so missing fields are kept
		const char *err_msg = NULL;
		bool any = false;

		const cJSON *en = cJSON_GetObjectItemCaseSensitive(root, "enabled");
		if (en != NULL)
		{
			if (!cJSON_IsBool(en))
				err_msg = "enabled must be true or false";
			else
			{
				raw.en = cJSON_IsTrue(en) ? 1 : 0;
				any = true;
			}
		}

		const cJSON *a = cJSON_GetObjectItemCaseSensitive(root, "a_pct");
		if (err_msg == NULL && a != NULL)
		{
			int32_t h = 0;
			if (!cJSON_IsNumber(a) || !r39::hundredths_from_user(a->valuedouble, r39::A_BP_MAX, &h))
				err_msg = "a_pct must be a number from 0 to 10 with at most 2 decimals";
			else
			{
				raw.a_bp = (uint16_t)h;
				any = true;
			}
		}

		const cJSON *b = cJSON_GetObjectItemCaseSensitive(root, "b_kph");
		if (err_msg == NULL && b != NULL)
		{
			int32_t h = 0;
			if (!cJSON_IsNumber(b) || !r39::hundredths_from_user(b->valuedouble, r39::B_CKPH_MAX, &h))
				err_msg = "b_kph must be a number from 0 to 4 with at most 2 decimals";
			else
			{
				raw.b_ckph = (int16_t)h;
				any = true;
			}
		}
		cJSON_Delete(root);

		if (err_msg == NULL && !any)
			err_msg = "No field found (enabled, a_pct, b_kph)";
		if (err_msg == NULL && !raw.in_range())
			err_msg = "Values out of range";
		if (err_msg != NULL)
		{
			ESP_LOGW(__func__, "Rejected ECE R39 settings: %s", err_msg);
			httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, err_msg);
			return ESP_FAIL;
		}

		nvs_handle_t h;
		esp_err_t err = nvs_open(vx::nvs::r39::NS, NVS_READWRITE, &h);
		if (err == ESP_OK)
		{
			err = nvs_set_u8(h, vx::nvs::r39::EN, raw.en);
			if (err == ESP_OK)
				err = nvs_set_u16(h, vx::nvs::r39::A_BP, raw.a_bp);
			if (err == ESP_OK)
				err = nvs_set_i16(h, vx::nvs::r39::B_CKPH, raw.b_ckph);
			if (err == ESP_OK)
				err = nvs_commit(h);
			nvs_close(h);
		}
		if (err != ESP_OK)
		{
			ESP_LOGE(__func__, "Could not store ECE R39 settings: %s", esp_err_to_name(err));
			httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, esp_err_to_name(err));
			return ESP_FAIL;
		}
		ESP_LOGI(__func__, "ECE R39 settings stored: %s, +%.2f %%, %.2f km/h", raw.en ? "ON" : "OFF", raw.a_pct(), raw.b_kph());
		set_live(raw);
		return send_json(req, to_json(raw, Source::Nvs));
	}
} // namespace r39_web

/** @return The stored ECE R39 settings, for the live readout. */
inline r39::Params r39_live_params()
{
	taskENTER_CRITICAL(&r39_web::live_mux);
	const r39::Params p = r39_web::live;
	taskEXIT_CRITICAL(&r39_web::live_mux);
	return p;
}

/** @brief Re-reads the stored ECE R39 settings into the live readout. */
inline void r39_live_reload() { r39_web::reload_live(); }

/**
 * @brief Loads the live settings and registers the two /api/r39 handlers.
 * @param[in] server Running HTTP server. Needs 2 free URI handler slots.
 * @return ESP_OK, or the first registration error.
 */
inline esp_err_t r39_web_register(httpd_handle_t server)
{
	r39_web::reload_live();
	static const struct
	{
		const char *uri;
		httpd_method_t method;
		esp_err_t (*handler)(httpd_req_t *r);
	} routes[] = {
		{"/api/r39", HTTP_GET, r39_web::get_handler},
		{"/api/r39", HTTP_POST, r39_web::post_handler},
	};
	for (const auto &route : routes)
	{
		httpd_uri_t u = {};
		u.uri = route.uri;
		u.method = route.method;
		u.handler = route.handler;
		esp_err_t err = httpd_register_uri_handler(server, &u);
		if (err != ESP_OK)
		{
			ESP_LOGE(__func__, "Could not register %s: %s", u.uri, esp_err_to_name(err));
			return err;
		}
	}
	return ESP_OK;
}
