// Copyright 2026, Varjo Aero on Linux contributors.
// SPDX-License-Identifier: BSL-1.0
/*!
 * @file
 * @brief  Varjo Aero settings: environment first, then the launcher's session file.
 *
 * Settings that the OpenXR client library reads (inset size, gaze, quality) must reach apps that the launcher does
 * not start itself -- Steam games inherit Steam's environment, not aero-monado.sh's. So the launcher also writes them
 * to ~/.config/monado/varjo-aero-session.conf (KEY=VALUE lines; AERO_SESSION_CONF overrides the path), and every
 * lookup checks the environment first, then that file. The file is read once per translation unit; callers cache the
 * parsed value themselves when they sit on a per-frame path.
 *
 * Header-only so the C state tracker, the compositor and the C++ lighthouse driver can share it without build changes.
 */

#pragma once

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define U_AERO_MAX_SETTINGS 32

struct u_aero_setting
{
	char key[64];
	char value[256];
};

static inline const struct u_aero_setting *
u_aero_session_settings(size_t *out_count)
{
	static struct u_aero_setting settings[U_AERO_MAX_SETTINGS];
	static size_t count = 0;
	static bool loaded = false;

	if (!loaded) {
		loaded = true;
		char path[512] = {0};
		const char *env = getenv("AERO_SESSION_CONF");
		const char *home = getenv("HOME");
		if (env != NULL && env[0] != '\0') {
			snprintf(path, sizeof(path), "%s", env);
		} else if (home != NULL) {
			snprintf(path, sizeof(path), "%s/.config/monado/varjo-aero-session.conf", home);
		}
		FILE *f = path[0] != '\0' ? fopen(path, "r") : NULL;
		char line[384];
		while (f != NULL && count < U_AERO_MAX_SETTINGS && fgets(line, sizeof(line), f) != NULL) {
			line[strcspn(line, "\r\n")] = '\0';
			char *eq = strchr(line, '=');
			if (line[0] == '#' || eq == NULL || eq == line ||
			    (size_t)(eq - line) >= sizeof(settings[0].key)) {
				continue;
			}
			const size_t key_len = (size_t)(eq - line);
			memcpy(settings[count].key, line, key_len);
			settings[count].key[key_len] = '\0';
			snprintf(settings[count].value, sizeof(settings[count].value), "%s", eq + 1);
			count++;
		}
		if (f != NULL) {
			fclose(f);
		}
	}

	*out_count = count;
	return settings;
}

//! Like u_aero_get, but reads the session file again (no cache) -- for settings that may change while Monado runs.
//! The value is copied to out (out_size bytes); false when the setting is set nowhere.
static inline bool
u_aero_get_fresh(const char *key, char *out, size_t out_size)
{
	const char *env = getenv(key);
	if (env != NULL) {
		snprintf(out, out_size, "%s", env);
		return true;
	}
	char path[512] = {0};
	const char *conf = getenv("AERO_SESSION_CONF");
	const char *home = getenv("HOME");
	if (conf != NULL && conf[0] != '\0') {
		snprintf(path, sizeof(path), "%s", conf);
	} else if (home != NULL) {
		snprintf(path, sizeof(path), "%s/.config/monado/varjo-aero-session.conf", home);
	}
	FILE *f = path[0] != '\0' ? fopen(path, "r") : NULL;
	bool found = false;
	char line[384];
	const size_t key_len = strlen(key);
	while (f != NULL && !found && fgets(line, sizeof(line), f) != NULL) {
		line[strcspn(line, "\r\n")] = '\0';
		if (strncmp(line, key, key_len) == 0 && line[key_len] == '=') {
			const char *value = line + key_len + 1;
			size_t len = strlen(value);
			if (len >= out_size) {
				len = out_size - 1; // truncate, like snprintf
			}
			memcpy(out, value, len);
			out[len] = '\0';
			found = true;
		}
	}
	if (f != NULL) {
		fclose(f);
	}
	return found;
}

//! The setting's value, or NULL when it is set neither in the environment nor in the session file.
static inline const char *
u_aero_get(const char *key)
{
	const char *env = getenv(key);
	if (env != NULL) {
		return env;
	}
	size_t count = 0;
	const struct u_aero_setting *settings = u_aero_session_settings(&count);
	for (size_t i = 0; i < count; i++) {
		if (strcmp(settings[i].key, key) == 0) {
			return settings[i].value;
		}
	}
	return NULL;
}

//! True for "1", false for "0", @p default_value when unset or anything else.
static inline bool
u_aero_get_bool(const char *key, bool default_value)
{
	const char *v = u_aero_get(key);
	if (v != NULL && (v[0] == '0' || v[0] == '1') && v[1] == '\0') {
		return v[0] == '1';
	}
	return default_value;
}

static inline float
u_aero_get_float(const char *key, float default_value)
{
	const char *v = u_aero_get(key);
	return (v != NULL && v[0] != '\0') ? (float)atof(v) : default_value;
}
