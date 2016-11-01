#include "ecore_evas_wayland_private.h"

#ifdef BUILD_ECORE_EVAS_WAYLAND_SHM
# include <Evas_Engine_Wayland.h>
# include <stdlib.h>
# include <string.h>
# include <unistd.h>
# include <sys/types.h>
# include <sys/mman.h>

#ifdef EAPI
# undef EAPI
#endif

#ifdef _WIN32
# ifdef DLL_EXPORT
#  define EAPI __declspec(dllexport)
# else
#  define EAPI
# endif /* ! DLL_EXPORT */
#else
# ifdef __GNUC__
#  if __GNUC__ >= 4
#   define EAPI __attribute__ ((visibility("default")))
#  else
#   define EAPI
#  endif
# else
#  define EAPI
# endif
#endif /* ! _WIN32 */

static Ecore_Evas_Engine_Func _ecore_wl_engine_func = 
{
   _ecore_evas_wl_common_free,
   _ecore_evas_wl_common_callback_resize_set,
   _ecore_evas_wl_common_callback_move_set,
   NULL, 
   NULL,
   _ecore_evas_wl_common_callback_delete_request_set,
   NULL,
   _ecore_evas_wl_common_callback_focus_in_set,
   _ecore_evas_wl_common_callback_focus_out_set,
   _ecore_evas_wl_common_callback_mouse_in_set,
   _ecore_evas_wl_common_callback_mouse_out_set,
   NULL, // sticky_set
   NULL, // unsticky_set
   NULL, // pre_render_set
   NULL, // post_render_set
   _ecore_evas_wl_common_move,
   NULL, // managed_move
   _ecore_evas_wl_common_resize,
   _ecore_evas_wl_common_move_resize,
   _ecore_evas_wl_common_rotation_set,
   NULL, // shaped_set
   _ecore_evas_wl_common_show,
   _ecore_evas_wl_common_hide,
   _ecore_evas_wl_common_raise,
   NULL, // lower
   NULL, // activate
   _ecore_evas_wl_common_title_set,
   _ecore_evas_wl_common_name_class_set,
   _ecore_evas_wl_common_size_min_set,
   _ecore_evas_wl_common_size_max_set,
   _ecore_evas_wl_common_size_base_set,
   _ecore_evas_wl_common_size_step_set,
   _ecore_evas_wl_common_object_cursor_set,
   _ecore_evas_wl_common_object_cursor_unset,
   _ecore_evas_wl_common_layer_set,
   NULL, // focus set
   _ecore_evas_wl_common_iconified_set,
   _ecore_evas_wl_common_borderless_set,
   NULL, // override set
   _ecore_evas_wl_common_maximized_set,
   _ecore_evas_wl_common_fullscreen_set,
   NULL, // func avoid_damage set
   _ecore_evas_wl_common_withdrawn_set,
   NULL, // func sticky set
   _ecore_evas_wl_common_ignore_events_set,
   _ecore_evas_wl_common_alpha_set,
   _ecore_evas_wl_common_transparent_set,
   NULL, // func profiles set
   NULL, // func profile set
   NULL, // window group set
   _ecore_evas_wl_common_aspect_set,
   NULL, // urgent set
   NULL, // modal set
   NULL, // demand attention set
   NULL, // focus skip set
   NULL, //_ecore_evas_wl_common_render,
   _ecore_evas_wl_common_screen_geometry_get,
   _ecore_evas_wl_common_screen_dpi_get,
   NULL, // func msg parent send
   NULL, // func msg send

   _ecore_evas_wl_common_pointer_xy_get,
   NULL, // pointer_warp

   NULL, // wm_rot_preferred_rotation_set
   NULL, // wm_rot_available_rotations_set
   NULL, // wm_rot_manual_rotation_done_set
   NULL, // wm_rot_manual_rotation_done

   NULL, // aux_hints_set

   NULL, // fn_animator_register
   NULL, // fn_animator_unregister

   NULL, // fn_evas_changed
};

/* external variables */

