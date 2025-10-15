#include "cmsis_os2.h"
#include "app_wisun_multicast_ota.h"

uint32_t udp_index;
uint32_t udp_previous_index;
uint32_t udp_lost_count;
uint32_t udp_received_count;

#include "sl_memory_manager.h"
#include "sl_wisun_ota_dfu.h"
#include "btl_interface.h"
#include "printf.h"
#include "app_parameters.h"

#define CLEAR_NVM_NO   0
#define CLEAR_NVM_APP  1
#define CLEAR_NVM_FULL 2

extern char device_tag[];
//extern sl_wisun_ota_dfu_settings_t _settings;
extern const char * udp_ip_str;
/// Resent packet count
uint32_t _resent_count;
/// Received packet count
uint32_t _received_count;
/// Downloaded bytes
uint32_t _downl_bytes;

char  timestamp_string[20];
char* udp_string;

#define GBL_FILE_NAME_MAX_SIZE 100
#define OTA_TAG_MAX_SIZE       100
#define TIMESTAMP_STR_MAX_SIZE 100

char    gbl_file[GBL_FILE_NAME_MAX_SIZE];     // Expected GBL filename (set as for Unicast OTA)
char    gbl_filename[GBL_FILE_NAME_MAX_SIZE]; // GBL filename in received data
char    tx_timestamp_str[TIMESTAMP_STR_MAX_SIZE];
char    tag_str[OTA_TAG_MAX_SIZE];
char    expected_tag[OTA_TAG_MAX_SIZE];

uint32_t slot0_start_address = 0x12345678;
uint32_t udp_rx_total_count;
uint32_t chunk_index;
uint32_t data_offset;
#define INFO_STRING_LENGTH 1024
uint32_t  udp_data_len[MAX_CHUNKS];
uint32_t  udp_chunk_rx_count[MAX_CHUNKS];
uint32_t  missed_index[MAX_CHUNKS];
uint32_t  missed_rx_index[MAX_CHUNKS];

char    information_string[INFO_STRING_LENGTH];
BootloaderStorageInformation_t storage_info;
BootloaderStorageSlot_t slot0;

// safe append helper
#define APPEND(fmt, ...) do {                                            \
  size_t _u = strlen(information_string);                                 \
  if (_u < INFO_STRING_LENGTH - 1) {                                      \
    (void)snprintf(information_string + _u,                               \
                   INFO_STRING_LENGTH - _u, fmt, ##__VA_ARGS__);          \
  }                                                                       \
} while (0)

char* ota_multicast_info(void)
{
  int32_t  ret_val;
  const uint32_t BASE_CHUNK_BYTES = 1024U;   // slot addressing uses 1024-byte chunks
  uint32_t max_chunks       = 0;
  const char *stype = "UNKNOWN";

  information_string[0] = '\0';

  sl_wisun_ota_dfu_get_gbl_path(gbl_file, 100);

  sprintf(expected_tag, SL_BOARD_NAME);

  // Header: compile-time & runtime counters
  APPEND("expected_tag=%s | expected_gbl_file=%s | MULTICAST_OTA_STORE_IN_FLASH=%d | udp_rx_total=%lu\n",
         expected_tag,
         gbl_file,
         (int)MULTICAST_OTA_STORE_IN_FLASH,
         (unsigned long)udp_rx_total_count);

  // Bootloader storage info
  bootloader_getStorageInfo(&storage_info);

    switch (storage_info.storageType) {
      case SPIFLASH:       stype = "SPIFLASH";       break;
      case INTERNAL_FLASH: stype = "INTERNAL_FLASH"; break;
      case CUSTOM_STORAGE: stype = "CUSTOM_STORAGE"; break;
      default: break;
    }

    APPEND("storage_info: version=0x%lx caps=0x%lx type=%s(%lu) numSlots=%lu\n",
            (unsigned long)storage_info.version,
            (unsigned long)storage_info.capabilities,
            stype, (unsigned long)storage_info.storageType,
            (unsigned long)storage_info.numStorageSlots);


  ret_val = bootloader_getStorageSlotInfo(0, &slot0);
  if (ret_val != BOOTLOADER_OK) {
    APPEND("bootloader_getStorageSlotInfo(0) error: 0x%08lx\n",
           (unsigned long)ret_val);
    return information_string; // return what we have
  }

  // Capacity derived from slot0
  if (slot0.length >= BASE_CHUNK_BYTES) {
    max_chunks = (uint32_t)(slot0.length / BASE_CHUNK_BYTES);
  }

  // Bootloader storage summary
  APPEND("slot0.addr=0x%08lx | slot0.len=%lu\n",
         (unsigned long)slot0.address,
         (unsigned long)slot0.length);

  // Capacity view
  APPEND("capacity: chunk=%lu | max_chunks=%lu\n",
         (unsigned long)BASE_CHUNK_BYTES,
         (unsigned long)max_chunks);


  // Progress indices
  APPEND("progress: last_written_idx=%lu | last_rx_idx=%lu\n",
         (unsigned long)last_index(),
         (unsigned long)last_index_rx());

  return information_string;
}


