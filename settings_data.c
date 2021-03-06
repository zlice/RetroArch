/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2013-2014 - Jason Fetters
 * 
 *  RetroArch is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  RetroArch is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with RetroArch.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#include "settings_data.h"
#include "file_path.h"
#include "input/input_common.h"
#include "config.def.h"

#define ENFORCE_RANGE(setting, type)                  \
{                                                     \
   if (setting->flags & SD_FLAG_HAS_RANGE)            \
   {                                                  \
      if (*setting->value.type < setting->min)        \
      *setting->value.type = setting->min;         \
      if (*setting->value.type > setting->max)        \
      *setting->value.type = setting->max;         \
   }                                                  \
}

// HACK
struct settings fake_settings;
struct global fake_extern;

void setting_data_load_current(void)
{
   memcpy(&fake_settings, &g_settings, sizeof(struct settings));
   memcpy(&fake_extern, &g_extern, sizeof(struct global));
}

// Input
static const char* get_input_config_prefix(const rarch_setting_t* setting)
{
   static char buffer[32];
   snprintf(buffer, 32, "input%cplayer%d", setting->index ? '_' : '\0', setting->index);
   return buffer;
}

static const char* get_input_config_key(const rarch_setting_t* setting, const char* type)
{
   static char buffer[64];
   snprintf(buffer, 64, "%s_%s%c%s", get_input_config_prefix(setting), setting->name, type ? '_' : '\0', type);
   return buffer;
}

//FIXME - make portable
#ifdef APPLE
static const char* get_key_name(const rarch_setting_t* setting)
{
   uint32_t hidkey, i;

   if (BINDFOR(*setting).key == RETROK_UNKNOWN)
      return "nul";

   hidkey = input_translate_rk_to_keysym(BINDFOR(*setting).key);

   for (i = 0; apple_key_name_map[i].hid_id; i++)
      if (apple_key_name_map[i].hid_id == hidkey)
         return apple_key_name_map[i].keyname;

   return "nul";
}
#endif


static const char* get_button_name(const rarch_setting_t* setting)
{
   static char buffer[32];

   if (BINDFOR(*setting).joykey == NO_BTN)
      return "nul";

   snprintf(buffer, 32, "%lld", (long long int)(BINDFOR(*setting).joykey));
   return buffer;
}

static const char* get_axis_name(const rarch_setting_t* setting)
{
   static char buffer[32];
   uint32_t joyaxis = BINDFOR(*setting).joyaxis;

   if (AXIS_NEG_GET(joyaxis) != AXIS_DIR_NONE)
      snprintf(buffer, 8, "-%d", AXIS_NEG_GET(joyaxis));
   else if (AXIS_POS_GET(joyaxis) != AXIS_DIR_NONE)
      snprintf(buffer, 8, "+%d", AXIS_POS_GET(joyaxis));
   else
      return "nul";

   return buffer;
}

void setting_data_reset_setting(const rarch_setting_t* setting)
{
   switch (setting->type)
   {
      case ST_BOOL:
         *setting->value.boolean          = setting->default_value.boolean;
         break;
      case ST_INT:
         *setting->value.integer          = setting->default_value.integer;
         break;
      case ST_UINT:
         *setting->value.unsigned_integer = setting->default_value.unsigned_integer;
         break;
      case ST_FLOAT:
         *setting->value.fraction         = setting->default_value.fraction;
         break;
      case ST_BIND:
         *setting->value.keybind          = *setting->default_value.keybind;
         break;
      case ST_STRING:
      case ST_PATH:
         if (setting->default_value.string)
         {
            if (setting->type == ST_STRING)
               strlcpy(setting->value.string, setting->default_value.string, setting->size);
            else
               fill_pathname_expand_special(setting->value.string, setting->default_value.string, setting->size);
         }
         break;
      default:
         break;
   }

   if (setting->change_handler)
      setting->change_handler(setting);
}

void setting_data_reset(const rarch_setting_t* settings)
{
   const rarch_setting_t *setting;
   memset(&fake_settings, 0, sizeof(fake_settings));
   memset(&fake_extern, 0, sizeof(fake_extern));

   for (setting = settings; setting->type != ST_NONE; setting++)
      setting_data_reset_setting(setting);
}

bool setting_data_load_config_path(const rarch_setting_t* settings, const char* path)
{
   config_file_t *config;

   if (!(config = (config_file_t*)config_file_new(path)))
      return NULL;

   setting_data_load_config(settings, config);
   config_file_free(config);

   return config;
}

