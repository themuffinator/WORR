#include "shared/shared.h"
#include "refresh/refresh.h"
#include "refresh/images.h"
#include "refresh/models.h"

int registration_sequence;
refcfg_t r_config;

void (*IMG_Load)(image_t *image, byte *pic);
void (*IMG_Unload)(image_t *image);
void (*IMG_ReadPixels)(screenshot_t *s);
void (*IMG_ReadPixelsHDR)(screenshot_t *s);

int (*MOD_LoadMD2)(model_t *model, const void *rawdata, size_t length, const char* mod_name);
int (*MOD_LoadMD3)(model_t *model, const void *rawdata, size_t length, const char* mod_name);
int (*MOD_LoadIQM)(model_t *model, const void *rawdata, size_t length, const char* mod_name);
void (*MOD_Reference)(model_t *model);