void clear_ota_data() {
  int i;
  int32_t ret_val;
  _resent_count = 0;
  _received_count = 0;
  _downl_bytes = 0;

  sl_wisun_ota_dfu_set_notify_download_chunk(_downl_bytes);

  slot0_start_address = 0x12345678;

  udp_rx_total_count = 0;

  for (i=0; i<MAX_CHUNKS; i++) {
    udp_data_len[i]=0;
    udp_chunk_rx_count[i]=0;
    missed_index[i]= 0;
    missed_rx_index[i]=0;
  }
  printf("Chunks [0:%d] cleared\n", i-1);

  bootloader_getStorageInfo(&storage_info);
  printf("numStorageSlots: %ld\n", storage_info.numStorageSlots);
  assert(storage_info.numStorageSlots >= 1);
  assert(bootloader_getStorageSlotInfo(0, &slot0) == BOOTLOADER_OK);

  ret_val = bootloader_getStorageSlotInfo(0, &slot0);
  if (ret_val != BOOTLOADER_OK) {
    printf("bootloader_getStorageSlotInfo(0, &slot0) error: 0x%08lx\n", ret_val);
    return;
  }
  printf("slot0: address 0x%08lx, length %ld\n", slot0.address, slot0.length);

#if MULTICAST_OTA_STORE_IN_FLASH == 1
  ret_val = bootloader_eraseStorageSlot(0);
    if (ret_val != BOOTLOADER_OK) {
      printf("bootloader_eraseStorageSlot(0, &slot0) error: 0x%08lx\n", ret_val);
      return;
  }
  printf("slot0 erased\n");
#endif /* MULTICAST_OTA_STORE_IN_FLASH == 1 */
};

// Highest chunk index that has been received on UDP (independent of flash writes)
uint32_t last_index_rx(void) {
  uint32_t last = 0;
  for (uint32_t i = 1; i < MAX_CHUNKS; i++) {
    if (udp_chunk_rx_count[i] > 0) {
      last = i;
    }
  }
  return last;
}

// Build a compact list of missed chunks into an output array (RX-based)
// Returns the number of missed entries written to out[]
uint32_t list_missed_rx(uint32_t *out, uint32_t out_cap) {
  uint32_t n = 0;
  uint32_t last = last_index_rx();
  for (uint32_t i = 1; i <= last; i++) {
    if (udp_chunk_rx_count[i] == 0) {
      if (n < out_cap) out[n] = i;
      n++;
    }
  }
  return n;
}


// Mirrors show_missed_from_list(), but computed from udp_chunk_rx_count[] (RX activity)
// and includes udp_rx_total_count in the header.
sl_status_t show_missed_from_list_rx(void) {
  const uint32_t last_rx = last_index_rx();
  const uint32_t missed_cnt = list_missed_rx(missed_rx_index, MAX_CHUNKS);
  const uint32_t received_cnt = (last_rx > 0) ? (last_rx - missed_cnt) : 0;

  information_string[0] = '\0';

  if (last_rx == 0) {
    // nothing received yet from the network
    APPEND("[%s] rx_total=%lu | no UDP chunks received yet\n",
           device_tag, (unsigned long)udp_rx_total_count);
    printf("%s", information_string);
    return SL_STATUS_OK;
  }

  const double success = (100.0 * (double)received_cnt) / (double)last_rx;

  // Header
  APPEND("[%s] rx_total=%lu | received=%lu/%lu (%.2f%%) | missed=%lu",
         device_tag,
         (unsigned long)udp_rx_total_count,
         (unsigned long)received_cnt,
         (unsigned long)last_rx,
         success,
         (unsigned long)missed_cnt
         );

  // Missed list
  APPEND("missed:   ");
  for (uint32_t k = 0; k < missed_cnt; k++) {
    if (strlen(information_string) > INFO_STRING_LENGTH - 8) { APPEND("..."); break; }
    APPEND("%lu ", (unsigned long)missed_rx_index[k]);
  }
  APPEND("\n");

  printf("%s", information_string);
  return SL_STATUS_OK;
}