bool setting_data_load_config(const rarch_setting_t* settings, config_file_t* config)
{
   const rarch_setting_t *setting;
   if (!config)
      return false;

   for (setting = settings; setting->type != ST_NONE; setting++)
   {
      switch (setting->type)
      {
         case ST_BOOL:
            config_get_bool  (config, setting->name, setting->value.boolean);
            break;
         case ST_PATH:
            config_get_path  (config, setting->name, setting->value.string, setting->size);
            break;
         case ST_STRING:
            config_get_array (config, setting->name, setting->value.string, setting->size);
            break;
         case ST_INT:
            config_get_int(config, setting->name, setting->value.integer);
            ENFORCE_RANGE(setting, integer);
            break;
         case ST_UINT:
            config_get_uint(config, setting->name, setting->value.unsigned_integer);
            ENFORCE_RANGE(setting, unsigned_integer);
            break;
         case ST_FLOAT:
            config_get_float(config, setting->name, setting->value.fraction);
            ENFORCE_RANGE(setting, fraction);
            break;         
         case ST_BIND:
            {
               const char *prefix = (const char *)get_input_config_prefix(setting);
               input_config_parse_key       (config, prefix, setting->name, setting->value.keybind);
               input_config_parse_joy_button(config, prefix, setting->name, setting->value.keybind);
               input_config_parse_joy_axis  (config, prefix, setting->name, setting->value.keybind);
            }
            break;
         case ST_HEX:
            break;
         default:
            break;
      }
   }

   return true;
}

bool setting_data_save_config_path(const rarch_setting_t* settings, const char* path)
{
   config_file_t* config;
   bool result = false;

   if (!(config = (config_file_t*)config_file_new(path)))
      config = config_file_new(0);

   setting_data_save_config(settings, config);
   result = config_file_write(config, path);
   config_file_free(config);

   return result;
}

bool setting_data_save_config(const rarch_setting_t* settings, config_file_t* config)
{
   const rarch_setting_t *setting;

   if (!config)
      return false;

   for (setting = settings; setting->type != ST_NONE; setting++)
   {
      switch (setting->type)
      {
         case ST_BOOL:
            config_set_bool(config, setting->name, *setting->value.boolean);
            break;
         case ST_PATH:
            config_set_path(config, setting->name,  setting->value.string);
            break;
         case ST_STRING:
            config_set_string(config, setting->name,  setting->value.string);
            break;
         case ST_INT:
            ENFORCE_RANGE(setting, integer);
            config_set_int(config, setting->name, *setting->value.integer);
            break;
         case ST_UINT:
            ENFORCE_RANGE(setting, unsigned_integer);         
            config_set_uint64(config, setting->name, *setting->value.unsigned_integer);
            break;
         case ST_FLOAT:
            ENFORCE_RANGE(setting, fraction);         
            config_set_float(config, setting->name, *setting->value.fraction);
            break;
         case ST_BIND:
            //FIXME: make portable
#ifdef APPLE
            config_set_string(config, get_input_config_key(setting, 0     ), get_key_name(setting));
#endif
            config_set_string(config, get_input_config_key(setting, "btn" ), get_button_name(setting));
            config_set_string(config, get_input_config_key(setting, "axis"), get_axis_name(setting));
            break;
         case ST_HEX:
            break;
         default:
            break;
      }
   }

   return true;
}

const rarch_setting_t* setting_data_find_setting(const rarch_setting_t* settings, const char* name)
{
   const rarch_setting_t *setting;

   if (!name)
      return NULL;

   for (setting = settings; setting->type != ST_NONE; setting++)
      if (setting->type <= ST_GROUP && strcmp(setting->name, name) == 0)
         return setting;

   return NULL;
}

void setting_data_set_with_string_representation(const rarch_setting_t* setting, const char* value)
{
   if (!setting || !value)
      return;

   switch (setting->type)
   {
      case ST_INT:
         sscanf(value, "%d", setting->value.integer);
         ENFORCE_RANGE(setting, integer);
         break;
      case ST_UINT:
         sscanf(value, "%u", setting->value.unsigned_integer);
         ENFORCE_RANGE(setting, unsigned_integer);
         break;      
      case ST_FLOAT:
         sscanf(value, "%f", setting->value.fraction);
         ENFORCE_RANGE(setting, fraction);         
         break;
      case ST_PATH:
         strlcpy(setting->value.string, value, setting->size);
         break;
      case ST_STRING:
         strlcpy(setting->value.string, value, setting->size);
         break;

      default:
         return;
   }

   if (setting->change_handler)
      setting->change_handler(setting);
}

const char* setting_data_get_string_representation(const rarch_setting_t* setting, char* buffer, size_t length)
{
   if (!setting || !buffer || !length)
      return "";

   switch (setting->type)
   {
      case ST_BOOL:
         snprintf(buffer, length, "%s", *setting->value.boolean ? "True" : "False");
         break;
      case ST_INT:
         snprintf(buffer, length, "%d", *setting->value.integer);
         break;
      case ST_UINT:
         snprintf(buffer, length, "%u", *setting->value.unsigned_integer);
         break;
      case ST_FLOAT:
         snprintf(buffer, length, "%f", *setting->value.fraction);
         break;
      case ST_PATH:
         strlcpy(buffer, setting->value.string, length);
         break;
      case ST_STRING:
         strlcpy(buffer, setting->value.string, length);
         break;
      case ST_BIND:
#ifdef APPLE
         snprintf(buffer, length, "[KB:%s] [JS:%s] [AX:%s]", get_key_name(setting), get_button_name(setting), get_axis_name(setting));
#endif
         break;
      default:
         return "";
   }

   return buffer;
}

rarch_setting_t setting_data_group_setting(enum setting_type type, const char* name)
{
   rarch_setting_t result = { type, name };
   return result;
}

