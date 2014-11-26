#include "ns.h"
#include "inc/lib.h"

#define BUF_SIZE	128
union Nsipc bufs[BUF_SIZE] __attribute__((aligned(PGSIZE)));

void
input(envid_t ns_envid)
{
	binaryname = "ns_input";

    int result;
    union Nsipc *buf = bufs;
    while (true) {
        if ((result = sys_net_receive(buf->pkt.jp_data)) >= 0) {
            buf->pkt.jp_len = result;
            ipc_send(ns_envid, NSREQ_INPUT, buf, PTE_U | PTE_P);
            if (++buf == bufs + BUF_SIZE)
                buf = bufs;
        }
    }
}
