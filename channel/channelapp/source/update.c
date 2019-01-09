
#include <malloc.h>
#include <stdio.h>
#include <string.h>

#include <ogcsys.h>

#include "../config.h"

#ifdef ENABLE_UPDATES

#include "view.h"
#include "dialogs.h"
#include "http.h"
#include "i18n.h"
#include "sha1.h"
#include "ecdsa.h"

#include "update.h"

#define		UPDATE_HEADER		"SIG0"
#define		UPDATE_OFFSET_R		4
#define		UPDATE_OFFSET_S		4+30
#define		UPDATE_OFFSET_XML	64


static const u8 update_key[] = UPDATE_PUBLIC_KEY;

static u8 nibble2hex(u8 byte)
{
	byte &= 0xf;
	if (byte <= 9)
		return '0' + byte;
	else
		return 'a' + byte - 0xa;
}

static void hash2ascii(u8 *hash, u8 *ascii)
{
	u32 i;
	for (i = 0; i < 20; i++) {
		ascii[2*i] = nibble2hex(hash[i] >> 4);
		ascii[(2*i) + 1] = nibble2hex(hash[i]);
	}
	ascii[40] = 0;
}

typedef struct {
	u8 *sha_data;
	u32 sha_len;

	u8 *ecdsa_r;
	u8 *ecdsa_s;

	bool valid;
	bool running;
} update_arg;

static lwpq_t update_queue;
static lwp_t update_thread;
static u8 update_stack[UPDATE_THREAD_STACKSIZE] ATTRIBUTE_ALIGN (32);
static update_arg ta_ua;

static void *update_func(void *arg) {
	u8 hash[20];
	update_arg *ua = (update_arg *) arg;

	SHA1(ua->sha_data, ua->sha_len, hash);
	if (check_ecdsa(update_key, ua->ecdsa_r, ua->ecdsa_s, hash))
		ua->valid = true;

	ua->running = false;
	return NULL;
}

typedef enum {
	UPDATE_IDLE = 0,
	UPDATE_FETCH_META,
	UPDATE_VALIDATE,
	UPDATE_NO_UPDATE,
	UPDATE_CAN_UPDATE
} update_state;

static const char *update_url = UPDATE_URL;
static update_state us = UPDATE_IDLE;
static update_info *info = NULL;

bool update_signal(void) {
	if ((us == UPDATE_FETCH_META) || us == UPDATE_CAN_UPDATE)
		return false;

	gprintf("starting update check\n");

	if (http_request(update_url, 8 * 1024)) {
		us = UPDATE_FETCH_META;
		return true;
	}

	return false;
}

static u8 *update_data = NULL;
static u32 update_len = 0;

bool update_busy(bool *update_available) {
	http_res http_result;
	u32 status;
	s32 res;

	if ((us != UPDATE_FETCH_META) && (us != UPDATE_VALIDATE))
		return false;

	if (us == UPDATE_FETCH_META) {
		if (!http_get_result(&http_result, &status, &update_data, &update_len))
			return true;

		*update_available = false;
		us = UPDATE_NO_UPDATE;

		gprintf("update check done, status=%u len=%u\n", status, update_len);

		if (http_result != HTTPR_OK)
			return false;

		gprintf("fetched update xml\n");

		if (update_len < 64) {
			gprintf("update.sxml length is invalid\n");
			free(update_data);
			return false;
		}

		if (memcmp(UPDATE_HEADER, update_data, 4)) {
			gprintf("SIG0 header not found.\n");
			free(update_data);
			return false;
		}

		memset(&ta_ua, 0, sizeof(ta_ua));
		ta_ua.sha_data = update_data + UPDATE_OFFSET_XML;
		ta_ua.sha_len = update_len - UPDATE_OFFSET_XML;
		ta_ua.ecdsa_r = update_data + UPDATE_OFFSET_R;
		ta_ua.ecdsa_s = update_data + UPDATE_OFFSET_S;
		ta_ua.running = true;

		memset(&update_stack, 0, UPDATE_THREAD_STACKSIZE);
		LWP_InitQueue(&update_queue);
		res = LWP_CreateThread(&update_thread, update_func, &ta_ua,
								update_stack, UPDATE_THREAD_STACKSIZE,
								UPDATE_THREAD_PRIO);

		if (res) {
			gprintf("error creating thread: %d\n", res);
			LWP_CloseQueue(update_queue);
			free(update_data);
			return false;
		}

		us = UPDATE_VALIDATE;
		return true;
	}

	if (ta_ua.running)
		return true;

	LWP_CloseQueue(update_queue);

	us = UPDATE_NO_UPDATE;

	if (!ta_ua.valid) {
		gprintf("update.sxml signature is invalid.\n");
		free(update_data);
		return false;
	}

	gprintf("update.sxml signature is valid.\n");

	info = update_parse((char *) update_data + UPDATE_OFFSET_XML);
	free(update_data);

	if (!info) {
		gprintf("Failed to parse update.sxml\n");
		return false;
	}

	if (!info->hash) {
		gprintf("no <hash> node in update.sxml\n");
		update_free(info);
		info = NULL;
		return false;
	}

	if (strnlen(info->hash, 40) != 40) {
		gprintf("Invalid <hash> length\n");
		update_free(info);
		info = NULL;
		return false;
	}

	if (info->date > CHANNEL_VERSION_DATE) {
		gprintf("update available\n");
		us = UPDATE_CAN_UPDATE;
		*update_available = true;
	} else {
		gprintf("hbc is up2date\n");
		update_free(info);
		info = NULL;
	}

	return false;
}

