// Copyright 2018-2019 Barefoot Networks, Inc.
// Copyright 2020-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0

#ifdef __cplusplus
extern "C" {
#endif

#include "bf_types/bf_types.h"
#include "bf_switchd/bf_switchd.h"

int switch_pci_sysfs_str_get(char* name, size_t name_size);

#ifdef __cplusplus
}  // extern "C"
#endif

#include <stdio.h>
#include <string.h>
#include "bf_init.h"

int InitBfSwitchd(const char *bf_sde_install, const char *bf_switchd_cfg, bool bf_switchd_background) {
  char bf_sysfs_fname[128];
  char sde_install[128];
  char switchd_cfg[128];
  FILE* fd;
  bf_switchd_context_t switchd_main_ctx;

  if (strlen(bf_sde_install) == 0) return BF_INVALID_ARG;
  // TODO ensure that bf_sde_install fits
  strncpy(sde_install, bf_sde_install, sizeof(sde_install));

  if (strlen(bf_switchd_cfg) == 0) return BF_INVALID_ARG;
  // TODO ensure that bf_switchd_cfg fits
  strncpy(switchd_cfg, bf_switchd_cfg, sizeof(switchd_cfg));

  switchd_main_ctx.install_dir = sde_install;
  switchd_main_ctx.conf_file = switchd_cfg;
  switchd_main_ctx.skip_p4 = true;
  if (bf_switchd_background)
    switchd_main_ctx.running_in_background = true;
  else
    switchd_main_ctx.shell_set_ucli = true;

  /* determine if kernel mode packet driver is loaded */
  switch_pci_sysfs_str_get(bf_sysfs_fname,
                           sizeof(bf_sysfs_fname) - sizeof("/dev_add"));
  strncat(bf_sysfs_fname, "/dev_add", sizeof("/dev_add"));
  printf("bf_sysfs_fname %s\n", bf_sysfs_fname);
  fd = fopen(bf_sysfs_fname, "r");
  if (fd != NULL) {
    /* override previous parsing if bf_kpkt KLM was loaded */
    printf("kernel mode packet driver present, forcing kernel_pkt option!\n");
    switchd_main_ctx.kernel_pkt = true;
    fclose(fd);
  }

  return bf_switchd_lib_init(&switchd_main_ctx);
}
