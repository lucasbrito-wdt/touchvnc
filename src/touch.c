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

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <linux/uinput.h>
#include <linux/input-event-codes.h>
#include <neatvnc.h>

#include "touch.h"

static void emit(int fd, uint16_t type, uint16_t code, int32_t value)
{
	struct input_event ev = {
		.type = type,
		.code = code,
		.value = value,
	};
	write(fd, &ev, sizeof(ev));
}

int touch_init(struct touch* self, uint32_t width, uint32_t height)
{
	memset(self, 0, sizeof(*self));

	for (int i = 0; i < TOUCH_MAX_SLOTS; i++)
		self->slots_tracking[i] = -1;

	self->next_tracking_id = 0;
	self->output_width = width;
	self->output_height = height;

	int fd = open("/dev/uinput", O_WRONLY | O_NONBLOCK);
	if (fd < 0)
		return -1;

	ioctl(fd, UI_SET_EVBIT, EV_ABS);
	ioctl(fd, UI_SET_EVBIT, EV_KEY);
	ioctl(fd, UI_SET_EVBIT, EV_SYN);
	ioctl(fd, UI_SET_KEYBIT, BTN_TOUCH);
	ioctl(fd, UI_SET_PROPBIT, INPUT_PROP_DIRECT);

	/* Multi-touch type B axes */
	struct uinput_abs_setup abs_setup;

	memset(&abs_setup, 0, sizeof(abs_setup));
	abs_setup.code = ABS_MT_SLOT;
	abs_setup.absinfo.minimum = 0;
	abs_setup.absinfo.maximum = TOUCH_MAX_SLOTS - 1;
	ioctl(fd, UI_ABS_SETUP, &abs_setup);

	memset(&abs_setup, 0, sizeof(abs_setup));
	abs_setup.code = ABS_MT_TRACKING_ID;
	abs_setup.absinfo.minimum = 0;
	abs_setup.absinfo.maximum = INT32_MAX;
	ioctl(fd, UI_ABS_SETUP, &abs_setup);

	memset(&abs_setup, 0, sizeof(abs_setup));
	abs_setup.code = ABS_MT_POSITION_X;
	abs_setup.absinfo.minimum = 0;
	abs_setup.absinfo.maximum = width - 1;
	ioctl(fd, UI_ABS_SETUP, &abs_setup);

	memset(&abs_setup, 0, sizeof(abs_setup));
	abs_setup.code = ABS_MT_POSITION_Y;
	abs_setup.absinfo.minimum = 0;
	abs_setup.absinfo.maximum = height - 1;
	ioctl(fd, UI_ABS_SETUP, &abs_setup);

	memset(&abs_setup, 0, sizeof(abs_setup));
	abs_setup.code = ABS_MT_PRESSURE;
	abs_setup.absinfo.minimum = 0;
	abs_setup.absinfo.maximum = 65535;
	ioctl(fd, UI_ABS_SETUP, &abs_setup);

	/* Single-touch axes for compatibility */
	memset(&abs_setup, 0, sizeof(abs_setup));
	abs_setup.code = ABS_X;
	abs_setup.absinfo.minimum = 0;
	abs_setup.absinfo.maximum = width - 1;
	ioctl(fd, UI_ABS_SETUP, &abs_setup);

	memset(&abs_setup, 0, sizeof(abs_setup));
	abs_setup.code = ABS_Y;
	abs_setup.absinfo.minimum = 0;
	abs_setup.absinfo.maximum = height - 1;
	ioctl(fd, UI_ABS_SETUP, &abs_setup);

	memset(&abs_setup, 0, sizeof(abs_setup));
	abs_setup.code = ABS_PRESSURE;
	abs_setup.absinfo.minimum = 0;
	abs_setup.absinfo.maximum = 65535;
	ioctl(fd, UI_ABS_SETUP, &abs_setup);

	struct uinput_setup setup;
	memset(&setup, 0, sizeof(setup));
	setup.id.bustype = BUS_VIRTUAL;
	setup.id.vendor = 0x1234;
	setup.id.product = 0x5678;
	setup.id.version = 1;
	snprintf(setup.name, UINPUT_MAX_NAME_SIZE,
		 "TouchVNC Virtual Touchscreen");

	if (ioctl(fd, UI_DEV_SETUP, &setup) < 0)
		goto fail;

	if (ioctl(fd, UI_DEV_CREATE) < 0)
		goto fail;

	self->uinput_fd = fd;
	self->initialized = true;
	return 0;

fail:
	close(fd);
	return -1;
}

void touch_event(struct touch* self, const struct nvnc_touch_slot* slots,
		 uint8_t count)
{
	if (!self->initialized)
		return;

	int fd = self->uinput_fd;
	bool any_active = false;

	for (uint8_t i = 0; i < count; i++) {
		const struct nvnc_touch_slot* slot = &slots[i];
		uint8_t sid = slot->id;

		if (sid >= TOUCH_MAX_SLOTS)
			continue;

		int32_t px = (int32_t)slot->x * (self->output_width - 1) / 65535;
		int32_t py = (int32_t)slot->y * (self->output_height - 1) / 65535;

		emit(fd, EV_ABS, ABS_MT_SLOT, sid);

		switch (slot->type) {
		case 0: /* down */
			self->slots_tracking[sid] = self->next_tracking_id++;
			emit(fd, EV_ABS, ABS_MT_TRACKING_ID,
			     self->slots_tracking[sid]);
			emit(fd, EV_ABS, ABS_MT_POSITION_X, px);
			emit(fd, EV_ABS, ABS_MT_POSITION_Y, py);
			emit(fd, EV_ABS, ABS_MT_PRESSURE, slot->pressure);
			break;
		case 1: /* move */
			emit(fd, EV_ABS, ABS_MT_POSITION_X, px);
			emit(fd, EV_ABS, ABS_MT_POSITION_Y, py);
			emit(fd, EV_ABS, ABS_MT_PRESSURE, slot->pressure);
			break;
		case 2: /* up */
			emit(fd, EV_ABS, ABS_MT_TRACKING_ID, -1);
			self->slots_tracking[sid] = -1;
			break;
		}
	}

	/* Check if any slot is still active */
	for (int i = 0; i < TOUCH_MAX_SLOTS; i++) {
		if (self->slots_tracking[i] != -1) {
			any_active = true;
			break;
		}
	}

	emit(fd, EV_KEY, BTN_TOUCH, any_active ? 1 : 0);
	emit(fd, EV_SYN, SYN_REPORT, 0);
}

void touch_destroy(struct touch* self)
{
	if (!self->initialized)
		return;

	ioctl(self->uinput_fd, UI_DEV_DESTROY);
	close(self->uinput_fd);
	self->uinput_fd = -1;
	self->initialized = false;
}