#define UPDATE_MSG_SIZE 4096

bool update_execute (view *sub_view, entry_point *ep) {
	http_state state;
	http_res res;
	u32 status, length, progress;
	u8 *data;
	view *v = sub_view;
	bool downloading = false;
	static char text[UPDATE_MSG_SIZE];
	bool b = false;
	u8 hash[20];
	u8 ascii_hash[41];

	if (us != UPDATE_CAN_UPDATE)
		return false;

	us = UPDATE_IDLE;

	snprintf (text, UPDATE_MSG_SIZE, _("An update to the Homebrew Channel "
				"(version %s, replacing the installed version %s) "
				"is available for installation, do you want "
				"to update now?\n\n"
				"Release notes:\n\n%s"),
				info->version, CHANNEL_VERSION_STR, info->notes);
	text[UPDATE_MSG_SIZE-1] = 0;

	if (show_message (sub_view, DLGMT_CONFIRM, DLGB_YESNO, text, 0) == 1) {
		update_free(info);
		info = NULL;
		return false;
	}

	if (!http_request(info->uri, 4 * 1024 * 1024)) {
		gprintf ("error requesting update download\n");
		update_free(info);
		info = NULL;
		return false;
	}

	while (true) {
		view_plot (v, DIALOG_MASK_COLOR, NULL, NULL, NULL);

		if (!http_get_state (&state, &length, &progress))
			break;

		if ((!downloading) && (state == HTTPS_RECEIVING)) {
			downloading = true;

			snprintf (text, 32, _("Downloading Update (%u kB)"), length / 1024);

			v = dialog_progress (sub_view, text, length);

			dialog_fade (v, true);
		}

		if (downloading)
			dialog_set_progress (v, progress);

		if (http_get_result (&res, &status, &data, &length)) {
			gprintf("update download done, status=%u len=%u\n", status, length);

			if (res == HTTPR_OK) {
				SHA1(data, length, hash);
				hash2ascii(hash, ascii_hash);
				gprintf("Calculated hash : %s\n", ascii_hash);
				gprintf("Expected hash   : %s\n", info->hash);

				if (memcmp(ascii_hash, info->hash, 40)) {
					gprintf("installer.elf hash is invalid.\n");
					free(data);
					update_free(info);
					info = NULL;
					break;
				}

				gprintf("installer.elf hash is valid.\n");
				b = true;
			}

			break;
		}
	}

	update_free(info);
	info = NULL;
	
	if (downloading)
		dialog_fade (v, false);

	if (!b) {
		show_message (sub_view, DLGMT_ERROR, DLGB_OK, _("Download failed"), 0);

		return false;
	}

	b = loader_reloc(ep, data, length,
						"installer.elf\0-updatehbc\0\0", 26, true);

	free (data);

	return b;
}
#endif
