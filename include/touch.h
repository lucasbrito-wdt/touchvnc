/*
 * Copyright (c) 2026 Lucas Brito
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES WITH
 * REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT,
 * INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
 * LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
 * OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <neatvnc.h>

#define TOUCH_MAX_SLOTS 10

struct touch {
	int uinput_fd;
	int32_t slots_tracking[TOUCH_MAX_SLOTS]; /* tracking_id per slot, -1 = free */
	int32_t next_tracking_id;
	uint32_t output_width;
	uint32_t output_height;
	bool initialized;
};

int  touch_init(struct touch* self, uint32_t width, uint32_t height);
void touch_event(struct touch* self, const struct nvnc_touch_slot* slots, uint8_t count);
void touch_destroy(struct touch* self);
