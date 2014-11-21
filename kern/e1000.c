#include <kern/e1000.h>

// Initialize the E1000
int
e1000_attachfn(struct pci_func *pcif) {
    pci_func_enable(pcif);
    return 0;
}
