# Fused Softmax Width-Sequence Reproducibility Report

## Purpose

This report compares repeated `streamer_sweeper` width-sequence runs using the same fused softmax configuration.

The goal is to check whether the previously observed counter trends are stable and reproducible, rather than caused by one-off noise.

## Benchmark Configuration

```text
widths:      128,256,512,1024,2048
rows:        128
repeats:     4
time:        4 s
width-sleep: 0.1 s
variant:     fused_lmem softmax
```

## Reference Windows

Each run used `11_SP/SP_chunk001.csv` to detect the five width regions, then applied padded windows to every counter chunk.

```text
                  run  region_index  width  start_idx  end_idx_exclusive  start_s    end_s  samples
sweep_20260625_152944             1    128         27                 69 0.049915 0.112017       42
sweep_20260625_152944             2    256        157                198 0.248662 0.307354       41
sweep_20260625_152944             3    512        285                326 0.441789 0.500608       41
sweep_20260625_152944             4   1024        411                452 0.634295 0.693474       41
sweep_20260625_152944             5   2048        549                590 0.844253 0.902618       41
sweep_20260625_174559             1    128         30                 72 0.057689 0.119569       42
sweep_20260625_174559             2    256        165                206 0.262054 0.320504       41
sweep_20260625_174559             3    512        304                345 0.468723 0.527212       41
sweep_20260625_174559             4   1024        441                483 0.675249 0.735577       42
sweep_20260625_174559             5   2048        577                618 0.880872 0.940251       41
```

## Stable Near-16x Scaling Counters

Since `2048 / 128 = 16`, counters that repeatedly scale near 16x are strong evidence that the width sequence is being captured correctly.

```text
group                  counter       majority_label  ratio_mean  ratio_cv  corr_mean  label_consistency  total_w128_mean  total_w2048_mean
   TP         TP_OUTPUT_PIXELS strong_width_scaling        16.0       0.0        1.0                1.0          65536.0         1048576.0
   TP           TP_TP_SP_TRANS strong_width_scaling        16.0       0.0        1.0                1.0          32768.0          524288.0
   TP   TP_L1_CACHELINE_MISSES strong_width_scaling        16.0       0.0        1.0                1.0           8192.0          131072.0
   TP TP_L1_CACHELINE_REQUESTS strong_width_scaling        16.0       0.0        1.0                1.0           8192.0          131072.0
   TP           TP_SP_TP_TRANS strong_width_scaling        16.0       0.0        1.0                1.0           2048.0           32768.0
```

## Memory/Cache/TP/UCHE Bottleneck-Looking Counters

These counters are relevant to cache traffic, memory requests, stalls, starvation, and latency.

