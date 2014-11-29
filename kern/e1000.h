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

#define RX_RCTL	0x00100	// Receive control register
#define RX_RCTL_EN	1		// Receive control enable bit
#define RX_RCTL_BAM	15		// Receive control broadcast accept mode bit
#define RX_RCTL_BSIZE	16		// Receive control buffer size bit
#define RX_RCTL_SECRC	26		// Receive control strip ethernet CRC bit

#define RX_RDBAL	0x02800	// Receive descriptor base address
#define RX_RDLEN	0x02808	// Receive descriptor length
#define RX_RDH		0x02810	// Receive address head
#define RX_RDT		0x02818	// Receive address tail

#define RX_MTA		0x05200	// Multicast table array
#define RX_RAL		0x05400	// Receive address low
#define RX_RAH		0x05404	// Receive address high
#define RX_RAH_AV	31		// Receive address valid

#define RDESC_DD	0		// Descriptor done bit

#define NUM_TX		64		// Number of transmit descriptors
#define NUM_RX		128		// Number of receive descriptors
#define MAX_PACKET_LEN	16288	// Max length of transmit descriptor
#define MAX_PACKET_BUF	2048	// Max length of received packet

#define EC_EERD		0x00014 // EEPROM read
#define EC_EERD_START	0		// EEPROM read start
#define EC_EERD_DONE	4		// EEPROM read done
#define EC_EERD_ADDR	8		// EEPROM read address
#define EC_EERD_DATA	16		// EEPROM read data

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

struct rx_desc {
    uint64_t addr;
    uint16_t length;
    uint16_t cso;
    uint8_t status;
    uint8_t errors;
    uint16_t special;
};

int
e1000_attachfn(struct pci_func *pcif);

int
e1000_mac_addr_low();

int
e1000_mac_addr_high();

int
e1000_transmit(uint64_t addr, uint16_t length);

int
e1000_receive(uint64_t addr);

#endif	// JOS_KERN_E1000_H
