#include "app_check_neighbors.h"

extern char json_string[];
char tag[5];

char * _neighbor_info_str(sl_wisun_neighbor_info_t neighbor_info, uint8_t index, char *tag) {
  snprintf(json_string, 1024,
  "\"tag\":%s,\n" \
  "\"index\":%d,\n" \
  "\"type\":%ld,\n" \
  "\"lifetime\":%ld,\n" \
  "\"mac_tx_count\":%ld,\n" \
  "\"mac_tx_failed_count\":%ld,\n" \
  "\"mac_tx_ms_count\":%ld,\n" \
  "\"mac_tx_ms_failed_count\":%ld,\n" \
  "\"rpl_rank\":%d,\n" \
  "\"etx\":%d,\n" \
  "\"rsl_in\":%d,\n" \
  "\"rsl_out\":%d,\n" \
  "\"is_lfn\":%d,\n",
  tag,
  index,
  neighbor_info.type,
  neighbor_info.lifetime,
  neighbor_info.mac_tx_count,
  neighbor_info.mac_tx_failed_count,
  neighbor_info.mac_tx_ms_count,
  neighbor_info.mac_tx_ms_failed_count,
  neighbor_info.rpl_rank,
  neighbor_info.etx,
  neighbor_info.rsl_in  -174,
  neighbor_info.rsl_out -174,
  neighbor_info.is_lfn
  );
  return json_string;
}

uint8_t app_get_neighbor_info(sl_wisun_neighbor_type_t neighbor_type,
                                                 uint8_t *index,
                                                 char *tag,
                                                 sl_wisun_neighbor_info_t * neighbor_info) {
  sl_status_t ret;
  uint8_t neighbor_count;
  uint8_t i;

  ret = sl_wisun_get_neighbor_count(&neighbor_count);
  if (ret) printf("[Failed: sl_wisun_get_neighbor_count() returned 0x%04x]\n", (uint16_t)ret);

  if (neighbor_count == 0) return 0;

  sl_wisun_mac_address_t neighbor_mac_addresses[neighbor_count];
  ret = sl_wisun_get_neighbors(&neighbor_count, neighbor_mac_addresses);
  if (ret) printf("[Failed: sl_wisun_get_neighbors() returned 0x%04x]\n", (uint16_t)ret);

  if (neighbor_type == SL_WISUN_NEIGHBOR_TYPE_PRIMARY_PARENT) {
    for (i = 0 ; i < neighbor_count; i++) {
      ret = sl_wisun_get_neighbor_info(&neighbor_mac_addresses[i], neighbor_info);
      if (neighbor_info->type == SL_WISUN_NEIGHBOR_TYPE_PRIMARY_PARENT) {
        *index = i;
        sprintf(tag, "%02x%02x", neighbor_mac_addresses[i].address[6],
                                 neighbor_mac_addresses[i].address[7]);
        return 1;
      }
    }
  } else {
    if (*index < neighbor_count) {
      sl_wisun_get_neighbor_info(&neighbor_mac_addresses[*index], neighbor_info);
      sprintf(tag, "%02x%02x", neighbor_mac_addresses[*index].address[6],
                               neighbor_mac_addresses[*index].address[7]);
      return 1;
    }
  }
  return 0;
}

char * app_parent_info_str(void) {
  sl_wisun_neighbor_info_t neighbor_info;
  uint8_t index;
  if (app_get_neighbor_info(SL_WISUN_NEIGHBOR_TYPE_PRIMARY_PARENT, &index, tag, &neighbor_info)) {
    return _neighbor_info_str(neighbor_info, index, tag);
  } else {
    snprintf(json_string, 1024, "\"no parent\":%d\n", 0);
    return json_string;
  }
}

char * app_child_info_str(uint8_t index) {
  sl_wisun_neighbor_info_t neighbor_info;
  if (app_get_neighbor_info(SL_WISUN_NEIGHBOR_TYPE_CHILD, &index, tag, &neighbor_info)) {
    return _neighbor_info_str(neighbor_info, index, tag);
  } else {
    return "";
  }
}

char * app_neighbor_info_str(uint8_t index) {
  sl_status_t ret;
  uint8_t neighbor_count;
  sl_wisun_neighbor_info_t neighbor_info;

  ret = sl_wisun_get_neighbor_count(&neighbor_count);
  if (ret) printf("[Failed: sl_wisun_get_neighbor_count() returned 0x%04x]\n", (uint16_t)ret);

  if (neighbor_count == 0)     return "";
  if (index >= neighbor_count) return "";

  sl_wisun_mac_address_t neighbor_mac_addresses[neighbor_count];
  ret = sl_wisun_get_neighbors(&neighbor_count, neighbor_mac_addresses);
  if (ret) printf("[Failed: sl_wisun_get_neighbors() returned 0x%04x]\n", (uint16_t)ret);

  ret = sl_wisun_get_neighbor_info(&neighbor_mac_addresses[index], &neighbor_info);
  sprintf(tag, "%02x%02x", neighbor_mac_addresses[index].address[6],
                           neighbor_mac_addresses[index].address[7]);

  return _neighbor_info_str(neighbor_info, index, tag);
}