```text
group                                counter         majority_label  ratio_mean  ratio_cv  corr_mean  label_consistency  total_w128_mean  total_w2048_mean
  VPC              VPC_BE_STARVE_CYCLES_CCHE           active_mixed    1.745453  0.539131   0.598407                0.5      262411023.0       390074283.0
  LRZ                  LRZ_STARVE_CYCLES_RAS      flat_or_saturated    1.270319  0.121357   0.505708                1.0       56764342.0        71560837.0
  VPC           VPC_US_STARVE_CYCLES_REORDER           active_mixed    2.326741  0.145129   0.400772                1.0       23229522.0        53879128.5
 UCHE               UCHE_VBIF_LATENCY_CYCLES   strong_width_scaling   14.757011  1.414214   0.230884                0.5         361515.0         3690787.5
   SP                         SP_BUSY_CYCLES moderate_width_scaling    2.743225  0.011729   0.992745                1.0        1277048.0         3503235.5
   TP                         TP_BUSY_CYCLES   strong_width_scaling   12.478290  0.019904   0.965586                1.0          91350.5         1139850.0
   TP                       TP_OUTPUT_PIXELS   strong_width_scaling   16.000000  0.000000   1.000000                1.0          65536.0         1048576.0
   SP                SP_GPR_CACHE_SRC2_RDCNT moderate_width_scaling    1.411467  1.414214   0.261599                0.5         707548.0          996286.5
   SP SP_GPR_CACHE_SRC2_GPR_CACHE_HINT_RDCNT moderate_width_scaling    1.411498  1.414214   0.261599                0.5         707524.0          996274.5
   SP SP_GPR_CACHE_SRC0_GPR_CACHE_HINT_RDCNT moderate_width_scaling    1.354862  1.414214   0.258034                0.5         657472.0          895867.5
   SP  SP_GPR_CACHE_SRC2_GPR_CACHE_HIT_RDCNT moderate_width_scaling    1.298168  1.414214   0.261675                0.5         627565.0          812857.0
   SP                     SP_UCHE_READ_BURST moderate_width_scaling    1.298193  1.414214   0.261675                0.5         627553.0          812857.0
   TP                      TP_LATENCY_CYCLES   strong_width_scaling   11.348559  0.055311   0.972195                1.0          66958.5          759283.5
   CP                         CP_BUSY_CYCLES           active_mixed    2.938504  1.385452   0.002483                0.5         831166.5          730350.0
   SP                  SP_STARVE_CYCLES_HLSQ moderate_width_scaling    1.293244  1.414214   0.075319                0.5         540714.0          699353.0
   SP  SP_GPR_CACHE_SRC1_GPR_CACHE_HIT_RDCNT moderate_width_scaling    1.278372  1.414214   0.257397                0.5         540281.0          690583.0
   SP SP_GPR_CACHE_SRC1_GPR_CACHE_HINT_RDCNT moderate_width_scaling    2.685180  1.414214   0.247886                0.5         250642.0          687868.0
   PC             PC_S_TESS_STARVE_CYCLES_PC           active_mixed    1.558803  0.282458   0.606788                0.5         407655.0          609279.0
   SP        SP_TEXTURE_FETCH_LATENCY_CYCLES   strong_width_scaling   11.947151  1.414214   0.219165                0.5          49670.0          596461.5
   SP        SP_LOW_EFFICIENCY_STARVED_BY_TP   strong_width_scaling    7.766627  1.414214   0.082496                0.5          73657.0          572913.0
   SP                SP_GPR_CACHE_SRC1_RDCNT   strong_width_scaling    6.347969  1.414214   0.238662                0.5          83201.5          561827.0
  RAS                  RAS_STARVE_CYCLES_TSE      flat_or_saturated    1.114055  0.429279  -0.154496                1.0         550344.0          558303.0
   TP                    TP_STARVE_CYCLES_SP   strong_width_scaling    6.132911  1.414214   0.302770                0.5          86253.0          558076.5
   TP               TP_FILTER_WORKLOAD_32BIT   strong_width_scaling    8.000000  1.414214   0.225648                0.5          65536.0          524288.0
   TP              TP_OUTPUT_PIXELS_ZERO_LOD   strong_width_scaling    8.000000  1.414214   0.188375                0.5          65536.0          524288.0
   TP                 TP_OUTPUT_PIXELS_POINT   strong_width_scaling    8.000000  1.414214   0.188375                0.5          65536.0          524288.0
   TP                   TP_FILTER_POINT_FP32   strong_width_scaling    8.000000  1.414214   0.261698                0.5          65536.0          524288.0
   TP                         TP_TP_SP_TRANS   strong_width_scaling   16.000000  0.000000   1.000000                1.0          32768.0          524288.0
   TP                  TP_STARVE_CYCLES_UCHE   strong_width_scaling    7.332205  1.414214   0.308027                0.5          63052.5          501273.5
  VFD                  VFDP_STARVE_CYCLES_PC moderate_width_scaling    2.540330  0.006765   0.996113                1.0         191662.5          486889.5
   RB                         RB_BUSY_CYCLES moderate_width_scaling    1.446805  1.414214   0.257393                0.5         287172.0          413902.0
   TP            TP_L1_5_MISS_LATENCY_CYCLES   strong_width_scaling    7.908393  1.414214   0.136965                0.5          49652.0          400441.5
   SP                            SP_GPR_READ moderate_width_scaling    1.756759  1.414214   0.106838                0.5         207822.5          365075.5
   SP            SP_GPR_CACHE_SRC0_GPR_RDCNT moderate_width_scaling    1.703664  1.414214   0.261697                0.5         205167.5          349564.5
   SP                 SP_LB_NONUAV_TOTAL_REQ moderate_width_scaling    1.712121  1.414214   0.261698                0.5         202752.0          347136.0
   CP                  CP_BUSY_GFX_CORE_IDLE           active_mixed    2.070891  1.209881   0.014249                0.5         200198.5          264506.0
   SP                           SP_GPR_WRITE moderate_width_scaling    1.697248  1.414214   0.105486                0.5         146255.0          248253.0
   SP                 SP_ALU_GPR_READ_CYCLES moderate_width_scaling    1.620626  1.414214   0.103609                0.5         121955.5          197637.0
 UCHE                       UCHE_BUSY_CYCLES   strong_width_scaling    7.955258  1.414214   0.261183                0.5          27888.0          197274.5
 RBBM                      RBBM_US_VBIF_BUSY moderate_width_scaling    1.917130  0.844977   0.146740                0.5         100821.0          195012.0
   CP                   CP_SQE_SYS_WFI_STALL moderate_width_scaling    1.469874  1.414214   0.261503                0.5         116654.0          166814.5
   CP              CP_MEMORY_POOL_SYNC_STALL moderate_width_scaling    1.400612  1.414214   0.261402                0.5         120148.5          149893.5
 RBBM                      RBBM_US_UCHE_BUSY moderate_width_scaling    2.019841  1.414214   0.107548                0.5          66778.0          134123.5
 UCHE             UCHE_READ_REQUESTS_TP_GBIF   strong_width_scaling    8.258695  1.414214   0.261654                0.5          15872.0          131082.0
 UCHE                  UCHE_READ_REQUESTS_TP   strong_width_scaling    8.258695  1.414214   0.261654                0.5          15872.0          131082.0
 UCHE                      UCHE_RAM_READ_REQ   strong_width_scaling    8.103116  1.414214   0.261694                0.5          16160.0          131076.0
   SP       SP_TEXTURE_FETCH_LATENCY_SAMPLES   strong_width_scaling    8.000000  1.414214   0.225648                0.5          16384.0          131072.0
 UCHE                 UCHE_WRITE_REQUESTS_SP   strong_width_scaling    8.000000  1.414214   0.261698                0.5          16384.0          131072.0
   TP                      TP_QUADS_RECEIVED   strong_width_scaling    8.000000  1.414214   0.225648                0.5          16384.0          131072.0
   TP                        TP_QUADS_BUFFER   strong_width_scaling    8.000000  1.414214   0.225648                0.5          16384.0          131072.0
   TP               TP_L1_CACHELINE_REQUESTS   strong_width_scaling   16.000000  0.000000   1.000000                1.0           8192.0          131072.0
   TP                 TP_L1_CACHELINE_MISSES   strong_width_scaling   16.000000  0.000000   1.000000                1.0           8192.0          131072.0
 UCHE                     UCHE_RAM_WRITE_REQ   strong_width_scaling   12.800000  1.414214   0.254040                0.5          10240.0          131072.0
 UCHE         UCHE_STARVED_CYCLES_VBIF_DECMP   strong_width_scaling   24.982959  1.414214   0.238056                0.5           5439.0          122416.5
   SP                      SP_TTU_GPR_WR_REQ      flat_or_saturated    0.643188  1.414214   0.261690                1.0         162115.5          104275.5
   SP                       SP_ICL1_REQUESTS           active_mixed    0.917391  1.414214   0.077201                0.5         110400.0          101280.0
   CP                         CP_ICACHE_HITS moderate_width_scaling    1.247079  1.296601   0.259897                0.5          79932.0           96164.5
 HLSQ                       HLSQ_BUSY_CYCLES moderate_width_scaling    1.232686  1.354517   0.258699                0.5          74213.0           89430.5
 RBBM                      RBBM_US_HLSQ_BUSY moderate_width_scaling    1.209877  1.349932   0.076866                0.5          72976.0           87393.5
   CP                  CP_SQE_I_CACHE_STARVE      flat_or_saturated    0.937788  0.482269   0.165728                1.0          91177.5           81897.0
   PC                      PC_US_BUSY_CYCLES moderate_width_scaling    1.284083  1.414214   0.259127                0.5          63789.5           81850.0
 RBBM                        RBBM_US_PC_BUSY moderate_width_scaling    1.279019  1.414214   0.080619                0.5          64043.0           81838.0
   PC         PC_US_STALL_CYCLES_COMPUTE_GFX moderate_width_scaling    1.291634  1.414214   0.259754                0.5          62059.5           80133.0
   CP                        CP_ICACHE_STALL      flat_or_saturated    0.819561  0.557031  -0.213199                1.0          95758.5           70634.5
   TP               TP_L1_TAG_WORKING_CYCLES   strong_width_scaling    8.000000  1.414214   0.448152                0.5           8192.0           65536.0
   TP             TP_FRONTEND_WORKING_CYCLES   strong_width_scaling    8.000000  1.414214   0.448152                0.5           8192.0           65536.0
   SP                    SP_UCHE_WRITE_TRANS   strong_width_scaling    8.000000  1.414214   0.225648                0.5           8192.0           65536.0
   TP        TP_L1_DATA_WRITE_WORKING_CYCLES   strong_width_scaling    8.000000  1.414214   0.448152                0.5           8192.0           65536.0
   TP              TP_BACKEND_WORKING_CYCLES   strong_width_scaling    8.000000  1.414214   0.448152                0.5           8192.0           65536.0
   TP           TP_L1_5_CACHE_WORKING_CYCLES   strong_width_scaling    8.000000  1.414214   0.448152                0.5           8192.0           65536.0
   TP         TP_PRE_L1_DECOM_WORKING_CYCLES   strong_width_scaling    8.000000  1.414214   0.448152                0.5           8192.0           65536.0
   SP                  SP_CCHE_UAV_TOTAL_REQ   strong_width_scaling    8.000000  1.414214   0.261698                0.5           8192.0           65536.0
   TP                    TP_L1_5_L2_REQUESTS   strong_width_scaling    8.000000  1.414214   0.188375                0.5           8192.0           65536.0
   TP                       TP_TPA2TPC_TRANS   strong_width_scaling    8.000000  1.414214   0.188375                0.5           8192.0           65536.0
 UCHE                UCHE_VBIF_READ_BEATS_TP   strong_width_scaling   32.000000  1.414214   0.234126                0.5           2048.0           65536.0
 UCHE             UCHE_VBIF_STALL_WRITE_DATA   strong_width_scaling    6.608900  1.414214   0.212192                0.5           8347.0           46487.0
   SP                    SP_LB_READ_XFER_ALU           active_mixed    0.970946  1.414214   0.233812                0.5          35332.0           34304.5
 UCHE               UCHE_VBIF_READ_BEATS_CH1   strong_width_scaling   32.000000  1.414214   0.234126                0.5           1024.0           32768.0
 UCHE               UCHE_VBIF_READ_BEATS_CH0   strong_width_scaling   32.000000  1.414214   0.234126                0.5           1024.0           32768.0
 UCHE              UCHE_VBIF_WRITE_BEATS_CH1   strong_width_scaling    8.000000  1.414214   0.261698                0.5           4096.0           32768.0
```