static Eina_Bool
_ee_cb_sync_done(void *data, int type EINA_UNUSED, void *event EINA_UNUSED)
{
   Ecore_Evas *ee;
   Evas_Engine_Info_Wayland *einfo;
   Ecore_Evas_Engine_Wl_Data *wdata;

   ee = data;
   wdata = ee->engine.data;
   if (wdata->sync_done) return ECORE_CALLBACK_PASS_ON;
   wdata->sync_done = EINA_TRUE;

   if ((einfo = (Evas_Engine_Info_Wayland *)evas_engine_info_get(ee->evas)))
     {
        ecore_evas_manual_render_set(ee, 0);
        einfo->info.wl_display = ecore_wl2_display_get(wdata->display);
        einfo->info.wl_dmabuf = ecore_wl2_display_dmabuf_get(wdata->display);
        einfo->info.wl_shm = ecore_wl2_display_shm_get(wdata->display);
        einfo->info.compositor_version = ecore_wl2_display_compositor_version_get(wdata->display);
        einfo->info.destination_alpha = EINA_TRUE;
        einfo->info.rotation = ee->rotation;
        einfo->info.wl_surface = ecore_wl2_window_surface_get(wdata->win);

        if (!evas_engine_info_set(ee->evas, (Evas_Engine_Info *)einfo))
          {
             ERR("Failed to set Evas Engine Info for '%s'", ee->driver);
          }
     }
   else
     {
        ERR("Failed to get Evas Engine Info for '%s'", ee->driver);
     }

   if (wdata->defer_show)
     {
        int fw, fh;

        wdata->defer_show = EINA_FALSE;

        ecore_wl2_window_show(wdata->win);
        ecore_wl2_window_alpha_set(wdata->win, ee->alpha);
        ecore_wl2_window_transparent_set(wdata->win, ee->transparent);

        evas_output_framespace_get(ee->evas, NULL, NULL, &fw, &fh);

        if (wdata->win)
          evas_damage_rectangle_add(ee->evas, 0, 0, ee->w + fw, ee->h + fh);

        if (wdata->frame)
          {
             evas_object_show(wdata->frame);
             evas_object_resize(wdata->frame, ee->w + fw, ee->h + fh);
          }

        ee->prop.withdrawn = EINA_FALSE;
        if (ee->func.fn_state_change) ee->func.fn_state_change(ee);

        if (!ee->visible)
          {
             ee->visible = 1;
             ee->should_be_visible = 1;
             ee->draw_ok = EINA_TRUE;
             if (ee->func.fn_show) ee->func.fn_show(ee);
          }
     }

   return ECORE_CALLBACK_PASS_ON;
}

