#include <inc/error.h>

#include <kern/e1000.h>
#include <kern/pmap.h>

volatile uint32_t *e1000_mem;
struct tx_desc transmit_descriptors[NUM_TX];

// Initialize the E1000
int
e1000_attachfn(struct pci_func *pcif) {
    // Enable the E1000
    pci_func_enable(pcif);

    // Allocate memory mapping
    e1000_mem = (uint32_t*) mmio_map_region(
            pcif->reg_base[0], pcif->reg_size[0]);

    // Initialize transmit descriptor array
    e1000_mem[TX_TDBAL >> 2] = PADDR(transmit_descriptors);
    e1000_mem[TX_TDLEN >> 2] = NUM_TX * sizeof(struct tx_desc);
    e1000_mem[TX_TDH >> 2] = 0;
    e1000_mem[TX_TDT >> 2] = 0;
    e1000_mem[TX_TCTL >> 2] =
        (1 << TX_TCTL_EN) | (1 << TX_TCTL_PSP) | (0x40 << TX_TCTL_COLD);
    int i;
    for (i = 0; i < NUM_TX; i++)
        transmit_descriptors[i].status |= (1 << TDESC_DD);

    return 0;
}

// Transmit a packet
int
e1000_transmit(uint64_t addr, uint16_t length) {
    struct tx_desc *desc = &transmit_descriptors[e1000_mem[TX_TDT >> 2]];
    if (!(desc->status & (1 << TDESC_DD)))
        return -E_NO_MEM;

    desc->addr = addr;
    desc->length = length;
    desc->cmd = (1 << TDESC_RS) | (1 << TDESC_EOP);
    desc->status &= ~(1 << TDESC_DD);
    e1000_mem[TX_TDT >> 2] = (e1000_mem[TX_TDT >> 2] + 1) % NUM_TX;

    return 0;
}
