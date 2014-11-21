#ifndef JOS_KERN_E1000_H
#define JOS_KERN_E1000_H

#define TX_TCTL		0x00400	// Transmit control register
#define TX_TCTL_EN	1		// Transmit control enable bit
#define TX_TCTL_PSP	3		// Transmit control pad short packets bit
#define TX_TCTL_COLD	12		// Transmit control collision threshold
#define TX_TIPG		0x00410	// Transmit IPG register

#define TX_TDBAL	0x03800	// Transmit descriptor base address
#define TX_TDLEN	0x03808	// Transmit descriptor length
#define TX_TDH		0x03810	// Transmit descriptor head
#define TX_TDT		0x03818	// Transmit descriptor tail

#define TDESC_DEXT	5		// Legacy extension bit
#define TDESC_RS	3		// Report status bit
#define TDESC_EOP	0		// End of packet bit
#define TDESC_DD	0		// Descriptor done bit

#define NUM_TX		64		// Number of transmit descriptors

#include <kern/pci.h>

struct tx_desc {
    uint64_t addr;
    uint16_t length;
    uint8_t cso;
    uint8_t cmd;
    uint8_t status;
    uint8_t css;
    uint16_t special;
};

int
e1000_attachfn(struct pci_func *pcif);

int
e1000_transmit(uint64_t addr, uint16_t length);

#endif	// JOS_KERN_E1000_H
