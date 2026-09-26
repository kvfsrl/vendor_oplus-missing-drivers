# Oplus missing drivers (peridot / F6)

Oplus kernel modules that exist on F5 (marble, kernel 5.10) but have no F6
(peridot, kernel 6.1) equivalent in the stock tree.

Ported from OnePlusOSS:
`android_kernel_modules_and_devicetree_oneplus_sm8650`
branch `oneplus/sm8650_v_15.0.0_oneplus12` (Android 15 = kernel 6.1).

## Modules

| module | source | exposes |
|---|---|---|
| `oplus_bsp_zram_opt` | `vendor/oplus/kernel/mm/zram_opt/zram_opt.c` | `/proc/oplus_mem/swappiness_para`, `/proc/oplus_mem/dynamic_swappiness`; hooks `android_vh_tune_swappiness`, `android_vh_tune_inactive_ratio`, `android_rvh_set_balance_anon_file_reclaim`, `android_vh_init_adjust_zone_wmark` |

`/proc/oplus_mem/swappiness_para` is what F5's `init.oplus.nandswap.sh` writes to
set `vm_swappiness` / `direct_swappiness` / `swapd_swappiness`. Without this
module F6 cannot set `direct_swappiness=60` / `swapd_swappiness=200`.

## Build config

Enabled: `CONFIG_OPLUS_BALANCE_ANON_FILE_RECLAIM`, `CONFIG_DYNAMIC_TUNING_SWAPPINESS`,
`CONFIG_HYBRIDSWAP_SWAPD`.

Deliberately NOT enabled: `CONFIG_OPLUS_EXTRA_FREE_KBYTES` — it redefines the
core `first_online_pgdat()`, which is unusable from an out-of-tree module.
