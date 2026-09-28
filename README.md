# Oplus missing drivers (peridot / F6)

Oplus kernel modules that exist on F5 (marble, kernel 5.10) but have no F6
(peridot, kernel 6.1) equivalent in the stock tree.

Ported from OnePlusOSS:
`android_kernel_modules_and_devicetree_oneplus_sm8650`
branch `oneplus/sm8650_v_15.0.0_oneplus12` (Android 15 = kernel 6.1).

## Modules

| module | source | exposes |
|---|---|---|
| `oplus_bsp_midas` | `vendor/oplus/kernel/cpu/midas/v1_gki/` | `/dev/midas` char device (cdev) + `mmap` ring of per-task `time_in_state[]`; ioctl table in `midas_ioctl.c`; `/proc` binder stats tree via `binder_stats_dev.c`; hooks `android_vh_cpufreq_acct_update_power`, `android_vh_binder_proc_transaction`, `android_vh_binder_new_ref`, `android_vh_binder_del_ref` |
| `oplus_bsp_zram_opt` | `vendor/oplus/kernel/mm/zram_opt/zram_opt.c` | `/proc/oplus_mem/swappiness_para`, `/proc/oplus_mem/dynamic_swappiness`; hooks `android_vh_tune_swappiness`, `android_vh_tune_inactive_ratio`, `android_rvh_set_balance_anon_file_reclaim`, `android_vh_init_adjust_zone_wmark` |
| `oplus_bootprof` | OnePlus phoenix base | boot profiling ring, exposed over `/proc` |
| `oplus_shutdown_reason` | OnePlus last_boot_reason | `/proc/last_boot_reason`, panic/boot reason passthrough |

`oplus_afs_config` is **not** here -- it lives in
`kvfsrl/vendor_frameboost-drivers` (`afs_config/`), which already ships the stub
with a builtin protobuf fallback matching peridot's `/system_ext/etc/afsConfig.pb`.

## midas port notes (v1_gki)

Only the four translation units that the F5 `oplus_bsp_midas.ko` was actually
built from are kept: `midas_dev.c`, `midas_ioctl.c`, `midas_module.c`,
`binder_stats_dev.c`. `dispcap_dev.c` and `vpu_pw_off_latency_proc.c` exist in
the upstream directory but are not in the `v1_gki` Makefile `objs`, so they are
not part of the module and are dropped.

The one source change from upstream is in `binder_stats_dev.c`:

```c
-#include <linux/android/binder.h>
```

`include/linux/android/binder.h` does not exist in the peridot 6.1 GKI tree
(binder internals are private in `drivers/android/binder_internal.h`). The
module never touches those internals -- all three binder hooks it registers
take only `struct task_struct *` arguments, and the signatures are byte-for-byte
identical on 6.1:

| hook | 5.10 (F5) | 6.1 (peridot) |
|---|---|---|
| `android_vh_binder_proc_transaction` | `TP_PROTO(task_struct *caller_task, task_struct *binder_proc_task, task_struct *binder_th_task, int node_debug_id, unsigned int code, bool pending_async)` | same |
| `android_vh_binder_new_ref` | `TP_PROTO(task_struct *proc, uint32_t ref_desc, int node_debug_id)` | same |
| `android_vh_binder_del_ref` | `TP_PROTO(task_struct *proc, uint32_t ref_desc)` | same |

Note: `android_vh_midas_record_task_times` does not exist on 6.1, but upstream
does not use it either -- the registration is against
`android_vh_cpufreq_acct_update_power`, whose signature
`TP_PROTO(u64 cputime, struct task_struct *p, unsigned int state)` also matches.

## Build config

`CONFIG_OPLUS_FEATURE_MIDAS_GKI=m` plus `CONFIG_OPLUS_FEATURE_BINDER_STATS_ENABLE=y`.
`BINDER_STATS_ENABLE` must be set: the whole of `binder_stats_dev.c` sits behind
that `#if`, and without it the object is empty and `midas_module.c` fails to link
against `binder_stats_dev_init()`.

Enabled: `CONFIG_OPLUS_BALANCE_ANON_FILE_RECLAIM`, `CONFIG_DYNAMIC_TUNING_SWAPPINESS`,
`CONFIG_HYBRIDSWAP_SWAPD`.

Deliberately NOT enabled: `CONFIG_OPLUS_EXTRA_FREE_KBYTES` — it redefines the
core `first_online_pgdat()`, which is unusable from an out-of-tree module.
