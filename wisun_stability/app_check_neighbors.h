#include <stdio.h>
#include <assert.h>
#include "cmsis_os2.h"
#include "sl_cmsis_os2_common.h"
#include "sl_wisun_api.h"


char *  _neighbor_info_str(sl_wisun_neighbor_info_t neighbor_info, uint8_t index, char *tag);
uint8_t app_get_neighbor_info(sl_wisun_neighbor_type_t neighbor_type,
                                                 uint8_t *index,
                                                 char *tag,
                                                 sl_wisun_neighbor_info_t * neighbor_info);
char * app_parent_info_str(void);
char * app_child_info_str(uint8_t index);
char * app_neighbor_info_str(uint8_t index);
