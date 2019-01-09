#include <sys/types.h>
#include <sys/stat.h>
#include <malloc.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <unistd.h>
#include <locale.h>
#include <ctype.h>

#include <ogcsys.h>
#include <ogc/machine/processor.h>
#include <ogc/conf.h>
#include <debug.h>

#include "../config.h"
#include "controls.h"
#include "appentry.h"
#include "playtime.h"
#include "gfx.h"
#include "tex.h"
#include "theme.h"
#include "cursors.h"
#include "font.h"
#include "widgets.h"
#include "view.h"
#include "bubbles.h"
#include "dialogs.h"
#include "browser.h"
#include "m_main.h"
#include "loader.h"
#ifdef ENABLE_UPDATES
#include "http.h"
#include "update.h"
#endif
#include "i18n.h"
#include "dvd.h"
#include "manage.h"
#include "xml.h"
#include "title.h"

// Languages
#include "spanish_mo.h"
#include "dutch_mo.h"
#include "german_mo.h"
#include "french_mo.h"
#include "italian_mo.h"
#include "japanese_mo.h"

#ifdef DEBUG_APP
//#define GDBSTUB
#else
#undef GDBSTUB
#endif

#define WARN_SPLIT 360

extern const u8 stub_bin[];
extern const u8 stub_bin_end;
extern const u32 stub_bin_size;

u64 *conf_magic = STUB_ADDR_MAGIC;
u64 *conf_title_id = STUB_ADDR_TITLE;

static bool should_exit;
static bool shutdown;
#ifdef GDBSTUB
static bool gdb;
#endif
static const char *text_delete;
static const char *text_error_delete;

s32 __IOS_LoadStartupIOS(void) {
#if 0
	__ES_Init();
	__IOS_LaunchNewIOS(58);
#endif
	return 0;
}

#define MEM2_PROT          0x0D8B420A
#define ES_MODULE_START (u16*)0x939F0000

static const u16 ticket_check[] = {
    0x685B,               // ldr r3,[r3,#4] ; get TMD pointer
    0x22EC, 0x0052,       // movls r2, 0x1D8
    0x189B,               // adds r3, r3, r2; add offset of access rights field in TMD
    0x681B,               // ldr r3, [r3]   ; load access rights (haxxme!)
    0x4698,               // mov r8, r3  ; store it for the DVD video bitcheck later
    0x07DB                // lsls r3, r3, #31; check AHBPROT bit
};

static int patch_ahbprot_reset(void)
{
	u16 *patchme;

	if ((read32(0x0D800064) == 0xFFFFFFFF) ? 1 : 0) {
		write16(MEM2_PROT, 2);
		for (patchme=ES_MODULE_START; patchme < ES_MODULE_START+0x4000; ++patchme) {
			if (!memcmp(patchme, ticket_check, sizeof(ticket_check)))
			{
				// write16/uncached poke doesn't work for MEM2
				patchme[4] = 0x23FF; // li r3, 0xFF
				DCFlushRange(patchme+4, 2);
				return 0;
			}
		}
		return -1;
	} else {
		return -2;
	}
}

static void reset_cb(u32 irq, void *ctx) {
#ifdef GDBSTUB
	gdb = true;
#else
	should_exit = true;
#endif
}

static void power_cb(void) {
	should_exit = true;
	shutdown = true;
}

