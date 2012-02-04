/*
 * QEMU Common PCI Host bridge configuration data space access routines.
 *
 * Copyright (c) 2006 Fabrice Bellard
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

/* Worker routines for a PCI host controller that uses an {address,data}
   register pair to access PCI configuration space.  */

#ifndef PCI_HOST_H
#define PCI_HOST_H

#include "sysbus.h"

#define TYPE_PCI_HOST "pci-host"
#define PCI_HOST(obj) OBJECT_CHECK(PCIHostState, (obj), TYPE_PCI_HOST)

struct PCIHostState {
    SysBusDevice busdev;
    MemoryRegion conf_mem;
    MemoryRegion data_mem;
    MemoryRegion mmcfg;
    MemoryRegion *address_space;
    uint32_t config_reg;
    PCIBus *bus;
};

/* common internal helpers for PCI/PCIe hosts, cut off overflows */
void pci_host_config_write_common(PCIDevice *pci_dev, uint32_t addr,
                                  uint32_t limit, uint32_t val, uint32_t len);
uint32_t pci_host_config_read_common(PCIDevice *pci_dev, uint32_t addr,
                                     uint32_t limit, uint32_t len);

void pci_data_write(PCIBus *s, uint32_t addr, uint32_t val, int len);
uint32_t pci_data_read(PCIBus *s, uint32_t addr, int len);

void pci_host_set_mmio(PCIHostState *s, MemoryRegion *value);

extern const MemoryRegionOps pci_host_conf_le_ops;
extern const MemoryRegionOps pci_host_conf_be_ops;
extern const MemoryRegionOps pci_host_data_le_ops;
extern const MemoryRegionOps pci_host_data_be_ops;

#endif /* PCI_HOST_H */
