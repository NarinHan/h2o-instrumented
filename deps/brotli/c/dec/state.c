#ifndef LOGGING_H
#define LOGGING_H
#include "logging.h"
#endif

/* Copyright 2015 Google Inc. All Rights Reserved.

   Distributed under MIT license.
   See file LICENSE for detail or copy at https://opensource.org/licenses/MIT
*/

#include "./state.h"

#include <stdlib.h>  /* free, malloc */

#include <brotli/types.h>
#include "./huffman.h"

#if defined(__cplusplus) || defined(c_plusplus)
extern "C" {
#endif

static void* DefaultAllocFunc(void* opaque, size_t size) {
  BROTLI_UNUSED(opaque);
  return malloc(size);
}

static void DefaultFreeFunc(void* opaque, void* address) {
  BROTLI_UNUSED(opaque);
  free(address);
}

void BrotliDecoderStateInit(BrotliDecoderState* s) {
  BrotliDecoderStateInitWithCustomAllocators(s, 0, 0, 0);
}

void BrotliDecoderStateInitWithCustomAllocators(BrotliDecoderState* s,
    brotli_alloc_func alloc_func, brotli_free_func free_func, void* opaque) {
  if (!alloc_func) {
    s->alloc_func = DefaultAllocFunc;
    s->free_func = DefaultFreeFunc;
    s->memory_manager_opaque = 0;
  } else {
    s->alloc_func = alloc_func;
    s->free_func = free_func;
    s->memory_manager_opaque = opaque;
  }

  s->error_code = 0; /* BROTLI_DECODER_NO_ERROR */

  BrotliInitBitReader(&s->br);
    {  // Begin logged block
    s->state = BROTLI_STATE_UNINITED;
    LOG_VAR_INT(s->state); // Auto-logged
    }  // End logged block
    {  // Begin logged block
    s->substate_metablock_header = BROTLI_STATE_METABLOCK_HEADER_NONE;
    LOG_VAR_INT(s->substate_metablock_header); // Auto-logged
    }  // End logged block
    {  // Begin logged block
    s->substate_tree_group = BROTLI_STATE_TREE_GROUP_NONE;
    LOG_VAR_INT(s->substate_tree_group); // Auto-logged
    }  // End logged block
    {  // Begin logged block
    s->substate_context_map = BROTLI_STATE_CONTEXT_MAP_NONE;
    LOG_VAR_INT(s->substate_context_map); // Auto-logged
    }  // End logged block
    {  // Begin logged block
    s->substate_uncompressed = BROTLI_STATE_UNCOMPRESSED_NONE;
    LOG_VAR_INT(s->substate_uncompressed); // Auto-logged
    }  // End logged block
    {  // Begin logged block
    s->substate_huffman = BROTLI_STATE_HUFFMAN_NONE;
    LOG_VAR_INT(s->substate_huffman); // Auto-logged
    }  // End logged block
  s->substate_decode_uint8 = BROTLI_STATE_DECODE_UINT8_NONE;
    {  // Begin logged block
    s->substate_read_block_length = BROTLI_STATE_READ_BLOCK_LENGTH_NONE;
    LOG_VAR_INT(s->substate_read_block_length); // Auto-logged
    }  // End logged block

  s->dictionary = BrotliGetDictionary();

  s->buffer_length = 0;
  s->loop_counter = 0;
  s->pos = 0;
  s->rb_roundtrips = 0;
  s->partial_pos_out = 0;

  s->block_type_trees = NULL;
  s->block_len_trees = NULL;
  s->ringbuffer = NULL;
  s->ringbuffer_size = 0;
  s->new_ringbuffer_size = 0;
  s->ringbuffer_mask = 0;

  s->context_map = NULL;
  s->context_modes = NULL;
  s->dist_context_map = NULL;
  s->context_map_slice = NULL;
  s->dist_context_map_slice = NULL;

  s->sub_loop_counter = 0;

  s->literal_hgroup.codes = NULL;
  s->literal_hgroup.htrees = NULL;
  s->insert_copy_hgroup.codes = NULL;
  s->insert_copy_hgroup.htrees = NULL;
  s->distance_hgroup.codes = NULL;
  s->distance_hgroup.htrees = NULL;

  s->is_last_metablock = 0;
  s->is_uncompressed = 0;
  s->is_metadata = 0;
  s->should_wrap_ringbuffer = 0;
  s->canny_ringbuffer_allocation = 1;

  s->window_bits = 0;
  s->max_distance = 0;
  s->dist_rb[0] = 16;
  s->dist_rb[1] = 15;
  s->dist_rb[2] = 11;
  s->dist_rb[3] = 4;
  s->dist_rb_idx = 0;
  s->block_type_trees = NULL;
  s->block_len_trees = NULL;

  /* Make small negative indexes addressable. */
  s->symbol_lists = &s->symbols_lists_array[BROTLI_HUFFMAN_MAX_CODE_LENGTH + 1];

  s->mtf_upper_bound = 63;
}

void BrotliDecoderStateMetablockBegin(BrotliDecoderState* s) {
  s->meta_block_remaining_len = 0;
  s->block_length[0] = 1U << 28;
  s->block_length[1] = 1U << 28;
  s->block_length[2] = 1U << 28;
  s->num_block_types[0] = 1;
  s->num_block_types[1] = 1;
  s->num_block_types[2] = 1;
  s->block_type_rb[0] = 1;
  s->block_type_rb[1] = 0;
  s->block_type_rb[2] = 1;
  s->block_type_rb[3] = 0;
  s->block_type_rb[4] = 1;
  s->block_type_rb[5] = 0;
  s->context_map = NULL;
  s->context_modes = NULL;
  s->dist_context_map = NULL;
  s->context_map_slice = NULL;
  s->literal_htree = NULL;
  s->dist_context_map_slice = NULL;
  s->dist_htree_index = 0;
  s->context_lookup1 = NULL;
  s->context_lookup2 = NULL;
  s->literal_hgroup.codes = NULL;
  s->literal_hgroup.htrees = NULL;
  s->insert_copy_hgroup.codes = NULL;
  s->insert_copy_hgroup.htrees = NULL;
  s->distance_hgroup.codes = NULL;
  s->distance_hgroup.htrees = NULL;
}

void BrotliDecoderStateCleanupAfterMetablock(BrotliDecoderState* s) {
  BROTLI_FREE(s, s->context_modes);
  BROTLI_FREE(s, s->context_map);
  BROTLI_FREE(s, s->dist_context_map);
  BROTLI_FREE(s, s->literal_hgroup.htrees);
  BROTLI_FREE(s, s->insert_copy_hgroup.htrees);
  BROTLI_FREE(s, s->distance_hgroup.htrees);
}

void BrotliDecoderStateCleanup(BrotliDecoderState* s) {
  BrotliDecoderStateCleanupAfterMetablock(s);

  BROTLI_FREE(s, s->ringbuffer);
  BROTLI_FREE(s, s->block_type_trees);
}

BROTLI_BOOL BrotliDecoderHuffmanTreeGroupInit(BrotliDecoderState* s,
    HuffmanTreeGroup* group, uint32_t alphabet_size, uint32_t ntrees) {
  /* Pack two allocations into one */
  const size_t max_table_size = kMaxHuffmanTableSize[(alphabet_size + 31) >> 5];
  const size_t code_size = sizeof(HuffmanCode) * ntrees * max_table_size;
  const size_t htree_size = sizeof(HuffmanCode*) * ntrees;
  /* Pointer alignment is, hopefully, wider than sizeof(HuffmanCode). */
  HuffmanCode** p = (HuffmanCode**)BROTLI_ALLOC(s, code_size + htree_size);
  group->alphabet_size = (uint16_t)alphabet_size;
  group->num_htrees = (uint16_t)ntrees;
  group->htrees = p;
  group->codes = (HuffmanCode*)(&p[ntrees]);
  return !!p;
}

#if defined(__cplusplus) || defined(c_plusplus)
}  /* extern "C" */
#endif
