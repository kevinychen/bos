#include <kern/e1000.h>
#include <kern/pmap.h>

volatile uint32_t *e1000_mem;

// Initialize the E1000
int
e1000_attachfn(struct pci_func *pcif) {
    // Enable the E1000
    pci_func_enable(pcif);

    // Allocate memory mapping
    e1000_mem = (uint32_t*) mmio_map_region(
            pcif->reg_base[0], pcif->reg_size[0]);

    return 0;
}
