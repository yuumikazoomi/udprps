#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

/*
 *THIS IS PART OF THE NETWORK PROTOCOL AND SHOULD NOT BE TOUCHED
 */
int8_t makeaddr(const char* srvip,struct sockaddr_in* srv){
    
    srv->sin_family = AF_INET;
    srv->sin_port = htons(8030);
    if(inet_aton(srvip,&srv->sin_addr)==0){
        return 0;
    }
    return 1;
}
int8_t donat(int s, struct sockaddr_in* srv,socklen_t* slen,const char* targetip,uint8_t* buffer,size_t bufsize,size_t* trans){
    struct sockaddr_in target = {0};
    if(inet_aton(targetip, &target.sin_addr)==0){
        return 0;
    }
    if(sendto(s, &target.sin_addr.s_addr, sizeof(uint32_t), 0,(struct sockaddr*)srv,*slen)==-1){
        return 0;
    }
    *trans = recvfrom(s, buffer, bufsize, 0, (struct sockaddr*)srv, slen);
    if(*trans == -1){
        return 0;
    }
    
    return 1;
}
int8_t spam(int s,struct sockaddr_in* srv,socklen_t* slen,struct sockaddr_in* siother,socklen_t* slenother){
    struct timeval tv;
    tv.tv_sec = 2;
    tv.tv_usec = 0;
    setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof tv);
    uint8_t packet = 0xAB;
    for(;;){
        
        printf("signaling client...\n");
        for(int index = 0; index < 5; ++index){
            sleep(1);
            size_t trans = sendto(s, &packet, sizeof(packet), 0, (struct sockaddr*)siother, *slenother);
            if(trans==-1){
                return 0;
            }
        }
        packet = 0x0;
        for(int index = 0;  index < 10 ; ++index){
            if(recvfrom(s, &packet, sizeof(packet), 0, (struct sockaddr*)srv, slen)==-1){
                if ((errno != EAGAIN) && (errno != EWOULDBLOCK)){
                    return 0;
                }else{
                }
            }
        }
        if(packet == 0xAB){
            
            tv.tv_sec = 0;
            tv.tv_usec = 0;
            setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof tv);
            break;
        }
    }
    
    return 1;
}
int8_t doprotocol(int s, struct sockaddr_in* srv, socklen_t* slen,const char* targetip,int8_t* host){
    uint8_t buffer[11] = {0};
    size_t trans = 0;
    if(donat(s, srv, slen, targetip, buffer, sizeof(buffer), &trans)==0||trans!=sizeof(buffer)){
        return 0;
    }
    
    struct sockaddr_in siother = {0};
    socklen_t slenother = 0;
    memcpy(&siother.sin_addr.s_addr, buffer+sizeof(uint32_t), sizeof(uint32_t));
    memcpy(&siother.sin_port, buffer+(sizeof(uint32_t)*2), sizeof(uint16_t));
    *host = buffer[10];
    slenother = sizeof(siother);
    if(!spam(s,srv,slen,&siother,&slenother)){
        return 0;
    }
    
    return 1;
}



/*
 *THIS IS PART OF THE GAME PROTOCL AND CAN BE ALTERED TO WHATEVER GAME YOU WANT
 */
struct RpsNetworkContext{
    int s;
    struct sockaddr_in* srv;
    socklen_t slen;
};
enum RpsPacketId{
  kRpsNop = 0,
    kRpsMove = 0x11,
};
int forceread(struct RpsNetworkContext* rpsnc,uint8_t* buffer,size_t size){
    size_t trans = 0;
    do{
       trans = recvfrom(rpsnc->s, buffer, size, 0, (struct sockaddr*)rpsnc->srv, &rpsnc->slen);
        if(trans == -1){
            return 0;
        }
    }while(trans != size);
    return 1;
}
int forcewrite(struct RpsNetworkContext* rpsnc, uint8_t* buffer, size_t size){
    size_t trans = 0;
    do{
       trans = sendto(rpsnc->s, buffer, size, 0, (struct sockaddr*)rpsnc->srv, rpsnc->slen);
        if(trans == -1){
            return 0;
        }
    }while(trans != size);
    
    return 1;
}
int sendmove(struct RpsNetworkContext* rpsnc,uint32_t rps){
    enum RpsPacketId pid = kRpsMove;
    if(!forcewrite(rpsnc, (uint8_t*)&pid, sizeof(uint32_t))){
        return 0;
    }
    if(!forcewrite(rpsnc, (uint8_t*)&rps, sizeof(uint32_t))){
        return 0;
    }
    return 1;
}
void rpsgame(int8_t host,struct RpsNetworkContext* rpsnc){
    while(1){
        char choice[16] = {0};
        uint32_t rps = 0;
        do{
            printf("choose 1 for rock\t2 for paper\t3 for scissor:");
            fgets(choice, sizeof(choice), stdin);
            rps = (uint32_t)strtol(choice, NULL, 10);
        }while(rps<1||rps>3);
        if(!sendmove(rpsnc,rps)){
            return;
        }
        
        
        enum RpsPacketId pid = kRpsNop;
        
        
        if(!forceread(rpsnc,(uint8_t*)&pid,sizeof(uint32_t))){
            return;
        }
        printf("incoming packet:%x\n",pid);
        switch (pid) {
            case kRpsNop:
                break;
            case kRpsMove:{
                uint32_t appchoice = 0;
                if(!forceread(rpsnc, (uint8_t*)&appchoice, sizeof(uint32_t))){
                    return;
                }
                
                printf("you chose:%s\tapponent chose:%s\n",rps==1?"rock":rps>2?"scissor":"paper",
                       appchoice==1?"rock":appchoice>2?"scissor":"paper");
                
                if(rps==appchoice){
                    printf("Tie!\n");
                }else if(rps==1&&appchoice==2){
                    printf("you lose!\n");
                }else if(rps==1&&appchoice==3){
                    printf("you win!\n");
                }else if(rps==2&&appchoice==3){
                    printf("you lose!\n");
                }else if(rps==2&&appchoice==1){
                    printf("you win!\n");
                }else if(rps==3&&appchoice==1){
                    printf("you lose!\n");
                }else if(rps==3&&appchoice==2){
                    printf("you win!\n");
                }
            }
                break;
            default:
                printf("unkown packet\n");
                break;
        }
    }
}

int main(){

    char srvip[] = "18.168.115.193";
    struct sockaddr_in srv = {0};
    if(!makeaddr(srvip,&srv)){
        return -1;
    }
    socklen_t slen = sizeof(srv);
    int s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if(s == -1){
        return -1;
    }
    char targetip[20] = {0};
    
    targetip[strlen(targetip)-1] = 0;
    printf("target:");
    fgets(targetip, sizeof(targetip), stdin);
    
    int8_t host = 0;
    if(!doprotocol(s, &srv,&slen,targetip,&host)){
        close(s);
        return -1;
    }
    
    struct RpsNetworkContext rpsnc= {0};
    rpsnc.srv = &srv;
    rpsnc.slen = slen;
    rpsnc.s = s;
    rpsgame(host,&rpsnc);
    
    close(s);
}
