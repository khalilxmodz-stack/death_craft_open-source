    void setLanMode(int mode){
        if(lanSocket<0){
            lanSocket=socket(AF_INET,SOCK_DGRAM,0);
            if(lanSocket<0){status="LAN SOCKET ERROR";statusFrames=120;return;}
            int yes=1;setsockopt(lanSocket,SOL_SOCKET,SO_REUSEADDR,&yes,sizeof(yes));
#ifdef SO_REUSEPORT
            setsockopt(lanSocket,SOL_SOCKET,SO_REUSEPORT,&yes,sizeof(yes));
#endif
            setsockopt(lanSocket,SOL_SOCKET,SO_BROADCAST,&yes,sizeof(yes));
            sockaddr_in local{};local.sin_family=AF_INET;local.sin_port=htons(27015);local.sin_addr.s_addr=htonl(INADDR_ANY);
            if(bind(lanSocket,(sockaddr*)&local,sizeof(local))<0){close(lanSocket);lanSocket=-1;status="LAN PORT BUSY";statusFrames=120;return;}
            fcntl(lanSocket,F_SETFL,fcntl(lanSocket,F_GETFL,0)|O_NONBLOCK);
            networkId=(uint32_t)getpid()^(uint32_t)time(nullptr);if(!networkId)networkId=1;
            lanBroadcast.sin_family=AF_INET;lanBroadcast.sin_port=htons(27015);lanBroadcast.sin_addr.s_addr=htonl(INADDR_BROADCAST);
        }
        lanMode=mode;remoteValid=false;remoteTimer=0;lanTimer=0;helloTimer=0;
        status=mode==1?"LAN HOSTING - WAITING":"LAN SEARCHING FOR HOST";statusFrames=180;
    }
    void sendLan(const LanPacket&p){if(lanSocket>=0)sendto(lanSocket,&p,sizeof(p),0,(sockaddr*)&lanBroadcast,sizeof(lanBroadcast));}
    void updateLan(float dt){
        if(lanSocket<0||lanMode==0)return;
        LanPacket p{};sockaddr_in from{};socklen_t fromLen=sizeof(from);
        while(recvfrom(lanSocket,&p,sizeof(p),0,(sockaddr*)&from,&fromLen)==sizeof(p)){
            if(p.magic!=0x44434654u||p.player==networkId)continue;
            if(p.type==1&&lanMode==1){LanPacket reply{};reply.type=2;reply.player=networkId;reply.x=px;reply.y=feet;reply.z=pz;reply.yaw=yaw;std::strncpy(reply.name,playerName.c_str(),sizeof(reply.name)-1);sendLan(reply);}
            else if(p.type==2){remoteValid=true;remoteTimer=0;remoteX=p.x;remoteY=p.y;remoteZ=p.z;remoteYaw=p.yaw;remoteName=p.name[0]?p.name:"PLAYER";}
            else if(p.type==3){int cx=floorDiv(p.bx,CS),cz=floorDiv(p.bz,CS);Chunk&c=ensureChunk(cx,cz);c.set(modp(p.bx,CS),p.by,modp(p.bz,CS),p.block);edits[{p.bx,p.by,p.bz}]=p.block;for(auto&entry:chunks)entry.second.dirty=true;}
            else if(p.type==4&&lanMode==2){khalilX=p.x;khalilFeet=p.y;khalilZ=p.z;khalilAlive=(p.flags&1)!=0;khalilPrimed=(p.flags&2)!=0;}
            else if(p.type==5&&lanMode==2){
                explosionX=p.x;explosionY=p.y;explosionZ=p.z;explosionEffectTimer=.62f;
                float dx=px-explosionX,dy=feet+.85f-explosionY,dz=pz-explosionZ,dist=std::sqrt(dx*dx+dy*dy+dz*dz);
                if(dist<4.5f&&!creativeMode){int damage=dist<2.0f?10:(dist<3.3f?6:3);health=std::max(0,health-damage);if(health<=0){playerDead=true;swallowTimer=.001f;deathMessage="KHALIL EXPLODED";}status="KHALIL EXPLOSION!";statusFrames=120;}
            }
            fromLen=sizeof(from);
        }
        lanTimer+=dt;helloTimer+=dt;remoteTimer+=dt;
        if(lanTimer>=.10f){lanTimer=0;LanPacket state{};state.type=2;state.player=networkId;state.x=px;state.y=feet;state.z=pz;state.yaw=yaw;std::strncpy(state.name,playerName.c_str(),sizeof(state.name)-1);sendLan(state);}
        if(lanMode==2&&helloTimer>=.50f){helloTimer=0;LanPacket hello{};hello.type=1;hello.player=networkId;sendLan(hello);}
        if(lanMode==1&&lanTimer==0.0f){LanPacket monster{};monster.type=4;monster.player=networkId;monster.x=khalilX;monster.y=khalilFeet;monster.z=khalilZ;monster.flags=(khalilAlive?1:0)|(khalilPrimed?2:0);sendLan(monster);}
        if(remoteTimer>3.0f)remoteValid=false;
    }