char* rx_chunks() {
  show_missed_from_list_rx();
  return information_string;
}

// Highest chunk index that has been received on UDP and written in flash/
uint32_t last_index() {
  uint32_t last_chunk_index = 0;
  for (chunk_index=1; chunk_index<MAX_CHUNKS; chunk_index++) {
    if (udp_data_len[chunk_index] != 0) {last_chunk_index = chunk_index;}
  }
  return last_chunk_index;
};

// Build a compact list of missed chunks into an output array (RX + Write in flash based)
uint32_t list_missed() {
  uint32_t missed_count = 0;
  uint32_t chunk_index;
  uint32_t last_chunk_index;

  last_chunk_index = last_index();

  for (chunk_index=1; chunk_index<last_chunk_index; chunk_index++) {
    if (udp_data_len[chunk_index] == 0) {
      missed_index[missed_count] = chunk_index;
      missed_count ++;
    }
  }

  return missed_count;
};

bool verify_image_in_flash() {
  int32_t count = 0;
  int32_t ret_val;
  bool continue_verification = true;
  while (continue_verification) {
    count++;
    ret_val = bootloader_verifyImage(0U, NULL);
    continue_verification = (ret_val == BOOTLOADER_ERROR_PARSE_CONTINUE);
    //printf("[%s] continue_verification = %d (count %ld)\n", device_tag, continue_verification, count);
  }
  if (ret_val == BOOTLOADER_OK) {
      printf("[%s] bootloader_verifyImage(0U, NULL) %ld 0x%04lx\n", device_tag, ret_val, ret_val);
  } else {
    printf("[%s] Verify image failed with code %ld 0x%04lx\n", device_tag, ret_val, ret_val);
    if (ret_val == BOOTLOADER_ERROR_PARSER_CRC) {
        printf("[%s] Verify image FAILED: Invalid checksum\n", device_tag);
    }
    if (ret_val == BOOTLOADER_ERROR_PARSER_UNKNOWN_TAG) {
        printf("[%s] Verify image FAILED: Unknown data type in image file (possible compressed file without support for compression in the bootloader)\n", device_tag);
    }
  }
  return (ret_val == BOOTLOADER_OK);
}

//Show missed packet = not RX or store in flash
sl_status_t show_missed_from_list() {
#if MULTICAST_OTA_STORE_IN_FLASH == 1
  uint32_t index;
  uint32_t missed_count;
  uint32_t last_chunk_index;
  uint32_t info_length;

  last_chunk_index = last_index();

  missed_count = list_missed();
  if (last_chunk_index) {
    printf("[%s] show_missed_from_list(): %ld missed chunks, %ld received chunks out of %ld chunks (%.2f%% success rate) from %s %s\n",
                   device_tag,
                   missed_count,
                   last_chunk_index - missed_count,
                   last_chunk_index,
                   100.0*(last_chunk_index - missed_count)/last_chunk_index,
                   gbl_file,
                   expected_tag
                   );

    snprintf(information_string, INFO_STRING_LENGTH, "[%s] %3ld/%3ld missed chunks ", device_tag, missed_count, last_chunk_index);
    info_length = strlen(information_string);
    for (index=0; index<missed_count; index++) {
      snprintf(information_string + info_length, INFO_STRING_LENGTH - info_length, "%3ld ", missed_index[index]);
      info_length = strlen(information_string);
    }
  } else {
    snprintf(information_string, INFO_STRING_LENGTH, "[%s] %ld missed chunks because %ld received from %s %s\n", device_tag, missed_count, last_chunk_index, gbl_file, expected_tag);
  }
#else
  snprintf(information_string, INFO_STRING_LENGTH, "MULTICAST_OTA_STORE_IN_FLASH disabled, missed_chunk not supported %s %s", gbl_file, expected_tag);
#endif /* MULTICAST_OTA_STORE_IN_FLASH == 1 */
  printf("%s\n", information_string);
  return SL_STATUS_OK;
};

