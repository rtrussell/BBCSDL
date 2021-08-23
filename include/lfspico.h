/*
 * Block device in flash
 *
 * Copyright (c) 2017, Arm Limited. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 */
#ifndef LFSPICO_H
#define LFSPICO_H

#include "lfs.h"
#include "lfs_util.h"

#define ROOT_SIZE 0x100000
#define ROOT_OFFSET 0x100000

#ifdef PICO
#include <hardware/flash.h>
#include <hardware/sync.h>
#include <pico/stdlib.h>
#endif

#ifdef __cplusplus
extern "C"
{
#endif


// Block device specific tracing
#ifdef LFS_BBC_YES_TRACE
#define LFS_BBC_TRACE(...) LFS_TRACE(__VA_ARGS__)
#else
#define LFS_BBC_TRACE(...)
#endif

// bbc config (optional)
struct lfs_bbc_config {

    // Pointer to XIP_BASE + offset flash on the Pico
    void *buffer;
};

// bbc state
typedef struct lfs_bbc {
    uint8_t *buffer;
    const struct lfs_bbc_config *cfg;
} lfs_bbc_t;


// Create a flash block device using the geometry in lfs_config
int lfs_bbc_create(const struct lfs_config *cfg);
int lfs_bbc_createcfg(const struct lfs_config *cfg,
        const struct lfs_bbc_config *bdcfg);

// Clean up memory associated with block device
int lfs_bbc_destroy(const struct lfs_config *cfg);

// Read a block
int lfs_bbc_read(const struct lfs_config *cfg, lfs_block_t block,
        lfs_off_t off, void *buffer, lfs_size_t size);

// Program a block
//
// The block must have previously been erased.
int lfs_bbc_prog(const struct lfs_config *cfg, lfs_block_t block,
        lfs_off_t off, const void *buffer, lfs_size_t size);

// Erase a block
//
// A block must be erased before being programmed. The
// state of an erased block is undefined.
int lfs_bbc_erase(const struct lfs_config *cfg, lfs_block_t block);

// Sync the block device
int lfs_bbc_sync(const struct lfs_config *cfg);


#ifdef __cplusplus
} /* extern "C" */
#endif

#endif
