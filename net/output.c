#include "ns.h"
#include "inc/lib.h"

extern union Nsipc nsipcbuf;

void
output(envid_t ns_envid)
{
	binaryname = "ns_output";

    int result;
    while ((result = ipc_recv(NULL, &nsipcbuf, NULL, ns_envid)) >= 0) {
        struct jif_pkt *pkt = &nsipcbuf.pkt;
        sys_net_transmit(pkt->jp_data, pkt->jp_len);
    }

    panic("ipc recv returned %e", result);
}
