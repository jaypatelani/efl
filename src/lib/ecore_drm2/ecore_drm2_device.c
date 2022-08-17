#include "ecore_drm2_private.h"

/* local functions */
static Eina_Bool
_ecore_drm2_device_modeset_capable_get(int fd)
{
   Eina_Bool ret = EINA_TRUE;
   drmModeRes *res;

   res = sym_drmModeGetResources(fd);
   if (!res) return EINA_FALSE;

   if ((res->count_crtcs <= 0) || (res->count_connectors <= 0) ||
       (res->count_encoders <= 0))
     ret = EINA_FALSE;

   sym_drmModeFreeResources(res);

   return ret;
}

static const char *
_ecore_drm2_device_path_get(Elput_Manager *em, const char *seat)
{
   Eina_List *devs, *l;
   const char *denv = NULL, *dev = NULL, *chosen = NULL, *ret = NULL;
   Eina_Bool found = EINA_FALSE, ms = EINA_FALSE;

   EINA_SAFETY_ON_NULL_RETURN_VAL(em, NULL);
   EINA_SAFETY_ON_NULL_RETURN_VAL(seat, NULL);

   denv = getenv("ECORE_DRM2_CARD");
   if (denv)
     devs = eeze_udev_find_by_subsystem_sysname("drm", denv);
   else
     devs = eeze_udev_find_by_subsystem_sysname("drm", "card[0-9]*");

   if (!devs) return NULL;

   EINA_LIST_FOREACH(devs, l, dev)
     {
        int fd = -1;
        const char *dpath, *dseat, *dparent;

        dpath = eeze_udev_syspath_get_devpath(dev);
        if (!dpath) continue;

        dseat = eeze_udev_syspath_get_property(dev, "ID_SEAT");
        if (!dseat) dseat = eina_stringshare_add("seat0");

        if (strcmp(seat, dseat)) goto cont;

        fd = elput_manager_open(em, dpath, -1);
        if (fd < 0) goto cont;

        ms = _ecore_drm2_device_modeset_capable_get(fd);
        elput_manager_close(em, fd);
        if (!ms) goto cont;

        chosen = dev;

        dparent = eeze_udev_syspath_get_parent_filtered(dev, "pci", NULL);
        if (dparent)
          {
             const char *id;

             id = eeze_udev_syspath_get_sysattr(dparent, "boot_vga");
             if (id)
               {
                  if (!strcmp(id, "1")) found = EINA_TRUE;
                  eina_stringshare_del(id);
               }

             eina_stringshare_del(dparent);
          }

cont:
        eina_stringshare_del(dpath);
        eina_stringshare_del(dseat);
        if (found) break;
     }

   if (chosen)
     ret = eeze_udev_syspath_get_devpath(chosen);

   EINA_LIST_FREE(devs, dev)
     eina_stringshare_del(dev);

   return ret;
}

/* API functions */
EAPI Ecore_Drm2_Device *
ecore_drm2_device_open(const char *seat, unsigned int tty)
{
   const char *path;
   Ecore_Drm2_Device *dev;

   /* try to allocate space for return structure */
   dev = calloc(1, sizeof(Ecore_Drm2_Device));
   if (!dev) return NULL;

   /* try to connect to Elput manager */
   dev->em = elput_manager_connect(seat, tty);
   if (!dev->em)
     {
        ERR("Could not connect to input manager");
        goto man_err;
     }

   /* try to get drm device path */
   path = _ecore_drm2_device_path_get(dev->em, seat);
   if (!path)
     {
        ERR("Could not find drm device on seat %s", seat);
        goto path_err;
     }

   /* try to open this device */
   dev->fd = elput_manager_open(dev->em, path, -1);
   if (dev->fd < 0)
     {
        ERR("Could not open drm device %s", path);
        goto open_err;
     }

   /* TODO: elput_input_init, check atomic capable, etc */

   return dev;

open_err:
   eina_stringshare_del(path);
path_err:
   elput_manager_disconnect(dev->em);
man_err:
   free(dev);
   return NULL;
}

EAPI void
ecore_drm2_device_close(Ecore_Drm2_Device *dev)
{
   EINA_SAFETY_ON_NULL_RETURN(dev);

   /* TODO: elput_input_shutdown */

   elput_manager_close(dev->em, dev->fd);
   elput_manager_disconnect(dev->em);

   /* TODO: atomic state free */

   free(dev);
}
