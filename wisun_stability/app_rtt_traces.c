#include "app_rtt_traces.h"
#include "sl_malloc.h"

sl_status_t app_set_all_traces(uint8_t trace_level, bool verbose) {
  sl_status_t ret;
  sl_wisun_trace_group_config_t *trace_config;
  uint8_t group_count;
  uint8_t i;
  trace_config = sl_malloc(SL_WISUN_TRACE_GROUP_COUNT * sizeof(sl_wisun_trace_group_config_t));
  group_count = SL_WISUN_TRACE_GROUP_RF+1;

  for (i = 0; i < group_count; i++) {
      trace_config[i].group_id = i;
      trace_config[i].trace_level = trace_level;
  }
  ret = sl_wisun_set_trace_level(group_count, trace_config);
  if (verbose) printf("\nSet all %d trace groups to level %d: %lu\n", group_count, trace_level, ret);
  sl_free(trace_config);
  return ret;
}

sl_status_t app_set_trace(uint8_t group_id, uint8_t trace_level, bool verbose)
{
  sl_status_t ret;
  sl_wisun_trace_group_config_t trace_config;

  trace_config.group_id = group_id;
  trace_config.trace_level = trace_level;
  ret = sl_wisun_set_trace_level(1, &trace_config);
  if (verbose) printf("Set trace group %u to level %u: %lu\n", group_id, trace_level, ret);
  return ret;
}
