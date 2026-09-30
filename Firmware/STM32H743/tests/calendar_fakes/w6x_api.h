#include <stdint.h>
#include <stddef.h>
#include <sys/types.h>
#define W6X_STATUS_OK 0
#define W6X_NET_TLS_CREDENTIAL_CA_CERTIFICATE 0
#define AF_INET 2
#define SOCK_STREAM 1
#define IPPROTO_TLS_1_2 258
#define SOL_TLS 282
#define SOL_SOCKET 1
#define TLS_SEC_TAG_LIST 1
#define TLS_HOSTNAME 2
#define TLS_ALPN_LIST 3
#define SO_RCVTIMEO 20
#define SO_RCVBUF 8
#define PP_HTONS(x) ((((x)&255)<<8)|((x)>>8))
struct sockaddr { uint16_t family; };
struct sockaddr_in { uint16_t sin_family,sin_port; struct { uint32_t s_addr; } sin_addr; };
int W6X_Net_ResolveHostAddress(const char *,uint8_t *);
int W6X_Net_Socket(int,int,int);
int W6X_Net_TLS_Credential_AddByContent(uint32_t,int,const char *,const char *,uint32_t);
int W6X_Net_TLS_Credential_Delete(uint32_t,int);
int W6X_Net_Setsockopt(int,int,int,const void *,size_t);
int W6X_Net_Connect(int,const struct sockaddr *,size_t);
ssize_t W6X_Net_Send(int,const void *,size_t,int);
ssize_t W6X_Net_Recv(int,void *,size_t,int);
int W6X_Net_Close(int);
int W6X_Net_SNTP_SetConfiguration(uint8_t,int16_t,uint8_t *,uint8_t *,uint8_t *);
