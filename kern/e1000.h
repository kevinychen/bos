#ifndef JOS_KERN_E1000_H
#define JOS_KERN_E1000_H

#include <kern/pci.h>

int
e1000_attachfn(struct pci_func *pcif);

#endif	// JOS_KERN_E1000_H