void config_language(void) {
	s32 language;
	const void *mo;

	// this is probably wrong, but we don't let the C library do any UTF-8 text
	// processing worth mentioning anyway, and I'm scared of what might happen
	// if I change this :P
	setlocale(LC_CTYPE, "C-ISO-8859-1");

#ifdef FORCE_LANG
	language = FORCE_LANG;
#else
	language = CONF_GetLanguage();
	if (language < 0)
		language = CONF_LANG_ENGLISH;
#endif

	mo = NULL;

	switch (language) {
		case CONF_LANG_ENGLISH:
			mo = NULL;
			break;

		case CONF_LANG_SPANISH:
			mo = spanish_mo;
			break;

		case CONF_LANG_FRENCH:
			mo = french_mo;
			break;

		case CONF_LANG_GERMAN:
			mo = german_mo;
			break;

		case CONF_LANG_DUTCH:
			mo = dutch_mo;
			break;

		case CONF_LANG_ITALIAN:
			mo = italian_mo;
			break;

		case CONF_LANG_JAPANESE:
			if (theme.langs & TLANG_JA) {
				mo = japanese_mo;
			} else {
				gprintf("language ja disabled due to missing theme support\n");
				language = CONF_LANG_ENGLISH;
			}
			break;

		default:
			gprintf("unsupported language %d, defaulting to English\n",
					language);
			language = CONF_LANG_ENGLISH;
	}

	gprintf("configuring language: %d with mo file at %p\n", language, mo);
	i18n_set_mo(mo);
}


static void main_pre(void) {
#ifdef DEBUG_APP
	if(usb_isgeckoalive(USBGECKO_CHANNEL))
		CON_EnableGecko(USBGECKO_CHANNEL, 1);

#ifdef GDBSTUB
	DEBUG_Init(GDBSTUB_DEVICE_USB, USBGECKO_CHANNEL);
#endif
#endif

	AUDIO_Init(NULL);
	DSP_Init();
	AUDIO_StopDMA();
	AUDIO_RegisterDMACallback(NULL);

	gfx_init_video();
	PAD_Init();

	SYS_SetResetCallback(reset_cb);
	title_init();
	SYS_SetPowerCallback(power_cb);

	gprintf("installing stub (%u)\n", stub_bin_size);

#ifdef DEBUG_APP
	if (stub_bin_size > 0x1800)
		gprintf("WARNING: stub too big!\n");
#endif

	memcpy((u32 *) 0x80001800, stub_bin, stub_bin_size);
	DCFlushRange((u32 *) 0x80001800, stub_bin_size);

	*conf_magic = 0;

	gprintf ("startup\n");
	gprintf("IOS Version: IOS%d %d.%d\n",
			IOS_GetVersion(), IOS_GetRevisionMajor(), IOS_GetRevisionMinor());

	ISFS_Initialize();

	WiiDVD_StopMotorAsync();

	gfx_init();
	app_entry_init();
	theme_xml_init();
	theme_init(NULL, 0);
	config_language();
	loader_init();
	controls_init();
	cursors_init();
	font_init();
	widgets_init();
	view_init();
	dialogs_init();
}

static void load_text(void)
{
	text_delete = _("Do you really want to delete this application?");
	text_error_delete = _("Error deleting '%s'");
}

static void refresh_theme(view *v, app_entry *app, u8 *data, u32 data_len) {
	browser_gen_view(BA_REMOVE, NULL);
	view_fade(v, TEX_LAYER_CURSOR + 1, 0, 0, 0, 0, 32, 8);
	font_clear();
	theme_deinit();
	theme_init(data, data_len);
	config_language();
	widgets_theme_reinit();
	bubbles_theme_reinit();
	view_theme_reinit();
	browser_theme_reinit();
	m_main_theme_reinit();
	dialogs_theme_reinit();
	view_fade(v, TEX_LAYER_CURSOR + 1, 0xff, 0xff, 0xff, 0xff, 31, -8);
	browser_gen_view(BA_REFRESH, app);
	load_text();
}