## Key Counter Run-to-Run Comparison

```text
                  run group                         counter                  label  scaling_ratio_last_over_first  corr_total_vs_width  total_w128  total_w256  total_w512  total_w1024  total_w2048
sweep_20260625_152944    SP           SP_ALU_WORKING_CYCLES moderate_width_scaling                       2.797927             0.998826    197632.0    237568.0    282624.0     372736.0     552960.0
sweep_20260625_174559    SP           SP_ALU_WORKING_CYCLES moderate_width_scaling                       2.797927             0.998826    197632.0    237568.0    282624.0     372736.0     552960.0
sweep_20260625_152944    SP                  SP_BUSY_CYCLES moderate_width_scaling                       2.765977             0.992412   1277277.0   1323957.0   1581126.0    2053186.0    3532919.0
sweep_20260625_174559    SP                  SP_BUSY_CYCLES moderate_width_scaling                       2.720473             0.993079   1276819.0   1323902.0   1586409.0    2043408.0    3473552.0
sweep_20260625_152944    SP    SP_FULL_ALU_MUL_INSTRUCTIONS   strong_width_scaling                      16.000000             1.000000     32768.0     65536.0    131072.0     262144.0     524288.0
sweep_20260625_174559    SP    SP_FULL_ALU_MUL_INSTRUCTIONS      flat_or_saturated                       0.000000            -0.548703     32768.0     65536.0    131072.0          0.0          0.0
sweep_20260625_152944    SP        SP_GM_STORE_INSTRUCTIONS   strong_width_scaling                      16.000000             1.000000     16384.0     32768.0     65536.0     131072.0     262144.0
sweep_20260625_174559    SP        SP_GM_STORE_INSTRUCTIONS      flat_or_saturated                       0.000000            -0.548703     16384.0     32768.0     65536.0          0.0          0.0
sweep_20260625_152944    SP SP_LOW_EFFICIENCY_STARVED_BY_TP   strong_width_scaling                      15.533254             0.954833     73766.0     88336.0    107382.0     257788.0    1145826.0
sweep_20260625_174559    SP SP_LOW_EFFICIENCY_STARVED_BY_TP      flat_or_saturated                       0.000000            -0.789841     73548.0     87108.0    111791.0          0.0          0.0
sweep_20260625_152944    SP              SP_STALL_CYCLES_TP   strong_width_scaling                      14.111801             0.995543       644.0      1740.0      2991.0       5198.0       9088.0
sweep_20260625_174559    SP              SP_STALL_CYCLES_TP   strong_width_scaling                      14.786535             0.994411       609.0      1743.0      2987.0       5244.0       9005.0
sweep_20260625_152944    SP             SP_UCHE_WRITE_TRANS   strong_width_scaling                      16.000000             1.000000      8192.0     16384.0     32768.0      65536.0     131072.0
sweep_20260625_174559    SP             SP_UCHE_WRITE_TRANS      flat_or_saturated                       0.000000            -0.548703      8192.0     16384.0     32768.0          0.0          0.0
sweep_20260625_152944    TP        TP_L1_CACHELINE_REQUESTS   strong_width_scaling                      16.000000             1.000000      8192.0     16384.0     32768.0      65536.0     131072.0
sweep_20260625_174559    TP        TP_L1_CACHELINE_REQUESTS   strong_width_scaling                      16.000000             1.000000      8192.0     16384.0     32768.0      65536.0     131072.0
sweep_20260625_152944    TP           TP_STARVE_CYCLES_UCHE   strong_width_scaling                      14.664409             0.943925     68366.0     88136.0    108662.0     193887.0    1002547.0
sweep_20260625_174559    TP           TP_STARVE_CYCLES_UCHE      flat_or_saturated                       0.000000            -0.327870     57739.0     84568.0    107550.0     188336.0          0.0
sweep_20260625_152944  UCHE                UCHE_BUSY_CYCLES   strong_width_scaling                      15.910517             0.998969     24798.0     45313.0     93539.0     210341.0     394549.0
sweep_20260625_174559  UCHE                UCHE_BUSY_CYCLES      flat_or_saturated                       0.000000            -0.476603     30978.0         0.0         0.0          0.0          0.0
sweep_20260625_152944  UCHE               UCHE_RAM_READ_REQ   strong_width_scaling                      16.206231             0.999991     16176.0     32192.0     64374.0     131086.0     262152.0
sweep_20260625_174559  UCHE               UCHE_RAM_READ_REQ      flat_or_saturated                       0.000000            -0.476603     16144.0         0.0         0.0          0.0          0.0
sweep_20260625_152944  UCHE              UCHE_RAM_WRITE_REQ   strong_width_scaling                      25.600000             0.984683     10240.0     20480.0     40960.0      84984.0     262144.0
sweep_20260625_174559  UCHE              UCHE_RAM_WRITE_REQ      flat_or_saturated                       0.000000            -0.476603     10240.0         0.0         0.0          0.0          0.0
sweep_20260625_152944  UCHE           UCHE_READ_REQUESTS_TP   strong_width_scaling                      16.517389             0.999911     15872.0     32128.0     62108.0     131096.0     262164.0
sweep_20260625_174559  UCHE           UCHE_READ_REQUESTS_TP      flat_or_saturated                       0.000000            -0.476603     15872.0         0.0         0.0          0.0          0.0
sweep_20260625_152944  UCHE        UCHE_VBIF_LATENCY_CYCLES   strong_width_scaling                      29.514022             0.938372    250104.0    615345.0    961323.0    1149720.0    7381575.0
sweep_20260625_174559  UCHE        UCHE_VBIF_LATENCY_CYCLES      flat_or_saturated                       0.000000            -0.476603    472926.0         0.0         0.0          0.0          0.0
sweep_20260625_152944  UCHE          UCHE_WRITE_REQUESTS_SP   strong_width_scaling                      16.000000             1.000000     16384.0     32768.0     65536.0     131072.0     262144.0
sweep_20260625_174559  UCHE          UCHE_WRITE_REQUESTS_SP      flat_or_saturated                       0.000000            -0.476603     16384.0         0.0         0.0          0.0          0.0
```

## Interpretation

The finding is considered reproducible if the same counter families repeatedly show high width correlation, low run-to-run variation, and similar scaling behavior.

The main expected stable pattern is:

- SP instruction and transaction counters scale with softmax width.
- UCHE/TP request and transaction counters scale close to the 16x width ratio.
- UCHE/VBIF latency and TP/UCHE starvation counters grow strongly at larger widths.
- SP busy and ALU working cycles grow less than raw element count, because they represent cycle/utilization behavior under parallel execution.

If these patterns appear across repeated runs, the previous conclusion is strengthened: larger fused-softmax widths increasingly stress the memory/cache/TP/UCHE path rather than only increasing SP ALU work.
