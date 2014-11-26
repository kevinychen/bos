#include <inc/error.h>
#include <inc/string.h>

#include <kern/e1000.h>
#include <kern/pmap.h>

volatile uint32_t *e1000_mem;
struct tx_desc transmit_descriptors[NUM_TX];
struct rx_desc receive_descriptors[NUM_RX];
char buf[NUM_RX][MAX_PACKET_BUF];

// Initialize the E1000
int
e1000_attachfn(struct pci_func *pcif) {
    int i;

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
    for (i = 0; i < NUM_TX; i++)
        transmit_descriptors[i].status |= (1 << TDESC_DD);

    // Initialize receive descriptor array
    e1000_mem[RX_RAL >> 2] = 0x12005452;
    e1000_mem[RX_RAH >> 2] = 0x80005634;
    e1000_mem[RX_MTA >> 2] = 0;
    e1000_mem[RX_RDBAL >> 2] = PADDR(receive_descriptors);
    e1000_mem[RX_RDLEN >> 2] = NUM_RX * sizeof(struct rx_desc);
    e1000_mem[RX_RDH >> 2] = 0;
    e1000_mem[RX_RDT >> 2] = NUM_RX - 1;
    e1000_mem[RX_RCTL >> 2] =
        (1 << RX_RCTL_EN) | (1 << RX_RCTL_BAM) | (0 << RX_RCTL_BSIZE) |
        (1 << RX_RCTL_SECRC);
    for (i = 0; i < NUM_RX; i++)
        receive_descriptors[i].addr = PADDR(buf[i]);

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

// Receive a packet
int
e1000_receive(uint64_t addr) {
    uint32_t new_rdt = (e1000_mem[RX_RDT >> 2] + 1) % NUM_RX;
    struct rx_desc desc = receive_descriptors[new_rdt];
    if (!(desc.status & (1 << RDESC_DD)))
        return -E_NO_MEM;

    memcpy(KADDR(addr), buf[new_rdt], desc.length);
    desc.status &= ~(1 << TDESC_DD);
    e1000_mem[RX_RDT >> 2] = new_rdt;

    return desc.length;
}