#define DEFINE_BASIC_SETTING_TYPE(TAG, TYPE, VALUE, SETTING_TYPE, GROUP, SUBGROUP) \
   rarch_setting_t setting_data_##TAG##_setting(const char* name, const char* description, TYPE* target, TYPE default_value, const char *group, const char *subgroup) \
{ \
   rarch_setting_t result = { SETTING_TYPE, name, sizeof(TYPE), description, group, subgroup }; \
   result.value.VALUE = target; \
   result.default_value.VALUE = default_value; \
   return result; \
}

   DEFINE_BASIC_SETTING_TYPE(bool, bool, boolean, ST_BOOL, const char *, const char*)
   DEFINE_BASIC_SETTING_TYPE(int, int, integer, ST_INT, const char *, const char *)
   DEFINE_BASIC_SETTING_TYPE(uint, unsigned int, unsigned_integer, ST_UINT, const char *, const char *)
   DEFINE_BASIC_SETTING_TYPE(float, float, fraction, ST_FLOAT, const char *, const char *)

rarch_setting_t setting_data_string_setting(enum setting_type type,
      const char* name, const char* description, char* target,
      unsigned size, const char* default_value,
      const char *group, const char *subgroup)
{
   rarch_setting_t result = { type, name, size, description, group, subgroup };

   result.value.string = target;
   result.default_value.string = default_value;
   return result;
}

rarch_setting_t setting_data_bind_setting(const char* name,
      const char* description, struct retro_keybind* target,
      uint32_t index, const struct retro_keybind* default_value,
      const char *group, const char *subgroup)
{
   rarch_setting_t result = { ST_BIND, name, 0, description, group, subgroup };

   result.value.keybind = target;
   result.default_value.keybind = default_value;
   result.index = index;
   return result;
}


#define g_settings fake_settings
#define g_extern fake_extern

#define NEXT (list[index++])
#define START_GROUP(NAME)                       { const char *GROUP_NAME = NAME; NEXT = setting_data_group_setting (ST_GROUP, NAME); 
#define END_GROUP()                             NEXT = setting_data_group_setting (ST_END_GROUP, 0); }
#define START_SUB_GROUP(NAME)                   { const char *SUBGROUP_NAME = NAME; NEXT = setting_data_group_setting (ST_SUB_GROUP, NAME);
#define END_SUB_GROUP()                         NEXT = setting_data_group_setting (ST_END_SUB_GROUP, 0); }
#define CONFIG_BOOL(TARGET, NAME, SHORT, DEF, GROUP, SUBGROUP)   NEXT = setting_data_bool_setting  (NAME, SHORT, &TARGET, DEF, GROUP, SUBGROUP);
#define CONFIG_INT(TARGET, NAME, SHORT, DEF, GROUP, SUBGROUP)    NEXT = setting_data_int_setting   (NAME, SHORT, &TARGET, DEF, GROUP, SUBGROUP);
#define CONFIG_UINT(TARGET, NAME, SHORT, DEF, GROUP, SUBGROUP)   NEXT = setting_data_uint_setting  (NAME, SHORT, &TARGET, DEF, GROUP, SUBGROUP);
#define CONFIG_FLOAT(TARGET, NAME, SHORT, DEF, GROUP, SUBGROUP)  NEXT = setting_data_float_setting (NAME, SHORT, &TARGET, DEF, GROUP, SUBGROUP);
#define CONFIG_PATH(TARGET, NAME, SHORT, DEF, GROUP, SUBGROUP)   NEXT = setting_data_string_setting(ST_PATH, NAME, SHORT, TARGET, sizeof(TARGET), DEF, GROUP, SUBGROUP);
#define CONFIG_STRING(TARGET, NAME, SHORT, DEF, GROUP, SUBGROUP) NEXT = setting_data_string_setting(ST_STRING, NAME, SHORT, TARGET, sizeof(TARGET), DEF, GROUP, SUBGROUP);
#define CONFIG_HEX(TARGET, NAME, SHORT, GROUP, SUBGROUP)
#define CONFIG_BIND(TARGET, PLAYER, NAME, SHORT, DEF, GROUP, SUBGROUP) \
   NEXT = setting_data_bind_setting  (NAME, SHORT, &TARGET, PLAYER, DEF, GROUP, SUBGROUP);

#define WITH_FLAGS(FLAGS) (list[index - 1]).flags |= FLAGS;

#define WITH_RANGE(MIN, MAX)    \
   (list[index - 1]).min = MIN; \
(list[index - 1]).max = MAX; \
WITH_FLAGS(SD_FLAG_HAS_RANGE)

#define WITH_VALUES(VALUES) (list[index -1]).values = VALUES;