void show_missed() {
  uint32_t chunk_index;
  uint32_t last_chunk_index;

  printf("show_missed()\n");
  last_chunk_index = last_index();

  for (chunk_index=1; chunk_index<last_chunk_index; chunk_index++) {
    if (udp_data_len[chunk_index] == 0) {
        printf("[%s] missed chunk[%4ld]\n", device_tag, chunk_index);
    }
  }

};

char* missed_chunks() {
  show_missed_from_list();
  return information_string;
}

void show_repeated() {
  uint32_t chunk_index;
  uint32_t last_chunk_index;

  printf("show_repeated()\n");
  last_chunk_index = last_index();

  for (chunk_index=1; chunk_index<last_chunk_index; chunk_index++) {
    if (udp_chunk_rx_count[chunk_index] > 1) {
      printf("[%s] received chunk[%4ld] %4ld times\n", device_tag, chunk_index, udp_chunk_rx_count[chunk_index]);
    }
  }
};

void setImageToBootload(int slot) {
  int32_t ret_val;
  ret_val = bootloader_setImageToBootload(slot);
  printf("[%s] bootloader_setImageToBootload(%d) %ld 0x%04lx\n", device_tag, slot, ret_val, ret_val);
}

uint8_t rebootAndInstall(uint32_t time_reboot_sec, uint8_t clear_nvm) {
  sl_status_t status;
  uint8_t ret = 0xff;
  if (last_index() > 1) {
      if (list_missed() == 0) {
          if (verify_image_in_flash()) {
              setImageToBootload(0);
              if (time_reboot_sec < 90){
                  printf("[%s] Reboot and install in %ld sec\n", device_tag, time_reboot_sec);
                  osDelay(time_reboot_sec*1000);

                  if (clear_nvm == CLEAR_NVM_APP){
                      printf("[%s] Clear NVM APP\n", device_tag);
                      if (delete_app_parameters() != SL_STATUS_OK){
                          return 5;
                      }
                  }
                  else if (clear_nvm == CLEAR_NVM_FULL){
                      printf("[%s] Clear NVM FULL\n", device_tag);
                      status = nvm3_eraseAll(nvm3_defaultHandle);
                      if (status != SL_STATUS_OK) {
                          printfBothTime("nvm3_eraseAll(nvm3_defaultHandle) returned 0x%04lX, (check sl_status.h)\n",
                                          status);
                          return 5;
                      } else {
                          printfBothTime("application parameters deleted\n");
                      }

                  }

                  printf("[%s] bootloader_rebootAndInstall()\n", device_tag);
                  bootloader_rebootAndInstall();
                  ret = 0;
              } else {
                  printf("[%s] Invalid time : %ld (must be < 90 sec)", device_tag, time_reboot_sec);
                  ret = 1;
              }
          } else {
              printf("[%s] verify_image_in_flash() failed: no reboot\n", device_tag);
              ret = 2;
          }
      } else {
          printf("[%s] There are missed chunks: no reboot\n", device_tag);
          ret = 3;
      }
  } else {
      printf("[%s] There are no chunks: no reboot\n", device_tag);
      ret = 4;
  }
  return ret;
}

