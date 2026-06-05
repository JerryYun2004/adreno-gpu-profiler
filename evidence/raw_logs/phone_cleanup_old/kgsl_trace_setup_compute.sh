#!/system/bin/sh

TRACE=/sys/kernel/tracing
EVENTS=$TRACE/events/kgsl

echo 0 > $TRACE/tracing_on
echo > $TRACE/trace

for e in $EVENTS/*/enable; do
  echo 0 > "$e" 2>/dev/null || true
done

for ev in   adreno_cmdbatch_queued   adreno_cmdbatch_submitted   adreno_cmdbatch_ready   adreno_cmdbatch_retired   adreno_cmdbatch_done   kgsl_issueibcmds   kgsl_gpubusy   kgsl_pwrstats   kgsl_pwrlevel   kgsl_buslevel   gpu_frequency   kgsl_mem_alloc   kgsl_mem_map   kgsl_mem_free   kgsl_mem_sync_cache   kgsl_context_create   kgsl_pwr_set_state   kgsl_pwr_request_state
do
  if [ -e $EVENTS/$ev/enable ]; then
    echo 1 > $EVENTS/$ev/enable
  fi
done