const rarch_setting_t* setting_data_get_list(void)
{
   int i, player, index;
   static rarch_setting_t list[SETTINGS_DATA_LIST_SIZE];
   static bool initialized = false;

   if (!initialized)
   {
      for (i = 0; i < SETTINGS_DATA_LIST_SIZE; i++)
      {
         list[i].type = ST_NONE;
         list[i].name = NULL;
         list[i].size = 0;
         list[i].short_description = NULL;
         list[i].index = 0;
         list[i].min = 0;
         list[i].max = 0;
         list[i].values = NULL;
         list[i].flags = 0;
      }

      initialized = true;
   }

   if (list[0].type == ST_NONE)
   {
      index = 0;

      /***********/
      /* DRIVERS */
      /***********/
         START_GROUP("Driver Options")
         START_SUB_GROUP("Driver Options")
         CONFIG_STRING(g_settings.video.driver,             "video_driver",               "Video Driver",               config_get_default_video(), GROUP_NAME, SUBGROUP_NAME)
#ifdef HAVE_OPENGL
         CONFIG_STRING(g_settings.video.gl_context,         "video_gl_context",           "OpenGL Context Driver",      "", GROUP_NAME, SUBGROUP_NAME)
#endif
         CONFIG_STRING(g_settings.audio.driver,             "audio_driver",               "Audio Driver",               config_get_default_audio(), GROUP_NAME, SUBGROUP_NAME)
         CONFIG_STRING(g_settings.input.driver,             "input_driver",               "Input Driver",               config_get_default_input(), GROUP_NAME, SUBGROUP_NAME)
#ifdef HAVE_CAMERA
         CONFIG_STRING(g_settings.camera.device,            "camera_device",              "Camera Driver",              config_get_default_camera(), GROUP_NAME, SUBGROUP_NAME)
#endif         
#ifdef HAVE_LOCATION
         CONFIG_STRING(g_settings.location.driver,          "location_driver",            "Location Driver",            config_get_default_location(), GROUP_NAME, SUBGROUP_NAME)
#endif         
         CONFIG_STRING(g_settings.input.joypad_driver,      "input_joypad_driver",        "Joypad Driver",              "", GROUP_NAME, SUBGROUP_NAME)
         CONFIG_STRING(g_settings.input.keyboard_layout,    "input_keyboard_layout",      "Keyboard Layout",            "", GROUP_NAME, SUBGROUP_NAME)

         END_SUB_GROUP()
         END_GROUP()



         /*************/
         /* EMULATION */
         /*************/
         START_GROUP("General Options")
         START_SUB_GROUP("General Options")
         CONFIG_BOOL(g_extern.config_save_on_exit,          "config_save_on_exit",        "Configuration Save On Exit", config_save_on_exit, GROUP_NAME, SUBGROUP_NAME)
         CONFIG_BOOL(g_settings.fps_show,                   "fps_show",                   "Show Framerate",             fps_show, GROUP_NAME, SUBGROUP_NAME)
         CONFIG_BOOL(g_settings.rewind_enable,              "rewind_enable",              "Rewind",                     rewind_enable, GROUP_NAME, SUBGROUP_NAME)
         //CONFIG_INT(g_settings.rewind_buffer_size,          "rewind_buffer_size",         "Rewind Buffer Size",       rewind_buffer_size)     WITH_SCALE(1000000)
         CONFIG_UINT(g_settings.rewind_granularity,         "rewind_granularity",         "Rewind Granularity",         rewind_granularity, GROUP_NAME, SUBGROUP_NAME)
         CONFIG_BOOL(g_settings.block_sram_overwrite,       "block_sram_overwrite",       "SRAM Block overwrite",       block_sram_overwrite, GROUP_NAME, SUBGROUP_NAME)
         CONFIG_UINT(g_settings.autosave_interval,          "autosave_interval",          "SRAM Autosave",          autosave_interval, GROUP_NAME, SUBGROUP_NAME)
         CONFIG_BOOL(g_settings.video.disable_composition,  "video_disable_composition",  "Window Compositing",         disable_composition, GROUP_NAME, SUBGROUP_NAME)
         CONFIG_BOOL(g_settings.pause_nonactive,            "pause_nonactive",            "Window Unfocus Pause",       pause_nonactive, GROUP_NAME, SUBGROUP_NAME)
         CONFIG_FLOAT(g_settings.fastforward_ratio,         "fastforward_ratio",          "Maximum Run Speed",         fastforward_ratio, GROUP_NAME, SUBGROUP_NAME)
         CONFIG_FLOAT(g_settings.slowmotion_ratio,          "slowmotion_ratio",           "Slow-Motion Ratio",          slowmotion_ratio, GROUP_NAME, SUBGROUP_NAME)       WITH_RANGE(0, 1)
         CONFIG_BOOL(g_settings.savestate_auto_index,       "savestate_auto_index",       "Save State Auto Index",      savestate_auto_index, GROUP_NAME, SUBGROUP_NAME)
         CONFIG_BOOL(g_settings.savestate_auto_save,        "savestate_auto_save",        "Auto Save State",            savestate_auto_save, GROUP_NAME, SUBGROUP_NAME)
         CONFIG_BOOL(g_settings.savestate_auto_load,        "savestate_auto_load",        "Auto Load State",            savestate_auto_load, GROUP_NAME, SUBGROUP_NAME)
         CONFIG_INT(g_extern.state_slot,                    "state_slot",                 "State Slot",                 0, GROUP_NAME, SUBGROUP_NAME)
         END_SUB_GROUP()
       START_SUB_GROUP("Miscellaneous")
       CONFIG_BOOL(g_settings.network_cmd_enable,         "network_cmd_enable",         "Network Commands",           network_cmd_enable, GROUP_NAME, SUBGROUP_NAME)
       //CONFIG_INT(g_settings.network_cmd_port,            "network_cmd_port",           "Network Command Port",       network_cmd_port, GROUP_NAME, SUBGROUP_NAME)
       CONFIG_BOOL(g_settings.stdin_cmd_enable,           "stdin_cmd_enable",           "stdin command",              stdin_cmd_enable, GROUP_NAME, SUBGROUP_NAME)
       END_SUB_GROUP()
       END_GROUP()

         /*********/
         /* VIDEO */
         /*********/
         START_GROUP("Video Options")
         START_SUB_GROUP("Monitor")
         CONFIG_UINT(g_settings.video.monitor_index,        "video_monitor_index",        "Monitor Index",              monitor_index, GROUP_NAME, SUBGROUP_NAME)
         CONFIG_BOOL(g_settings.video.fullscreen,           "video_fullscreen",           "Use Fullscreen mode",        fullscreen, GROUP_NAME, SUBGROUP_NAME)
         CONFIG_BOOL(g_settings.video.windowed_fullscreen,  "video_windowed_fullscreen",  "Windowed Fullscreen Mode",   windowed_fullscreen, GROUP_NAME, SUBGROUP_NAME)
         CONFIG_UINT(g_settings.video.fullscreen_x,         "video_fullscreen_x",         "Fullscreen Width",           fullscreen_x, GROUP_NAME, SUBGROUP_NAME)
         CONFIG_UINT(g_settings.video.fullscreen_y,         "video_fullscreen_y",         "Fullscreen Height",          fullscreen_y, GROUP_NAME, SUBGROUP_NAME)
         CONFIG_FLOAT(g_settings.video.refresh_rate,        "video_refresh_rate",         "Refresh Rate",               refresh_rate, GROUP_NAME, SUBGROUP_NAME)
         END_SUB_GROUP()

         START_SUB_GROUP("Aspect")
         CONFIG_BOOL(g_settings.video.force_aspect,         "video_force_aspect",         "Force aspect ratio",         force_aspect, GROUP_NAME, SUBGROUP_NAME)
         CONFIG_FLOAT(g_settings.video.aspect_ratio,        "video_aspect_ratio",         "Aspect Ratio",               aspect_ratio, GROUP_NAME, SUBGROUP_NAME)
         CONFIG_BOOL(g_settings.video.aspect_ratio_auto,    "video_aspect_ratio_auto",    "Use Auto Aspect Ratio",      aspect_ratio_auto, GROUP_NAME, SUBGROUP_NAME)
         CONFIG_UINT(g_settings.video.aspect_ratio_idx,     "aspect_ratio_index",         "Aspect Ratio Index",         aspect_ratio_idx, GROUP_NAME, SUBGROUP_NAME)
         END_SUB_GROUP()

         START_SUB_GROUP("Scaling")
         CONFIG_FLOAT(g_settings.video.xscale,              "video_xscale",               "Windowed Scale (X)",                    xscale, GROUP_NAME, SUBGROUP_NAME)
         CONFIG_FLOAT(g_settings.video.yscale,              "video_yscale",               "Windowed Scale (Y)",                    yscale, GROUP_NAME, SUBGROUP_NAME)
         CONFIG_BOOL(g_settings.video.scale_integer,        "video_scale_integer",        "Integer Scale",      scale_integer, GROUP_NAME, SUBGROUP_NAME)

         CONFIG_INT(g_extern.console.screen.viewports.custom_vp.x,         "custom_viewport_x",       "Custom Viewport X",       0, GROUP_NAME, SUBGROUP_NAME)
         CONFIG_INT(g_extern.console.screen.viewports.custom_vp.y,         "custom_viewport_y",       "Custom Viewport Y",       0, GROUP_NAME, SUBGROUP_NAME)
         CONFIG_UINT(g_extern.console.screen.viewports.custom_vp.width,    "custom_viewport_width",   "Custom Viewport Width",   0, GROUP_NAME, SUBGROUP_NAME)
         CONFIG_UINT(g_extern.console.screen.viewports.custom_vp.height,   "custom_viewport_height",  "Custom Viewport Height",  0, GROUP_NAME, SUBGROUP_NAME)

         CONFIG_BOOL(g_settings.video.smooth,               "video_smooth",               "Use bilinear filtering",     video_smooth, GROUP_NAME, SUBGROUP_NAME)
         CONFIG_UINT(g_settings.video.rotation,             "video_rotation",             "Rotation",                   0, GROUP_NAME, SUBGROUP_NAME)
         END_SUB_GROUP()


         START_SUB_GROUP("Synchronization")
         CONFIG_BOOL(g_settings.video.threaded,             "video_threaded",             "Threaded Video",         video_threaded, GROUP_NAME, SUBGROUP_NAME)
         CONFIG_BOOL(g_settings.video.vsync,                "video_vsync",                "VSync",                      vsync, GROUP_NAME, SUBGROUP_NAME)
         CONFIG_UINT(g_settings.video.swap_interval,        "video_swap_interval",        "VSync Swap Interval",        swap_interval, GROUP_NAME, SUBGROUP_NAME)       WITH_RANGE(1, 4)
         CONFIG_BOOL(g_settings.video.hard_sync,            "video_hard_sync",            "Hard GPU Sync",              hard_sync, GROUP_NAME, SUBGROUP_NAME)
         CONFIG_UINT(g_settings.video.hard_sync_frames,     "video_hard_sync_frames",     "Hard GPU Sync Frames",       hard_sync_frames, GROUP_NAME, SUBGROUP_NAME)    WITH_RANGE(0, 3)
         CONFIG_BOOL(g_settings.video.black_frame_insertion,"video_black_frame_insertion","Black Frame Insertion",      black_frame_insertion, GROUP_NAME, SUBGROUP_NAME)
         END_SUB_GROUP()

         START_SUB_GROUP("Miscellaneous")
         CONFIG_BOOL(g_settings.video.post_filter_record,   "video_post_filter_record",   "Post filter record",         post_filter_record, GROUP_NAME, SUBGROUP_NAME)
         CONFIG_BOOL(g_settings.video.gpu_record,           "video_gpu_record",           "GPU Record",                 gpu_record, GROUP_NAME, SUBGROUP_NAME)
         CONFIG_BOOL(g_settings.video.gpu_screenshot,       "video_gpu_screenshot",       "GPU Screenshot",             gpu_screenshot, GROUP_NAME, SUBGROUP_NAME)
         CONFIG_BOOL(g_settings.video.allow_rotate,         "video_allow_rotate",         "Allow rotation",             allow_rotate, GROUP_NAME, SUBGROUP_NAME)
         CONFIG_BOOL(g_settings.video.crop_overscan,        "video_crop_overscan",        "Crop Overscan (reload)",     crop_overscan, GROUP_NAME, SUBGROUP_NAME)

         CONFIG_PATH(g_settings.video.filter_path,          "video_filter",               "Software filter",            "", GROUP_NAME, SUBGROUP_NAME)       WITH_FLAGS(SD_FLAG_ALLOW_EMPTY)
         END_SUB_GROUP()

         END_GROUP()

         START_GROUP("Shader Options")
         START_SUB_GROUP("State")
         CONFIG_BOOL(g_settings.video.shader_enable,        "video_shader_enable",        "Enable Shaders",             shader_enable, GROUP_NAME, SUBGROUP_NAME)
         CONFIG_PATH(g_settings.video.shader_path,          "video_shader",               "Shader",                     "", GROUP_NAME, SUBGROUP_NAME)       WITH_FLAGS(SD_FLAG_ALLOW_EMPTY)
         END_SUB_GROUP()
         END_GROUP()

         START_GROUP("Font Options")
         START_SUB_GROUP("Messages")
         CONFIG_PATH(g_settings.video.font_path,            "video_font_path",            "Font Path",                  "", GROUP_NAME, SUBGROUP_NAME)       WITH_FLAGS(SD_FLAG_ALLOW_EMPTY)
         CONFIG_FLOAT(g_settings.video.font_size,           "video_font_size",            "OSD Font Size",              font_size, GROUP_NAME, SUBGROUP_NAME)
         CONFIG_BOOL(g_settings.video.font_enable,          "video_font_enable",          "OSD Font Enable",            font_enable, GROUP_NAME, SUBGROUP_NAME)
         CONFIG_FLOAT(g_settings.video.msg_pos_x,           "video_message_pos_x",        "Message X Position",         message_pos_offset_x, GROUP_NAME, SUBGROUP_NAME)
         CONFIG_FLOAT(g_settings.video.msg_pos_y,           "video_message_pos_y",        "Message Y Position",         message_pos_offset_y, GROUP_NAME, SUBGROUP_NAME)
         /* message color */
         END_SUB_GROUP()
         END_GROUP()

         /*********/
         /* AUDIO */
         /*********/
         START_GROUP("Audio Options")
         START_SUB_GROUP("State")
         CONFIG_BOOL(g_settings.audio.enable,               "audio_enable",               "Audio Enable",                     audio_enable, GROUP_NAME, SUBGROUP_NAME)
         CONFIG_BOOL(g_extern.audio_data.mute,              "audio_mute",                 "Audio Mute",                 false, GROUP_NAME, SUBGROUP_NAME)
         CONFIG_FLOAT(g_settings.audio.volume,              "audio_volume",               "Volume Level",               audio_volume, GROUP_NAME, SUBGROUP_NAME)
         END_SUB_GROUP()

         START_SUB_GROUP("Synchronization")
         CONFIG_BOOL(g_settings.audio.sync,                 "audio_sync",                 "Enable Sync",                audio_sync, GROUP_NAME, SUBGROUP_NAME)
         CONFIG_UINT(g_settings.audio.latency,              "audio_latency",              "Latency",                    g_defaults.settings.out_latency ? g_defaults.settings.out_latency : out_latency, GROUP_NAME, SUBGROUP_NAME)
         CONFIG_BOOL(g_settings.audio.rate_control,         "audio_rate_control",         "Enable Rate Control",        rate_control, GROUP_NAME, SUBGROUP_NAME)
         CONFIG_FLOAT(g_settings.audio.rate_control_delta,  "audio_rate_control_delta",   "Rate Control Delta",         rate_control_delta, GROUP_NAME, SUBGROUP_NAME)
         CONFIG_UINT(g_settings.audio.block_frames,         "audio_block_frames",         "Block Frames",               0, GROUP_NAME, SUBGROUP_NAME)
         END_SUB_GROUP()

         START_SUB_GROUP("Miscellaneous")
         CONFIG_STRING(g_settings.audio.device,             "audio_device",               "Device",                     "", GROUP_NAME, SUBGROUP_NAME)
         CONFIG_UINT(g_settings.audio.out_rate,             "audio_out_rate",             "Ouput Rate",                 out_rate, GROUP_NAME, SUBGROUP_NAME)
         CONFIG_PATH(g_settings.audio.dsp_plugin,           "audio_dsp_plugin",           "DSP Plugin",                 "", GROUP_NAME, SUBGROUP_NAME)          WITH_FLAGS(SD_FLAG_ALLOW_EMPTY)
         END_SUB_GROUP()
         END_GROUP()

         /*********/
         /* INPUT */
         /*********/
         START_GROUP("Input Options")
         START_SUB_GROUP("State")
         CONFIG_BOOL(g_settings.input.autodetect_enable,    "input_autodetect_enable",    "Autodetect Enable",   input_autodetect_enable, GROUP_NAME, SUBGROUP_NAME)
         END_SUB_GROUP()

         START_SUB_GROUP("Joypad Mapping")
         //TODO: input_libretro_device_p%u
         CONFIG_INT(g_settings.input.joypad_map[0],         "input_player1_joypad_index", "Player 1 Pad Index",         0, GROUP_NAME, SUBGROUP_NAME)
         CONFIG_INT(g_settings.input.joypad_map[1],         "input_player2_joypad_index", "Player 2 Pad Index",         1, GROUP_NAME, SUBGROUP_NAME)
         CONFIG_INT(g_settings.input.joypad_map[2],         "input_player3_joypad_index", "Player 3 Pad Index",         2, GROUP_NAME, SUBGROUP_NAME)
         CONFIG_INT(g_settings.input.joypad_map[3],         "input_player4_joypad_index", "Player 4 Pad Index",         3, GROUP_NAME, SUBGROUP_NAME)
         CONFIG_INT(g_settings.input.joypad_map[4],         "input_player5_joypad_index", "Player 5 Pad Index",         4, GROUP_NAME, SUBGROUP_NAME)
         END_SUB_GROUP()

         START_SUB_GROUP("Turbo/Deadzone")
         CONFIG_FLOAT(g_settings.input.axis_threshold,      "input_axis_threshold",       "Axis Deadzone",              axis_threshold, GROUP_NAME, SUBGROUP_NAME)
         CONFIG_UINT(g_settings.input.turbo_period,         "input_turbo_period",         "Turbo Period",               turbo_period, GROUP_NAME, SUBGROUP_NAME)
         CONFIG_UINT(g_settings.input.turbo_duty_cycle,     "input_duty_cycle",           "Duty Cycle",                 turbo_duty_cycle, GROUP_NAME, SUBGROUP_NAME)
         END_SUB_GROUP()
       
       // The second argument to config bind is 1 based for players and 0 only for meta keys
       START_SUB_GROUP("Meta Keys")
       for (i = 0; i != RARCH_BIND_LIST_END; i ++)
           if (input_config_bind_map[i].meta)
           {
               const struct input_bind_map* bind = &input_config_bind_map[i];
               CONFIG_BIND(g_settings.input.binds[0][i], 0, bind->base, bind->desc, &retro_keybinds_1[i], GROUP_NAME, SUBGROUP_NAME)
           }
       END_SUB_GROUP()
       
       for (player = 0; player < MAX_PLAYERS; player ++)
       {
           char buffer[32];
           const struct retro_keybind* const defaults = (player == 0) ? retro_keybinds_1 : retro_keybinds_rest;
           
           snprintf(buffer, 32, "Player %d", player + 1);
           START_SUB_GROUP(strdup(buffer))
           for (i = 0; i != RARCH_BIND_LIST_END; i ++)
           {
               if (!input_config_bind_map[i].meta)
               {
                   const struct input_bind_map* bind = (const struct input_bind_map*)&input_config_bind_map[i];
                   CONFIG_BIND(g_settings.input.binds[player][i], player + 1, bind->base, bind->desc, &defaults[i], GROUP_NAME, SUBGROUP_NAME)
               }
           }
           END_SUB_GROUP()
       }
         START_SUB_GROUP("Miscellaneous")
         CONFIG_BOOL(g_settings.input.netplay_client_swap_input, "netplay_client_swap_input", "Swap Netplay Input",     netplay_client_swap_input, GROUP_NAME, SUBGROUP_NAME)
         END_SUB_GROUP()
         END_GROUP()

#ifdef HAVE_OVERLAY
         START_GROUP("Overlay Options")
         START_SUB_GROUP("State")
         CONFIG_BOOL(g_settings.input.overlay_enable,            "input_overlay_enable",            "Overlay Enable",        default_overlay_enable, GROUP_NAME, SUBGROUP_NAME)
         CONFIG_PATH(g_settings.input.overlay,              "input_overlay",              "Overlay Preset",              "", GROUP_NAME, SUBGROUP_NAME) WITH_FLAGS(SD_FLAG_ALLOW_EMPTY) WITH_VALUES("cfg")
         CONFIG_FLOAT(g_settings.input.overlay_opacity,     "input_overlay_opacity",      "Overlay Opacity",            0.7f, GROUP_NAME, SUBGROUP_NAME) WITH_RANGE(0, 1)
         CONFIG_FLOAT(g_settings.input.overlay_scale,       "input_overlay_scale",        "Overlay Scale",              1.0f, GROUP_NAME, SUBGROUP_NAME)
         END_SUB_GROUP()
         END_GROUP()
#endif

         /*********/
         /* PATHS */
         /*********/
         START_GROUP("Path Options")
         START_SUB_GROUP("Paths")
#ifdef HAVE_MENU
         CONFIG_PATH(g_settings.menu_content_directory,     "rgui_browser_directory",     "Content Directory",          "", GROUP_NAME, SUBGROUP_NAME)                WITH_FLAGS(SD_FLAG_ALLOW_EMPTY | SD_FLAG_PATH_DIR)
         CONFIG_PATH(g_settings.assets_directory,           "assets_directory",           "Assets Directory",           "", GROUP_NAME, SUBGROUP_NAME)                WITH_FLAGS(SD_FLAG_ALLOW_EMPTY | SD_FLAG_PATH_DIR)
         CONFIG_PATH(g_settings.menu_config_directory,      "rgui_config_directory",      "Config Directory",           "", GROUP_NAME, SUBGROUP_NAME)                WITH_FLAGS(SD_FLAG_ALLOW_EMPTY | SD_FLAG_PATH_DIR)
         CONFIG_BOOL(g_settings.menu_show_start_screen,     "rgui_show_start_screen",     "Show Start Screen",          menu_show_start_screen, GROUP_NAME, SUBGROUP_NAME)
#endif
         CONFIG_PATH(g_settings.libretro,                   "libretro_path",              "Libretro Path",              "", GROUP_NAME, SUBGROUP_NAME)                WITH_FLAGS(SD_FLAG_ALLOW_EMPTY)
         CONFIG_PATH(g_settings.libretro_info_path,         "libretro_info_path",         "Core Info Directory",        default_libretro_info_path, GROUP_NAME, SUBGROUP_NAME)   WITH_FLAGS(SD_FLAG_ALLOW_EMPTY | SD_FLAG_PATH_DIR)
         CONFIG_PATH(g_settings.core_options_path,          "core_options_path",          "Core Options Path",          "", "Paths", SUBGROUP_NAME)                WITH_FLAGS(SD_FLAG_ALLOW_EMPTY)
         CONFIG_PATH(g_settings.cheat_database,             "cheat_database_path",        "Cheat Database",             "", "Paths", SUBGROUP_NAME)                WITH_FLAGS(SD_FLAG_ALLOW_EMPTY)
         CONFIG_PATH(g_settings.cheat_settings_path,        "cheat_settings_path",        "Cheat Settings",             "", GROUP_NAME, SUBGROUP_NAME)                WITH_FLAGS(SD_FLAG_ALLOW_EMPTY)
         CONFIG_PATH(g_settings.game_history_path,          "game_history_path",          "Content History Path",       "", GROUP_NAME, SUBGROUP_NAME)                WITH_FLAGS(SD_FLAG_ALLOW_EMPTY)
         CONFIG_UINT(g_settings.game_history_size,          "game_history_size",          "Content History Size",       game_history_size, GROUP_NAME, SUBGROUP_NAME)
         CONFIG_PATH(g_settings.video.shader_dir,           "video_shader_dir",           "Shader Directory",           default_shader_dir, GROUP_NAME, SUBGROUP_NAME)  WITH_FLAGS(SD_FLAG_ALLOW_EMPTY | SD_FLAG_PATH_DIR)

#ifdef HAVE_OVERLAY
         CONFIG_PATH(g_extern.overlay_dir,                  "overlay_directory",          "Overlay Directory",          default_overlay_dir, GROUP_NAME, SUBGROUP_NAME) WITH_FLAGS(SD_FLAG_ALLOW_EMPTY | SD_FLAG_PATH_DIR)
#endif
         CONFIG_PATH(g_settings.screenshot_directory,       "screenshot_directory",       "Screenshot Directory",       "", GROUP_NAME, SUBGROUP_NAME)                WITH_FLAGS(SD_FLAG_ALLOW_EMPTY | SD_FLAG_PATH_DIR)
         CONFIG_PATH(g_settings.input.autoconfig_dir,       "joypad_autoconfig_dir",      "Joypad Autoconfig Directory", "", GROUP_NAME, SUBGROUP_NAME)          WITH_FLAGS(SD_FLAG_ALLOW_EMPTY | SD_FLAG_PATH_DIR)
         // savefile_directory
         // savestate_directory
         // system_directory
         END_SUB_GROUP()
         END_GROUP()
   }

   return list;
}
