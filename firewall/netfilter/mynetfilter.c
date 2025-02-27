#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/netfilter.h>
#include <linux/netfilter_ipv4.h>
#include <linux/ip.h>
#include <linux/tcp.h>
#include <linux/udp.h>
#include <linux/string.h>
#include <linux/rculist.h>
#include <linux/nsproxy.h>
#include <linux/proc_ns.h>
#include <linux/user_namespace.h>
#include <linux/net_namespace.h>

static struct nf_hook_ops *nf_blockport_ops = NULL;

static int vnet0id;
module_param(vnet0id, int, 0644);

static struct net *net;


static unsigned int nf_blockport(void *priv, struct sk_buff *skb, const struct nf_hook_state *state){

    
    if (!skb) {
        printk(KERN_INFO "mynetfilter: not skb \n");
		return NF_ACCEPT;
	} else {

        struct iphdr *iph;
        struct tcphdr *tcph;

        iph = ip_hdr(skb);

        if (iph->protocol == IPPROTO_TCP) {

            tcph = tcp_hdr(skb);

            if(ntohs(tcph->dest) == 9999) {

                printk(KERN_INFO "mynetfilter: drop port 9999 \n");

                return NF_DROP;
            }
        }

        return NF_ACCEPT;

	}
}


static int __init nf_knetfilter_init(void) {


    net = get_net_ns_by_id(&init_net, vnet0id);
    if (unlikely(!net)){
        printk(KERN_ERR "init mynetfilter failed\n");
        return -1;
    }

	nf_blockport_ops = (struct nf_hook_ops*)kcalloc(1, sizeof(struct nf_hook_ops), GFP_KERNEL);

    if (nf_blockport_ops != NULL) {
		nf_blockport_ops->hook = (nf_hookfn*)nf_blockport;
		nf_blockport_ops->hooknum = NF_INET_LOCAL_IN;        
		nf_blockport_ops->pf = NFPROTO_IPV4;
		nf_blockport_ops->priority = NF_IP_PRI_FIRST + 1;

		nf_register_net_hook(net, nf_blockport_ops);

        printk(KERN_INFO "init mynetfilter: block port\n");
	}
	return 0;
}

static void __exit nf_knetfilter_exit(void) {

    net = get_net_ns_by_id(&init_net, vnet0id);
    if (unlikely(!net)){
        printk(KERN_ERR "exit mynetfilter failed\n");
        return;
    }

	if (nf_blockport_ops  != NULL) {

		nf_unregister_net_hook(net, nf_blockport_ops);
		kfree(nf_blockport_ops);

        printk(KERN_INFO "exit mynetfilter: block port\n");
	}
}

module_init(nf_knetfilter_init);
module_exit(nf_knetfilter_exit);

MODULE_LICENSE("GPL");