int multicast_rx(char* udp_buff, uint32_t received_bytes) {
  int received = 0;
  int res;
#if MULTICAST_OTA_STORE_IN_FLASH == 1
  uint32_t time_reboot_sec;
#endif /* MULTICAST_OTA_STORE_IN_FLASH == 1 */
  uint32_t chunk_size;
  uint32_t start_address;
  uint32_t end_address;
  int32_t  ret_val;

  int last_chunk_index;
  char info_byte[4];
  char* data_buffer;
  info_byte[0] = 0x30;
  info_byte[1] = 0x31;
  info_byte[2] = 0x32;
  info_byte[3] = 0x33;

  sprintf(expected_tag, SL_BOARD_NAME);
  // NB:  slot0_start_address is set to 0x12345678 in clear_ota_data()
  if (slot0_start_address == 0x12345678) {
    ret_val = bootloader_getStorageSlotInfo(0, &slot0);
    sl_wisun_ota_dfu_get_gbl_path(gbl_file, 100);
    printf("expected gbl_filename %s\n", gbl_file);
    if (ret_val != BOOTLOADER_OK) {
      printf("bootloader_getStorageSlotInfo(0, &slot0) error: 0x%08lx\n", ret_val);
    }
    printf("slot0: address 0x%08lx, length %ld. It can accept max %ld chunks of 1024 bytes\n", slot0.address, slot0.length, slot0.length/1024);
    slot0_start_address = slot0.address;
  } else {
    sl_wisun_ota_dfu_get_gbl_path(gbl_file, 100);
  }

  res = sscanf(udp_buff, "OTA %s %ld %ld %s %s", gbl_filename, &chunk_index, &data_offset, tx_timestamp_str, tag_str);
  if (res == 5) {
    //Store OTA chunk has been received, do not check gbl file name or tag => metrics
    udp_chunk_rx_count[chunk_index]++;
    udp_rx_total_count ++;
    // check that the gbl_filename matches the expected file
    if (strcmp(gbl_filename, gbl_file) == 0) {
      // check that the tag matches SL_BOARD_NAME
      if (strcmp(tag_str, expected_tag) == 0) {
        if (chunk_index == 0) {
          ret_val = bootloader_getStorageSlotInfo(0, &slot0);
          if (ret_val != BOOTLOADER_OK) {
            printf("bootloader_getStorageSlotInfo(0, &slot0) error: 0x%08lx\n", ret_val);
          }
          printf("slot0: address 0x%08lx, length %ld. It can accept max %ld chunks of 1024 bytes\n", slot0.address, slot0.length, slot0.length/1024);
        }
        if (chunk_index < MAX_CHUNKS) { // chunk can be stored
          // Only store if there are data bytes in the received message
          if (received_bytes > data_offset) {
            chunk_size = received_bytes - data_offset;
            last_chunk_index = chunk_size - 1;
              start_address = 1024*(chunk_index-1);
              end_address   = start_address + last_chunk_index;
              // Only store no-duplicated chunks
              if (udp_data_len[chunk_index] == 0) {
                //printf("[%s] Calling bootloader_eraseWriteStorage(0, 0x%08lx, udp_buff, %ld) for chunk[%4ld]\n", device_tag, start_address, chunk_size, chunk_index);
                data_buffer = udp_buff + data_offset;
                info_byte[0] = data_buffer[0];
                info_byte[1] = data_buffer[1];
                info_byte[2] = data_buffer[chunk_size - 2];
                info_byte[3] = data_buffer[chunk_size - 1];

#if MULTICAST_OTA_STORE_IN_FLASH == 1
                 // Write bytes to flash
                ret_val = bootloader_writeStorage(0,
                                                  start_address,
                                                  (uint8_t *)data_buffer,
                                                  chunk_size);
                if (ret_val != BOOTLOADER_OK) {
                  printf("[%s] UDP Rx %4ld from %s (%4ld bytes): ERROR calling  bootloader_writeStorage() for chunk[%4ld] | %02x %02x ---(%4ld bytes)--- %02x %02x | [%6ld:%6ld]/[%08lx:%08lx] in Flash: 0x%04x\n",
                                  device_tag,
                                  udp_rx_total_count, udp_ip_str, received_bytes,
                                  chunk_index,
                                  info_byte[0],
                                  info_byte[1],
                                  chunk_size,
                                  info_byte[2],
                                  info_byte[3],
                                  start_address,
                                  end_address,
                                  start_address,
                                  end_address,
                                  (int)ret_val);
                } else {
                  udp_data_len[chunk_index] = chunk_size;
                  printfTime("[%s] UDP Rx %4ld from %s (%4ld bytes): %s chunk[%4ld] (count %2ld) | offset %4ld | tx at %-10s | tag %s | %02x %02x ---(%4ld bytes)--- %02x %02x | [%6ld:%6ld]/[%08lx:%08lx] \n",
                                  device_tag,
                                  udp_rx_total_count, udp_ip_str, received_bytes,
                                  gbl_filename,
                                  chunk_index,
                                  udp_chunk_rx_count[chunk_index],
                                  data_offset, tx_timestamp_str, tag_str,
                                  info_byte[0],
                                  info_byte[1],
                                  udp_data_len[chunk_index],
                                  info_byte[2],
                                  info_byte[3],
                                  start_address,
                                  end_address,
                                  start_address,
                                  end_address);

                }
                received = chunk_size;
                _received_count++;
                _downl_bytes += chunk_size;
                sl_wisun_ota_dfu_set_notify_download_chunk(_downl_bytes);
#else
                printf("[%s] UDP Rx %4ld from %s (%4ld bytes): ERROR MULTICAST_OTA_STORE_IN_FLASH Disabled for chunk[%4ld] | %02x %02x ---(%4ld bytes)--- %02x %02x | [%6ld:%6ld]/[%08lx:%08lx]\n",
                                device_tag,
                                udp_rx_total_count, udp_ip_str, received_bytes,
                                chunk_index,
                                info_byte[0],
                                info_byte[1],
                                chunk_size,
                                info_byte[2],
                                info_byte[3],
                                start_address,
                                end_address,
                                start_address,
                                end_address);
#endif /* MULTICAST_OTA_STORE_IN_FLASH == 1 */
              } else {
                printf("[%s] Duplicate chunk[%4ld], skipping it\n", device_tag, chunk_index);
                received = chunk_size;
                _resent_count++;
              }
          } else {
            printf("[%s] UDP Rx %2ld from %s (%4ld bytes): --------  data offset %ld out of the received data_buffer of %ld bytes\n", device_tag, udp_rx_total_count, udp_ip_str, received_bytes, data_offset, received_bytes);
          }
        } else{
          printf("[%s] UDP Rx %2ld from %s (%4ld bytes): --------  chunk index %ld above the MAX_CHUNKS of %d\n", device_tag, udp_rx_total_count, udp_ip_str, received_bytes, chunk_index, MAX_CHUNKS);
        }
      } else {
        printf("[%s] UDP Rx %2ld from %s (%4ld bytes): --------  Un-matching tag '%s' (expecting %s)\n", device_tag, udp_rx_total_count, udp_ip_str, received_bytes, tag_str, expected_tag);
      }
    } else {
      printf("[%s] UDP Rx %2ld from %s (%4ld bytes): --------  Un-matching gbl file '%s' (expecting '%s')\n", device_tag, udp_rx_total_count, udp_ip_str, received_bytes, gbl_filename, gbl_file);
      received = 0;
    }
  } else if (res == 1) {
    if      (strcmp(gbl_filename, "show_missed_from_list()") == 0) {received = 1; show_missed_from_list();}
#if MULTICAST_OTA_STORE_IN_FLASH == 1
    else if (strcmp(gbl_filename, "verify_image_in_flash()") == 0) {received = 2; verify_image_in_flash();}
    else if (strcmp(gbl_filename, "setImageToBootload()"   ) == 0) {received = 3; setImageToBootload(0);}
#endif /* MULTICAST_OTA_STORE_IN_FLASH == 1 */
    else if (strcmp(gbl_filename, "show_missed()"          ) == 0) {received = 5; show_missed();}
    else if (strcmp(gbl_filename, "clear_ota_data()"       ) == 0) {received = 6; clear_ota_data();}
    else if (strcmp(gbl_filename, "show_repeated()"        ) == 0) {received = 7; show_repeated();}
    else    {printf("unknown multicast_ota command '%s'\n", gbl_filename); received = 0; }
  } else if (res == 2) {
#if MULTICAST_OTA_STORE_IN_FLASH == 1
      if (strcmp(gbl_filename, "rebootAndInstall()"     ) == 0) {
          time_reboot_sec = chunk_index;
          if (rebootAndInstall(time_reboot_sec, CLEAR_NVM_NO) == 0){
              received = 8;
          }
          else{
              received = -8;
          }

      }
      else if (strcmp(gbl_filename, "rebootAndInstallClearNVMApp()"     ) == 0) {
          time_reboot_sec = chunk_index;
          if (rebootAndInstall(time_reboot_sec, CLEAR_NVM_APP) == 0){
              received = 9;
          }
          else{
              received = -9;
          }

      }
      else if (strcmp(gbl_filename, "rebootAndInstallClearNVMFull()"     ) == 0) {
          time_reboot_sec = chunk_index;
          if (rebootAndInstall(time_reboot_sec, CLEAR_NVM_FULL) == 0){
              received = 10;
          }
          else{
              received = -10;
          }

      }
      else{
#endif /* MULTICAST_OTA_STORE_IN_FLASH == 1 */
          printf("unknown multicast_ota command '%s'\n", gbl_filename); received = 0;
#if MULTICAST_OTA_STORE_IN_FLASH == 1
      }
#endif /* MULTICAST_OTA_STORE_IN_FLASH == 1 */
  } else {
    printf("[%s] UDP Rx %2ld from %s (%4ld bytes): --------  Only %d items can be found in the string %s\n", device_tag, udp_rx_total_count, udp_ip_str, received_bytes, res, udp_buff);
    return 0;
  }
  return received;
}
