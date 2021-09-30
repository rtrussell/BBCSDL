/*
 * Block device in flash modified by Eric Olson from lfs_rambd.c
 *
 * Copyright (c) 2017, Arm Limited. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "lfsmcu.h"
#ifdef PICO_MCLOCK
#include "pico/multicore.h"
#endif

void lfs_bbc_init(void){
}

int lfs_bbc_createcfg(const struct lfs_config *cfg,
		const struct lfs_bbc_config *bdcfg) {
	LFS_BBC_TRACE("lfs_bbc_createcfg(%p {.context=%p, "
				".read=%p, .prog=%p, .erase=%p, .sync=%p, "
				".read_size=%"PRIu32", .prog_size=%"PRIu32", "
				".block_size=%"PRIu32", .block_count=%"PRIu32"}, "
				"%p {.buffer=%p})",
			(void*)cfg, cfg->context,
			(void*)(uintptr_t)cfg->read, (void*)(uintptr_t)cfg->prog,
			(void*)(uintptr_t)cfg->erase, (void*)(uintptr_t)cfg->sync,
			cfg->read_size, cfg->prog_size, cfg->block_size, cfg->block_count,
			(void*)bdcfg, bdcfg->buffer);
	lfs_bbc_t *bd = cfg->context;
	bd->cfg = bdcfg;

	// allocate buffer?
	if (bd->cfg->buffer) {
		bd->buffer = bd->cfg->buffer;
	} else {
		LFS_BBC_TRACE("lfs_bbc_createcfg -> %d", LFS_ERR_NOMEM);
		return LFS_ERR_NOMEM;
	}
	LFS_BBC_TRACE("lfs_bbc_createcfg -> %d", 0);
	return 0;
}

int lfs_bbc_destroy(const struct lfs_config *cfg) {
	LFS_BBC_TRACE("lfs_bbc_destroy(%p)", (void*)cfg);
	LFS_BBC_TRACE("lfs_bbc_destroy -> %d", 0);
	return 0;
}

int lfs_bbc_read(const struct lfs_config *cfg, lfs_block_t block,
		lfs_off_t off, void *buffer, lfs_size_t size) {
	LFS_BBC_TRACE("lfs_bbc_read(%p, "
				"0x%"PRIx32", %"PRIu32", %p, %"PRIu32")",
			(void*)cfg, block, off, buffer, size);
	lfs_bbc_t *bd = cfg->context;

	// check if read is valid
	LFS_ASSERT(off  % cfg->read_size == 0);
	LFS_ASSERT(size % cfg->read_size == 0);
	LFS_ASSERT(block < cfg->block_count);

	// read data
	memcpy(buffer, &bd->buffer[block*cfg->block_size + off], size);

	LFS_BBC_TRACE("lfs_bbc_read -> %d", 0);
	return 0;
}

int lfs_bbc_prog(const struct lfs_config *cfg, lfs_block_t block,
		lfs_off_t off, const void *buffer, lfs_size_t size) {
	LFS_BBC_TRACE("lfs_bbc_prog(%p, "
				"0x%"PRIx32", %"PRIu32", %p, %"PRIu32")",
			(void*)cfg, block, off, buffer, size);
	lfs_bbc_t *bd = cfg->context;

	// check if write is valid
	LFS_ASSERT(off  % cfg->prog_size == 0);
	LFS_ASSERT(size % cfg->prog_size == 0);
	LFS_ASSERT(block < cfg->block_count);

	// check that data was erased? only needed for testing
	for (lfs_off_t i = 0; i < size; i++) {
		LFS_ASSERT(bd->buffer[block*cfg->block_size + off + i] == 0);
	}

	// program data
#ifdef PICO
#if defined (PICO_MCLOCK)
    multicore_lockout_start_blocking ();
#elif defined (PICO_VGA)
    printf ("Start saving to Flash at %p, size %d.\n", &bd->buffer[block*cfg->block_size + off], size);
#endif
	uint32_t ints = save_and_disable_interrupts();
	flash_range_program(&bd->buffer[block*cfg->block_size + off]
		-(uint8_t *)XIP_BASE, buffer, size);
	restore_interrupts(ints);
#if defined (PICO_MCLOCK)
    multicore_lockout_end_blocking ();
#elif defined (PICO_VGA)
    printf ("Complete saving to Flash.\n");
#endif
#else
	memcpy(&bd->buffer[block*cfg->block_size + off], buffer, size);
#endif

	LFS_BBC_TRACE("lfs_bbc_prog -> %d", 0);
	return 0;
}

int lfs_bbc_erase(const struct lfs_config *cfg, lfs_block_t block) {
	LFS_BBC_TRACE("lfs_bbc_erase(%p, 0x%"PRIx32")", (void*)cfg, block);
	lfs_bbc_t *bd = cfg->context;

	// check if erase is valid
	LFS_ASSERT(block < cfg->block_count);

#ifdef PICO
#if defined (PICO_MCLOCK)
    multicore_lockout_start_blocking ();
#elif defined (PICO_VGA)
    printf ("Start erase Flash at %p, size %d.\n", &bd->buffer[block*cfg->block_size], cfg->block_size);
#endif
	uint32_t ints = save_and_disable_interrupts();
	flash_range_erase(&bd->buffer[block*cfg->block_size]
		-(uint8_t *)XIP_BASE, cfg->block_size);
	restore_interrupts(ints);
#if defined (PICO_MCLOCK)
    multicore_lockout_end_blocking ();
#elif defined (PICO_VGA)
    printf ("Completed erase Flash.\n");
#endif
#else
	memset(&bd->buffer[block*cfg->block_size],
		0, cfg->block_size);
#endif

	LFS_BBC_TRACE("lfs_bbc_erase -> %d", 0);
	return 0;
}

int lfs_bbc_sync(const struct lfs_config *cfg) {
	LFS_BBC_TRACE("lfs_bbc_sync(%p)", (void*)cfg);
	// sync does nothing because we aren't backed by anything real
	(void)cfg;
	LFS_BBC_TRACE("lfs_bbc_sync -> %d", 0);
	return 0;
}