/* external functions */
EAPI Ecore_Evas *
ecore_evas_wayland_shm_new_internal(const char *disp_name, unsigned int parent, int x, int y, int w, int h, Eina_Bool frame)
{
   Ecore_Wl2_Display *ewd;
   Ecore_Wl2_Window *p = NULL;
   Evas_Engine_Info_Wayland *einfo;
   Ecore_Evas_Engine_Wl_Data *wdata;
   Ecore_Evas_Interface_Wayland *iface;
   Ecore_Evas *ee;
   int method = 0;
   int fx = 0, fy = 0, fw = 0, fh = 0;

   LOGFN(__FILE__, __LINE__, __FUNCTION__);

   if (!(method = evas_render_method_lookup("wayland_shm")))
     {
        ERR("Render method lookup failed for Wayland_Shm");
        return NULL;
     }

   if (!ecore_wl2_init())
     {
        ERR("Failed to initialize Ecore_Wl2");
        return NULL;
     }

   ewd = ecore_wl2_display_connect(disp_name);
   if (!ewd)
     {
        ERR("Failed to connect to Wayland Display %s", disp_name);
        goto conn_err;
     }

   if (!(ee = calloc(1, sizeof(Ecore_Evas))))
     {
        ERR("Failed to allocate Ecore_Evas");
        goto ee_err;
     }

   if (!(wdata = calloc(1, sizeof(Ecore_Evas_Engine_Wl_Data))))
     {
	ERR("Failed to allocate Ecore_Evas_Engine_Wl_Data");
	free(ee);
	goto ee_err;
     }

   ECORE_MAGIC_SET(ee, ECORE_MAGIC_EVAS);

   _ecore_evas_wl_common_init();

   ee->engine.func = (Ecore_Evas_Engine_Func *)&_ecore_wl_engine_func;
   ee->engine.data = wdata;

   iface = _ecore_evas_wl_interface_new();
   ee->engine.ifaces = eina_list_append(ee->engine.ifaces, iface);

   ee->driver = "wayland_shm";
   if (disp_name) ee->name = strdup(disp_name);

   if (w < 1) w = 1;
   if (h < 1) h = 1;

   ee->x = x;
   ee->y = y;
   ee->w = w;
   ee->h = h;
   ee->req.x = ee->x;
   ee->req.y = ee->y;
   ee->req.w = ee->w;
   ee->req.h = ee->h;
   ee->rotation = 0;
   ee->prop.max.w = 32767;
   ee->prop.max.h = 32767;
   ee->prop.layer = 4;
   ee->prop.request_pos = EINA_FALSE;
   ee->prop.sticky = EINA_FALSE;
   ee->prop.draw_frame = frame;
   ee->prop.withdrawn = EINA_TRUE;
   ee->alpha = EINA_FALSE;

   if (getenv("ECORE_EVAS_FORCE_SYNC_RENDER"))
     ee->can_async_render = 0;
   else
     ee->can_async_render = 1;

   /* frame offset and size */
   if (ee->prop.draw_frame)
     {
        fx = 4;
        fy = 18;
        fw = 8;
        fh = 22;
     }

   if (parent)
     {
        p = ecore_wl2_display_window_find(ewd, parent);
        ee->alpha = ecore_wl2_window_alpha_get(p);
     }

   wdata->sync_done = EINA_FALSE;
   wdata->parent = p;
   wdata->display = ewd;
   wdata->win = ecore_wl2_window_new(ewd, p, x, y, w + fw, h + fh);
   ee->prop.window = ecore_wl2_window_id_get(wdata->win);

   ee->evas = evas_new();
   evas_data_attach_set(ee->evas, ee);
   evas_output_method_set(ee->evas, method);
   evas_output_size_set(ee->evas, ee->w + fw, ee->h + fh);
   evas_output_viewport_set(ee->evas, 0, 0, ee->w + fw, ee->h + fh);

   if (ee->can_async_render)
     evas_event_callback_add(ee->evas, EVAS_CALLBACK_RENDER_POST,
			     _ecore_evas_wl_common_render_updates, ee);

   evas_event_callback_add(ee->evas, EVAS_CALLBACK_RENDER_FLUSH_PRE,
                           _ecore_evas_wl_common_render_flush_pre, ee);

   /* FIXME: This needs to be set based on theme & scale */
   if (ee->prop.draw_frame)
     evas_output_framespace_set(ee->evas, fx, fy, fw, fh);

   if (ewd->sync_done)
     {
        wdata->sync_done = EINA_TRUE;
        if ((einfo = (Evas_Engine_Info_Wayland *)evas_engine_info_get(ee->evas)))
          {
             einfo->info.wl_display = ecore_wl2_display_get(ewd);
             einfo->info.wl_dmabuf = ecore_wl2_display_dmabuf_get(ewd);
             einfo->info.wl_shm = ecore_wl2_display_shm_get(ewd);
             einfo->info.destination_alpha = EINA_TRUE;
             einfo->info.rotation = ee->rotation;
             einfo->info.wl_surface = ecore_wl2_window_surface_get(wdata->win);
             einfo->info.compositor_version = ecore_wl2_display_compositor_version_get(ewd);

             if (!evas_engine_info_set(ee->evas, (Evas_Engine_Info *)einfo))
               {
                  ERR("Failed to set Evas Engine Info for '%s'", ee->driver);
                  goto err;
               }
          }
        else
          {
             ERR("Failed to get Evas Engine Info for '%s'", ee->driver);
             goto err;
          }
     }

   /* ecore_wl2_display_animator_source_set(ewd, ECORE_ANIMATOR_SOURCE_CUSTOM); */

   ecore_evas_callback_pre_free_set(ee, _ecore_evas_wl_common_pre_free);

   if (ee->prop.draw_frame)
     {
        wdata->frame = _ecore_evas_wl_common_frame_add(ee->evas);
        _ecore_evas_wl_common_frame_border_size_set(wdata->frame, fx, fy, fw, fh);
        evas_object_move(wdata->frame, -fx, -fy);
        evas_object_layer_set(wdata->frame, EVAS_LAYER_MAX - 1);
     }

   ee->engine.func->fn_render = _ecore_evas_wl_common_render;

   _ecore_evas_register(ee);
   ecore_evas_input_event_register(ee);

   ecore_event_window_register(ee->prop.window, ee, ee->evas, 
                               (Ecore_Event_Mouse_Move_Cb)_ecore_evas_mouse_move_process, 
                               (Ecore_Event_Multi_Move_Cb)_ecore_evas_mouse_multi_move_process, 
                               (Ecore_Event_Multi_Down_Cb)_ecore_evas_mouse_multi_down_process, 
                               (Ecore_Event_Multi_Up_Cb)_ecore_evas_mouse_multi_up_process);
   _ecore_event_window_direct_cb_set(ee->prop.window,
                                     _ecore_evas_input_direct_cb);

   wdata->sync_handler =
     ecore_event_handler_add(ECORE_WL2_EVENT_SYNC_DONE, _ee_cb_sync_done, ee);

   ee_list = eina_list_append(ee_list, ee);

   return ee;

err:
   ecore_evas_free(ee);
ee_err:
   ecore_wl2_display_disconnect(ewd);
conn_err:
   ecore_wl2_shutdown();
   return NULL;
}

void 
_ecore_evas_wayland_shm_resize(Ecore_Evas *ee, int location)
{
   Ecore_Evas_Engine_Wl_Data *wdata;

   LOGFN(__FILE__, __LINE__, __FUNCTION__);

   if (!ee) return;
   wdata = ee->engine.data;
   if (wdata->win) 
     {
        _ecore_evas_wayland_shm_resize_edge_set(ee, location);

        if (ECORE_EVAS_PORTRAIT(ee))
          ecore_wl2_window_resize(wdata->win, ee->w, ee->h, location);
        else
          ecore_wl2_window_resize(wdata->win, ee->h, ee->w, location);
     }
}

void 
_ecore_evas_wayland_shm_resize_edge_set(Ecore_Evas *ee, int edge)
{
   Evas_Engine_Info_Wayland *einfo;

   if ((einfo = (Evas_Engine_Info_Wayland *)evas_engine_info_get(ee->evas)))
     einfo->info.edges = edge;
}

#endif