void main_real(void) {
	view *v_last, *v_current, *v_m_main, *v_browser, *v_detail, *v_about;

	u8 fhw;

	u32 bd, bh;
	s8 clicked;
	s16 mm;

#ifdef ENABLE_UPDATES
	s16 update_check = -1;
	bool update_in_progress = false;
	bool update_available = false;
#endif

	app_entry *app_sel;
	bool loading = false;

	loader_result ld_res;
	entry_point ep;
	bool reloced;
	bool ahb_access;
	bool launch_bootmii;

	u64 frame;
	bool exit_about;

	char charbuf[PATH_MAX];

	load_text();

	playtime_destroy();

	if (settings_load()) {
		app_entry_set_prefered(settings.device);
		app_entry_set_sort(settings.sort_order);

		if (settings.browse_mode == 1)
			browser_switch_mode();
	}

#ifdef ENABLE_UPDATES
	update_check = 60;
#endif

	app_sel = NULL;
	v_browser = browser_init();
	v_m_main = m_main_init ();
	view_bubbles = true;

#ifdef ENABLE_UPDATES
	http_init ();
#endif

	fhw = font_get_y_spacing(FONT_MEMO);

	should_exit = false;
	shutdown = false;
#ifdef GDBSTUB
	gdb = false;
#endif

	reloced = false;
	ahb_access = false;
	launch_bootmii = false;

	frame = 0;
	exit_about = false;

	v_last = v_current = v_browser;
	v_detail = NULL;
	v_about = NULL;

	view_fade(v_current, TEX_LAYER_CURSOR + 1, 0xff, 0xff, 0xff, 0xff, 31, -8);

	view_enable_cursor (true);

	while (!should_exit) {
#ifdef GDBSTUB
		if (gdb) {
			gprintf("attach gdb now!\n");
			_break();
			gdb = false;
			SYS_SetResetCallback (reset_cb);
		}
#endif
		memstats(false);

		if (v_current == v_browser) {
			switch (app_entry_action()) {
			case AE_ACT_NONE:
				break;

			case AE_ACT_REMOVE:
				app_sel = browser_sel();
				if (app_sel) {
					memset(settings.app_sel, 0, sizeof(settings.app_sel));
					strcpy(settings.app_sel, app_sel->dirname);
				}

				bubble_popall();
				browser_gen_view(BA_REMOVE, NULL);
				app_entries_free();
				memstats(true);
				break;

			case AE_ACT_ADD:
				if (strlen(settings.app_sel) > 0)
					app_sel = app_entry_find(settings.app_sel);
				else
					app_sel = NULL;

				view_show_throbber(false);
				browser_gen_view(BA_ADD, app_sel);
				break;
			}
		}

		if (loading != app_entry_is_loading()) {
			loading = !loading;

			view_show_throbber(loading);
		}

		if (loading)
			view_throbber_tickle();

		if ((frame % 30) == 0)
			app_entry_scan();

		view_plot (v_current, DIALOG_MASK_COLOR, &bd, &bh, NULL);

		frame++;
		if (v_last != v_current) {
			frame = 0;
			v_last = v_current;
		}

		if (bd & PADS_HOME) {
			if (v_current == v_browser) {
				m_main_update ();
				v_current = v_m_main;
				view_set_focus (v_m_main, 0);

				continue;
			} else {
				if (v_current == v_m_main)
					v_current = v_browser;

				continue;
			}
		}

		if (v_current == v_m_main) {
			if (bd & PADS_B) {
				v_current = v_browser;

				continue;
			}

			if (bd & PADS_UP)
				view_set_focus_prev (v_current);

			if (bd & PADS_DOWN)
				view_set_focus_next (v_current);

			if (bd & PADS_A) {
				switch (v_m_main->focus) {
				case 0:
					v_current = v_browser;
					continue;

				case 1:
					v_about = dialog_about (v_m_main);
					v_current = v_about;

					view_enable_cursor (false);

					dialog_fade (v_current, true);

					exit_about = false;

					continue;

				case 2:
					launch_bootmii = true;
					should_exit = true;
					break;

				case 3:
					should_exit = true;
					continue;

				case 4:
					should_exit = true;
					shutdown = true;
					break;
				}
			}

			continue;
		}

		if (v_current == v_browser) {
			widget_set_flag (&v_browser->widgets[2], WF_ENABLED,
								loader_gecko_initialized ());

			if (!loader_tcp_initializing ())
				widget_set_flag (&v_browser->widgets[3], WF_ENABLED,
									loader_tcp_initialized ());

			if (loader_tcp_initializing () && (frame % 20 == 0))
				widget_toggle_flag (&v_browser->widgets[3], WF_ENABLED);

			if (bd & PADS_NET_INIT)
				loader_tcp_init ();

#ifdef ENABLE_UPDATES
			if (loader_tcp_initialized () && (update_check > 0))
				update_check--;

			if (update_check == 0) {
				update_check = -1;
				update_in_progress = update_signal ();
			}

			if (update_in_progress) {
				if (!loading && !update_busy(&update_available))
					update_in_progress = false;
			}

			if (update_available) {
				update_available = false;

				reloced = update_execute (v_current, &ep);
				should_exit = reloced;

				continue;
			}
#endif
			if (bd & PADS_1) {
				dialog_options_result options;
				options = show_options_dialog(v_current);

				if (options.confirmed) {
					app_entry_set_sort(options.sort);
					app_entry_set_device(options.device);
					app_entry_set_prefered(options.device);
				}

				continue;
			}

			if (bd & PADS_2) {
				browser_switch_mode();
				continue;
			}

			if (bd & PADS_DPAD) {
				browser_set_focus(bd);
				continue;
			}

			if (bd & PADS_A) {
				clicked = view_widget_at_ir (v_browser);

				if (clicked == 0) {
					browser_gen_view(BA_PREV, NULL);
					continue;
				}

				if (clicked == 1) {
					browser_gen_view(BA_NEXT, NULL);
					continue;
				}

				if (clicked == 3) {
					loader_tcp_init ();
					continue;
				}

				app_sel = browser_sel();

				if (!app_sel)
					continue;

				v_detail = dialog_app (app_sel, v_browser);
				v_current = v_detail;

				dialog_fade (v_current, true);

				continue;
			}

			if ((bd | bh) & PADS_MINUS)
				browser_gen_view(BA_PREV, NULL);

			if ((bd | bh) & PADS_PLUS)
				browser_gen_view(BA_NEXT, NULL);

			if (loader_handshaked ()) {
				loader_load (&ld_res, v_browser, NULL);

				switch (ld_res.type) {
				case LT_UNKNOWN:
					break;

				case LT_EXECUTABLE:
					if (loader_load_executable(&ep, &ld_res, v_browser)) {
						reloced = true;
						ahb_access = true;
						should_exit = true;
					}

					break;

				case LT_ZIP_APP:
					if (loader_handle_zip_app(&ld_res, v_browser)) {
						app_sel = app_entry_add(ld_res.dirname);

						if (app_sel)
							browser_gen_view(BA_REFRESH, app_sel);
						memstats(true);
					}

					break;

				case LT_ZIP_THEME:
					refresh_theme(v_current, app_sel, ld_res.data,
									ld_res.data_len);
					fhw = font_get_y_spacing(FONT_MEMO);
					memstats(true);
					break;
				}

				continue;
			}

			if (!reloced)
				loader_signal_threads ();

			continue;
		}

		if (v_current == v_detail) {
			if (bd & PADS_LEFT)
				view_set_focus_prev (v_current);

			if (bd & PADS_RIGHT)
				view_set_focus_next (v_current);

			mm = 0;
			if (bd & PADS_UP)
				mm += fhw;

			if (bd & PADS_DOWN)
				mm -= fhw;

			mm += controls_sticky() / 8;

			if (v_current->drag && (v_current->drag_widget == 6)) {
				mm += -v_current->drag_y / 32;
			} else if (bd & PADS_B) {
				dialog_fade (v_current, false);

				v_current = v_browser;
				view_free (v_detail);
				v_detail = NULL;

				continue;
			}

			widget_scroll_memo_deco (&v_detail->widgets[5], mm);

			if ((bd & PADS_A) && v_detail->focus == 10) {
				dialog_fade (v_current, false);

				v_current = v_browser;
				view_free (v_detail);
				v_detail = NULL;

				continue;
			}

			if ((bd & PADS_A) && v_detail->focus == 8) {
				dialog_fade (v_current, false);

				v_current = v_browser;
				view_free (v_detail);
				v_detail = NULL;

				if (show_message (v_current, DLGMT_CONFIRM, DLGB_YESNO,
									text_delete, 1) == 1)
					continue;

				browser_gen_view(BA_REMOVE, NULL);
				if (!manage_run(v_current, app_sel->dirname, NULL, 0, 0)) {
					sprintf(charbuf, text_error_delete, app_sel->dirname);
					show_message(v_current, DLGMT_ERROR, DLGB_OK, charbuf, 0);
				} else {
					app_entry_remove(app_sel);
					app_sel = NULL;
				}
				browser_gen_view(BA_REFRESH, NULL);

				continue;
			}

			if ((bd & PADS_A) && v_detail->focus == 9) {
				dialog_fade (v_current, false);

				v_current = v_browser;

				view_free (v_detail);
				v_detail = NULL;

				loader_load(&ld_res, v_browser, app_sel);

				switch (ld_res.type) {
				case LT_UNKNOWN:
					break;

				case LT_EXECUTABLE:
					if (loader_load_executable(&ep, &ld_res, v_browser)) {
						reloced = true;

						if (app_sel && app_sel->meta)
							ahb_access = app_sel->meta->ahb_access;

						should_exit = true;
					}

					break;

				case LT_ZIP_APP:
					free(ld_res.data);
					break;

				case LT_ZIP_THEME:
					refresh_theme(v_current, app_sel, ld_res.data,
									ld_res.data_len);
					break;
				}

				continue;
			}

			continue;
		}

		if (v_current == v_about) {
			if (bd & (PADS_A | PADS_B))
				exit_about = true;

			if (exit_about) {
				dialog_fade (v_current, false);

				v_current = v_m_main;
				view_free (v_about);
				v_about = NULL;

				view_enable_cursor (true);

				continue;
			}

			if ((frame > 60 * 2) && (frame % 3) == 0)
				widget_scroll_memo (&v_current->widgets[2], -1);
		}
	}

	gprintf ("exiting\n");

	view_enable_cursor (false);

	if (v_current->sub_view)
		dialog_fade (v_current, false);

	view_fade (v_current, TEX_LAYER_CURSOR + 1, 0, 0, 0, 0, 32, 8);

	WiiDVD_ShutDown();
	controls_deinit();

	app_sel = browser_sel();
	if (app_sel) {
		size_t i, s = strlen(app_sel->dirname);
		memset(settings.app_sel, 0, sizeof(settings.app_sel));
		for (i = 0; i < s; ++i)
			settings.app_sel[i] = tolower((int) app_sel->dirname[i]);
	}

	// because the tcp thread is the only one that can block after the exit
	// command is sent, deinit it first. this gives the other threads a chance
	// to do any pending work if the tcp thread does block.
	loader_deinit ();
#ifdef ENABLE_UPDATES
        http_deinit ();
#endif
	app_entry_deinit ();

	gfx_deinit ();
	cursors_deinit ();

	browser_deinit ();
	m_main_deinit ();

	dialogs_deinit ();
	view_deinit ();
	widgets_deinit ();
	font_deinit ();
	theme_deinit();

	settings_save();

	if (launch_bootmii) {
		gprintf ("launching BootMii\n");
		__IOS_LaunchNewIOS(BOOTMII_IOS);
	}

	if (reloced) {
		if (ahb_access) {
			gprintf ("patching IOS for AHB access post-reload...\n");
			int res = patch_ahbprot_reset();
			if (res)
				gprintf ("patch failed (%d)\n", res);
		}

		gprintf ("reloading to IOS%d...\n", APPS_IOS_VERSION);
		__IOS_LaunchNewIOS(APPS_IOS_VERSION);

		if (ahb_access) {
			gprintf ("reenabling DVD access...\n");
			mask32(0x0D800180, 1<<21, 0);
		}

		gprintf ("branching to %p\n", ep);
		loader_exec (ep);
	}

	if (shutdown) {
		gprintf ("shutting down\n");
		SYS_ResetSystem(SYS_POWEROFF, 0, 0);
	}

	gprintf ("returning to sysmenu\n");
	SYS_ResetSystem (SYS_RETURNTOMENU, 0, 0);
}

int main(int argc, char *argv[]) {
	main_pre();
	main_real();
	gprintf("uh oh\n");
	return 0;
}

