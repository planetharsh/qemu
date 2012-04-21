/*
 * QEMU PC keyboard emulation
 *
 * Copyright (c) 2003 Fabrice Bellard
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */
#include "pckbd.h"
#include "sysemu.h"

/* debug PC keyboard */
//#define DEBUG_KBD
#ifdef DEBUG_KBD
#define DPRINTF(fmt, ...)                                       \
    do { printf("KBD: " fmt , ## __VA_ARGS__); } while (0)
#else
#define DPRINTF(fmt, ...)
#endif

/*	Keyboard Controller Commands */
#define KBD_CCMD_READ_MODE	0x20	/* Read mode bits */
#define KBD_CCMD_WRITE_MODE	0x60	/* Write mode bits */
#define KBD_CCMD_GET_VERSION	0xA1	/* Get controller version */
#define KBD_CCMD_MOUSE_DISABLE	0xA7	/* Disable mouse interface */
#define KBD_CCMD_MOUSE_ENABLE	0xA8	/* Enable mouse interface */
#define KBD_CCMD_TEST_MOUSE	0xA9	/* Mouse interface test */
#define KBD_CCMD_SELF_TEST	0xAA	/* Controller self test */
#define KBD_CCMD_KBD_TEST	0xAB	/* Keyboard interface test */
#define KBD_CCMD_KBD_DISABLE	0xAD	/* Keyboard interface disable */
#define KBD_CCMD_KBD_ENABLE	0xAE	/* Keyboard interface enable */
#define KBD_CCMD_READ_INPORT    0xC0    /* read input port */
#define KBD_CCMD_READ_OUTPORT	0xD0    /* read output port */
#define KBD_CCMD_WRITE_OUTPORT	0xD1    /* write output port */
#define KBD_CCMD_WRITE_OBUF	0xD2
#define KBD_CCMD_WRITE_AUX_OBUF	0xD3    /* Write to output buffer as if
					   initiated by the auxiliary device */
#define KBD_CCMD_WRITE_MOUSE	0xD4	/* Write the following byte to the mouse */
#define KBD_CCMD_DISABLE_A20    0xDD    /* HP vectra only ? */
#define KBD_CCMD_ENABLE_A20     0xDF    /* HP vectra only ? */
#define KBD_CCMD_PULSE_BITS_3_0 0xF0    /* Pulse bits 3-0 of the output port P2. */
#define KBD_CCMD_RESET          0xFE    /* Pulse bit 0 of the output port P2 = CPU reset. */
#define KBD_CCMD_NO_OP          0xFF    /* Pulse no bits of the output port P2. */

/* Keyboard Commands */
#define KBD_CMD_SET_LEDS	0xED	/* Set keyboard leds */
#define KBD_CMD_ECHO     	0xEE
#define KBD_CMD_GET_ID 	        0xF2	/* get keyboard ID */
#define KBD_CMD_SET_RATE	0xF3	/* Set typematic rate */
#define KBD_CMD_ENABLE		0xF4	/* Enable scanning */
#define KBD_CMD_RESET_DISABLE	0xF5	/* reset and disable scanning */
#define KBD_CMD_RESET_ENABLE   	0xF6    /* reset and enable scanning */
#define KBD_CMD_RESET		0xFF	/* Reset */

/* Keyboard Replies */
#define KBD_REPLY_POR		0xAA	/* Power on reset */
#define KBD_REPLY_ACK		0xFA	/* Command ACK */
#define KBD_REPLY_RESEND	0xFE	/* Command NACK, send the cmd again */

/* Status Register Bits */
#define KBD_STAT_OBF 		0x01	/* Keyboard output buffer full */
#define KBD_STAT_IBF 		0x02	/* Keyboard input buffer full */
#define KBD_STAT_SELFTEST	0x04	/* Self test successful */
#define KBD_STAT_CMD		0x08	/* Last write was a command write (0=data) */
#define KBD_STAT_UNLOCKED	0x10	/* Zero if keyboard locked */
#define KBD_STAT_MOUSE_OBF	0x20	/* Mouse output buffer full */
#define KBD_STAT_GTO 		0x40	/* General receive/xmit timeout */
#define KBD_STAT_PERR 		0x80	/* Parity error */

/* Controller Mode Register Bits */
#define KBD_MODE_KBD_INT	0x01	/* Keyboard data generate IRQ1 */
#define KBD_MODE_MOUSE_INT	0x02	/* Mouse data generate IRQ12 */
#define KBD_MODE_SYS 		0x04	/* The system flag (?) */
#define KBD_MODE_NO_KEYLOCK	0x08	/* The keylock doesn't affect the keyboard if set */
#define KBD_MODE_DISABLE_KBD	0x10	/* Disable keyboard interface */
#define KBD_MODE_DISABLE_MOUSE	0x20	/* Disable mouse interface */
#define KBD_MODE_KCC 		0x40	/* Scan code conversion to PC format */
#define KBD_MODE_RFU		0x80

/* Output Port Bits */
#define KBD_OUT_RESET           0x01    /* 1=normal mode, 0=reset */
#define KBD_OUT_A20             0x02    /* x86 only */
#define KBD_OUT_OBF             0x10    /* Keyboard output buffer full */
#define KBD_OUT_MOUSE_OBF       0x20    /* Mouse output buffer full */

/* Mouse Commands */
#define AUX_SET_SCALE11		0xE6	/* Set 1:1 scaling */
#define AUX_SET_SCALE21		0xE7	/* Set 2:1 scaling */
#define AUX_SET_RES		0xE8	/* Set resolution */
#define AUX_GET_SCALE		0xE9	/* Get scaling factor */
#define AUX_SET_STREAM		0xEA	/* Set stream mode */
#define AUX_POLL		0xEB	/* Poll */
#define AUX_RESET_WRAP		0xEC	/* Reset wrap mode */
#define AUX_SET_WRAP		0xEE	/* Set wrap mode */
#define AUX_SET_REMOTE		0xF0	/* Set remote mode */
#define AUX_GET_TYPE		0xF2	/* Get type */
#define AUX_SET_SAMPLE		0xF3	/* Set sample rate */
#define AUX_ENABLE_DEV		0xF4	/* Enable aux device */
#define AUX_DISABLE_DEV		0xF5	/* Disable aux device */
#define AUX_SET_DEFAULT		0xF6
#define AUX_RESET		0xFF	/* Reset aux device */
#define AUX_ACK			0xFA	/* Command byte ACK. */

#define MOUSE_STATUS_REMOTE     0x40
#define MOUSE_STATUS_ENABLED    0x20
#define MOUSE_STATUS_SCALE21    0x10

#define KBD_PENDING_KBD         1
#define KBD_PENDING_AUX         2

/* update irq and KBD_STAT_[MOUSE_]OBF */
/* XXX: not generating the irqs if KBD_MODE_DISABLE_KBD is set may be
   incorrect, but it avoids having to simulate exact delays */
static void kbd_update_irq(KBDState *s)
{
    int irq_kbd_level, irq_mouse_level;

    irq_kbd_level = 0;
    irq_mouse_level = 0;
    s->status &= ~(KBD_STAT_OBF | KBD_STAT_MOUSE_OBF);
    s->outport &= ~(KBD_OUT_OBF | KBD_OUT_MOUSE_OBF);
    if (s->pending) {
        s->status |= KBD_STAT_OBF;
        s->outport |= KBD_OUT_OBF;
        /* kbd data takes priority over aux data.  */
        if (s->pending == KBD_PENDING_AUX) {
            s->status |= KBD_STAT_MOUSE_OBF;
            s->outport |= KBD_OUT_MOUSE_OBF;
            if (s->mode & KBD_MODE_MOUSE_INT)
                irq_mouse_level = 1;
        } else {
            if ((s->mode & KBD_MODE_KBD_INT) &&
                !(s->mode & KBD_MODE_DISABLE_KBD))
                irq_kbd_level = 1;
        }
    }
    pin_set_level(&s->irq_kbd, irq_kbd_level);
    pin_set_level(&s->irq_mouse, irq_mouse_level);
}

static void kbd_update_kbd_irq(Notifier *notifier, void *data)
{
    KBDState *s = container_of(notifier, KBDState, kbd_notifier);
    PS2State *dev = PS2_DEVICE(&s->kbd);

    if (pin_get_level(&dev->irq))
        s->pending |= KBD_PENDING_KBD;
    else
        s->pending &= ~KBD_PENDING_KBD;
    kbd_update_irq(s);
}

static void kbd_update_aux_irq(Notifier *notifier, void *data)
{
    KBDState *s = container_of(notifier, KBDState, mouse_notifier);
    PS2State *dev = PS2_DEVICE(&s->mouse);

    if (pin_get_level(&dev->irq))
        s->pending |= KBD_PENDING_AUX;
    else
        s->pending &= ~KBD_PENDING_AUX;
    kbd_update_irq(s);
}

static uint32_t kbd_read_status(KBDState *s, uint32_t addr)
{
    int val;
    val = s->status;
    DPRINTF("kbd: read status=0x%02x\n", val);
    return val;
}

static void kbd_queue(KBDState *s, int b, int aux)
{
    if (aux)
        ps2_queue(PS2_DEVICE(&s->mouse), b);
    else
        ps2_queue(PS2_DEVICE(&s->kbd), b);
}

static void outport_write(KBDState *s, uint32_t val)
{
    DPRINTF("kbd: write outport=0x%02x\n", val);
    s->outport = val;
    pin_set_level(&s->a20_out, (val >> 1) & 1);
    if (!(val & 1)) {
        qemu_system_reset_request();
    }
}

static void kbd_write_command(KBDState *s, uint32_t addr, uint32_t val)
{
    DPRINTF("kbd: write cmd=0x%02x\n", val);

    /* Bits 3-0 of the output port P2 of the keyboard controller may be pulsed
     * low for approximately 6 micro seconds. Bits 3-0 of the KBD_CCMD_PULSE
     * command specify the output port bits to be pulsed.
     * 0: Bit should be pulsed. 1: Bit should not be modified.
     * The only useful version of this command is pulsing bit 0,
     * which does a CPU reset.
     */
    if((val & KBD_CCMD_PULSE_BITS_3_0) == KBD_CCMD_PULSE_BITS_3_0) {
        if(!(val & 1))
            val = KBD_CCMD_RESET;
        else
            val = KBD_CCMD_NO_OP;
    }

    switch(val) {
    case KBD_CCMD_READ_MODE:
        kbd_queue(s, s->mode, 0);
        break;
    case KBD_CCMD_WRITE_MODE:
    case KBD_CCMD_WRITE_OBUF:
    case KBD_CCMD_WRITE_AUX_OBUF:
    case KBD_CCMD_WRITE_MOUSE:
    case KBD_CCMD_WRITE_OUTPORT:
        s->write_cmd = val;
        break;
    case KBD_CCMD_MOUSE_DISABLE:
        s->mode |= KBD_MODE_DISABLE_MOUSE;
        break;
    case KBD_CCMD_MOUSE_ENABLE:
        s->mode &= ~KBD_MODE_DISABLE_MOUSE;
        break;
    case KBD_CCMD_TEST_MOUSE:
        kbd_queue(s, 0x00, 0);
        break;
    case KBD_CCMD_SELF_TEST:
        s->status |= KBD_STAT_SELFTEST;
        kbd_queue(s, 0x55, 0);
        break;
    case KBD_CCMD_KBD_TEST:
        kbd_queue(s, 0x00, 0);
        break;
    case KBD_CCMD_KBD_DISABLE:
        s->mode |= KBD_MODE_DISABLE_KBD;
        kbd_update_irq(s);
        break;
    case KBD_CCMD_KBD_ENABLE:
        s->mode &= ~KBD_MODE_DISABLE_KBD;
        kbd_update_irq(s);
        break;
    case KBD_CCMD_READ_INPORT:
        kbd_queue(s, 0x00, 0);
        break;
    case KBD_CCMD_READ_OUTPORT:
        kbd_queue(s, s->outport, 0);
        break;
    case KBD_CCMD_ENABLE_A20:
        pin_raise(&s->a20_out);
        s->outport |= KBD_OUT_A20;
        break;
    case KBD_CCMD_DISABLE_A20:
        pin_lower(&s->a20_out);
        s->outport &= ~KBD_OUT_A20;
        break;
    case KBD_CCMD_RESET:
        qemu_system_reset_request();
        break;
    case KBD_CCMD_NO_OP:
        /* ignore that */
        break;
    default:
        fprintf(stderr, "qemu: unsupported keyboard cmd=0x%02x\n", val);
        break;
    }
}

static uint32_t kbd_read_data(KBDState *s, uint32_t addr)
{
    uint32_t val;

    if (s->pending == KBD_PENDING_AUX)
        val = ps2_read_data(PS2_DEVICE(&s->mouse));
    else
        val = ps2_read_data(PS2_DEVICE(&s->kbd));

    DPRINTF("kbd: read data=0x%02x\n", val);
    return val;
}

static void kbd_write_data(KBDState *s, uint32_t addr, uint32_t val)
{
    DPRINTF("kbd: write data=0x%02x\n", val);

    switch(s->write_cmd) {
    case 0:
        ps2_write_keyboard(&s->kbd, val);
        break;
    case KBD_CCMD_WRITE_MODE:
        s->mode = val;
        ps2_keyboard_set_translation(&s->kbd, (s->mode & KBD_MODE_KCC) != 0);
        /* ??? */
        kbd_update_irq(s);
        break;
    case KBD_CCMD_WRITE_OBUF:
        kbd_queue(s, val, 0);
        break;
    case KBD_CCMD_WRITE_AUX_OBUF:
        kbd_queue(s, val, 1);
        break;
    case KBD_CCMD_WRITE_OUTPORT:
        outport_write(s, val);
        break;
    case KBD_CCMD_WRITE_MOUSE:
        ps2_write_mouse(&s->mouse, val);
        break;
    default:
        break;
    }
    s->write_cmd = 0;
}

static void i8042_reset(DeviceState *dev)
{
    KBDState *s = I8042(dev);

    s->mode = KBD_MODE_KBD_INT | KBD_MODE_MOUSE_INT;
    s->status = KBD_STAT_CMD | KBD_STAT_UNLOCKED;
    s->outport = KBD_OUT_RESET | KBD_OUT_A20;
}

static const VMStateDescription vmstate_kbd = {
    .name = "pckbd",
    .version_id = 3,
    .minimum_version_id = 3,
    .minimum_version_id_old = 3,
    .fields      = (VMStateField []) {
        VMSTATE_UINT8(write_cmd, KBDState),
        VMSTATE_UINT8(status, KBDState),
        VMSTATE_UINT8(mode, KBDState),
        VMSTATE_UINT8(pending, KBDState),
        VMSTATE_END_OF_LIST()
    }
};

static uint64_t i8042_read(void *opaque, target_phys_addr_t addr,
                           unsigned size)
{
    KBDState *s = opaque;

    if ((addr >> s->it_shift) & 0x01) {
        return kbd_read_status(s, 0) & 0xff;
    } else {
        return kbd_read_data(s, 0) & 0xff;
    }
}

static void i8042_write(void *opaque, target_phys_addr_t addr,
                        uint64_t value, unsigned size)

{
    KBDState *s = opaque;

    if ((addr >> s->it_shift) & 0x01) {
        kbd_write_command(s, 0, value & 0xff);
    } else {
        kbd_write_data(s, 0, value & 0xff);
    }
}

static const MemoryRegionOps i8042_ops = {
    .endianness = DEVICE_NATIVE_ENDIAN,
    .read = i8042_read,
    .write = i8042_write,
};

static Property i8042_properties[] = {
    DEFINE_PROP_INT32("it_shift", KBDState, it_shift, 2),
    DEFINE_PROP_INT32("addr_shift", KBDState, addr_size, 8),
    DEFINE_PROP_END_OF_LIST(),
};

void i8042_mouse_fake_event(KBDState *s)
{
    ps2_mouse_fake_event(&s->mouse);
}

static int i8042_realize(DeviceState *dev)
{
    KBDState *s = I8042(dev);
    int err;

    err = qdev_init(DEVICE(&s->kbd));
    if (err < 0) {
        return err;
    }

    err = qdev_init(DEVICE(&s->mouse));
    if (err < 0) {
        return err;
    }

    memory_region_init_io(&s->io, &i8042_ops, s, "i8042", s->addr_size);

    s->kbd_notifier.notify = kbd_update_kbd_irq;
    pin_add_level_change_notifier(&s->kbd.common.irq, &s->kbd_notifier);

    s->mouse_notifier.notify = kbd_update_aux_irq;
    pin_add_level_change_notifier(&s->mouse.common.irq, &s->mouse_notifier);

    return 0;
}

static void i8042_initfn(Object *obj)
{
    KBDState *s = I8042(obj);

    object_initialize(&s->irq_kbd, TYPE_PIN);
    object_initialize(&s->irq_mouse, TYPE_PIN);
    object_initialize(&s->a20_out, TYPE_PIN);

    object_initialize(&s->kbd, TYPE_PS2_KBD);
    /* FIXME: make mouse a link<> */
    object_initialize(&s->mouse, TYPE_PS2_MOUSE);

    object_property_add_child(obj, "irq_kbd", OBJECT(&s->irq_kbd), NULL);
    object_property_add_child(obj, "irq_mouse", OBJECT(&s->irq_mouse), NULL);
    object_property_add_child(obj, "a20_out", OBJECT(&s->a20_out), NULL);

    object_property_add_child(obj, "kbd", OBJECT(&s->kbd), NULL);
    object_property_add_child(obj, "mouse", OBJECT(&s->mouse), NULL);

    qdev_prop_set_globals(DEVICE(&s->kbd));
    qdev_prop_set_globals(DEVICE(&s->mouse));
}

static void i8042_class_initfn(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);

    dc->init = i8042_realize;
    dc->reset = i8042_reset;
    dc->vmsd = &vmstate_kbd;
    dc->props = i8042_properties;
}

static TypeInfo i8042_info = {
    .name          = "i8042",
    .parent        = TYPE_DEVICE,
    .instance_init = i8042_initfn,
    .instance_size = sizeof(KBDState),
    .class_init    = i8042_class_initfn,
};

static void i8042_register_types(void)
{
    type_register_static(&i8042_info);
}

type_init(i8042_register_types)

KBDState *i8042_init(ISABus *isa_bus, int base, qemu_irq a20_line)
{
    KBDState *s;
    DeviceState *dev;

    s = I8042(object_new(TYPE_I8042));
    dev = DEVICE(s);
    qdev_prop_set_globals(dev);

    pin_connect_pin(&s->irq_kbd, isa_get_pin(isa_bus, 1));
    pin_connect_pin(&s->irq_mouse, isa_get_pin(isa_bus, 12));

    qdev_init_nofail(dev);
    memory_region_add_subregion_overlap(isa_bus->address_space_io, 0x60,
                                        &s->io, 0);
    pin_connect_qemu_irq(&s->a20_out, a20_line);

    return s;
}

void i8042_mm_init(MemoryRegion *address_space,
                   qemu_irq kbd_irq, qemu_irq mouse_irq,
                   target_phys_addr_t base, ram_addr_t size,
                   int32_t it_shift)
{
    DeviceState *dev;
    KBDState *s;

    s = I8042(object_new(TYPE_I8042));
    dev = DEVICE(s);

    qdev_prop_set_globals(dev);
    qdev_prop_set_int32(dev, "it_shift", it_shift);
    qdev_prop_set_int32(dev, "addr_size", size);

    qdev_init_nofail(dev);

    pin_connect_qemu_irq(&s->irq_kbd, kbd_irq);
    pin_connect_qemu_irq(&s->irq_mouse, mouse_irq);
    memory_region_add_subregion(address_space, base, &s->io);
}

