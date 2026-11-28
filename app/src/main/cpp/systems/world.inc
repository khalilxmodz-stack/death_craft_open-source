    int groundHeight(int x,int z)const{return flatWorld?8:terrainHeight(x,z);}
    bool inWater()const{int x=(int)std::floor(px),z=(int)std::floor(pz);for(int y=(int)std::floor(feet+.15f);y<=(int)std::floor(feet+1.65f);y++)if(blockAt(x,y,z)==9)return true;return false;}
    std::string savePath()const{return saveDirectory+"/deathcraft_world_"+std::to_string(selectedSlot)+".dat";}
    void savePlayerName()const{if(saveDirectory.empty())return;std::ofstream out(saveDirectory+"/deathcraft_player_name.txt",std::ios::trunc);if(out)out<<playerName;}
    void loadPlayerName(){if(saveDirectory.empty())return;std::ifstream in(saveDirectory+"/deathcraft_player_name.txt");std::string value;if(std::getline(in,value)&&!value.empty()){if(value.size()>16)value.resize(16);playerName=value;}}
    void saveWorld(){
        if(!inWorld||saveDirectory.empty())return;
        std::ofstream out(savePath(),std::ios::binary|std::ios::trunc);if(!out)return;
        const char magic[8]={'D','C','W','O','R','L','D','1'};out.write(magic,8);
        int32_t version=2,mode=creativeMode?1:0,flat=flatWorld,slot=selectedSlot,hp=health,ap=armor,sel=selected;uint32_t count=(uint32_t)edits.size();
        out.write((char*)&version,4);out.write((char*)&mode,4);out.write((char*)&flat,4);out.write((char*)&slot,4);
        out.write((char*)&px,sizeof(px));out.write((char*)&feet,sizeof(feet));out.write((char*)&pz,sizeof(pz));out.write((char*)&yaw,sizeof(yaw));out.write((char*)&pitch,sizeof(pitch));
        out.write((char*)&hp,4);out.write((char*)&ap,4);out.write((char*)&sel,4);
        for(const auto&i:inventory){out.write((char*)&i.id,1);int32_t n=i.count;out.write((char*)&n,4);}
        out.write((char*)&count,4);
        for(const auto&e:edits){int32_t x=std::get<0>(e.first),y=std::get<1>(e.first),z=std::get<2>(e.first);uint8_t b=e.second;out.write((char*)&x,4);out.write((char*)&y,4);out.write((char*)&z,4);out.write((char*)&b,1);}
    }
    bool loadWorld(){
        std::ifstream in(savePath(),std::ios::binary);if(!in)return false;
        char magic[8]{};in.read(magic,8);if(!in||std::memcmp(magic,"DCWORLD1",8)!=0)return false;
        int32_t version=0,mode=1,flat=0,slot=0,hp=10,ap=10,sel=0;uint32_t count=0;
        in.read((char*)&version,4);in.read((char*)&mode,4);in.read((char*)&flat,4);in.read((char*)&slot,4);
        if(!in||(version!=1&&version!=2))return false;
        in.read((char*)&px,sizeof(px));in.read((char*)&feet,sizeof(feet));in.read((char*)&pz,sizeof(pz));in.read((char*)&yaw,sizeof(yaw));in.read((char*)&pitch,sizeof(pitch));
        in.read((char*)&hp,4);in.read((char*)&ap,4);in.read((char*)&sel,4);
        for(auto&i:inventory){int32_t n=0;in.read((char*)&i.id,1);in.read((char*)&n,4);i.count=std::clamp(n,0,999999);}
        in.read((char*)&count,4);if(!in||count>2000000)return false;
        edits.clear();for(uint32_t i=0;i<count;i++){int32_t x,y,z;uint8_t b;in.read((char*)&x,4);in.read((char*)&y,4);in.read((char*)&z,4);in.read((char*)&b,1);if(!in)return false;edits[{x,y,z}]=b;}
        creativeMode=mode!=0;spectatorMode=false;flatWorld=flat!=0;if(version==1)feet=(float)groundHeight((int)std::floor(px),(int)std::floor(pz));health=std::clamp(hp,0,10);armor=std::clamp(ap,0,10);selected=std::clamp(sel,0,4);return true;
    }
    void cycleGameMode(){if(!creativeMode&&!spectatorMode){creativeMode=true;spectatorMode=false;status="CREATIVE MODE";}else if(creativeMode){creativeMode=false;spectatorMode=true;status="SPECTATOR MODE";}else{creativeMode=false;spectatorMode=false;status="SURVIVAL MODE";}if(creativeMode)for(auto&i:inventory)if(i.id)i.count=999999;statusFrames=90;}
    void startWorld(){
        for(auto&entry:chunks)if(entry.second.meshVbo)glDeleteBuffers(1,&entry.second.meshVbo);
        chunks.clear();dirtyChunkQueue.clear();edits.clear();waterFlowDepth.clear();waterSimulationTimer=0;inventory={};health=10;armor=10;grafikHealth=6;grafikHitTimer=0;grafikAlive=true;playerDead=false;swallowTimer=0;
        px=0;pz=0;yaw=0;pitch=0;vy=0;feet=(float)groundHeight(0,0);grounded=true;
        bool loaded=loadWorld();spectatorMode=false;
        if(loaded){bool hasTnt=false,hasSword=false,hasPickaxe=false;for(const auto&i:inventory){if(i.id==10&&i.count>0)hasTnt=true;if(i.id==11&&i.count>0)hasSword=true;if(i.id==12&&i.count>0)hasPickaxe=true;}if(!hasTnt){if(inventory[4].id==5)inventory[8]=inventory[4];inventory[4]=Item{10,creativeMode?999999:4};}if(!hasSword)inventory[9]=Item{11,creativeMode?999999:1};if(!hasPickaxe)addItem(12,creativeMode?999999:1);}
        if(!loaded){
            px=0;pz=0;
            if(!flatWorld){for(int radius=0;radius<80&&groundHeight((int)px,(int)pz)<=SEA_LEVEL;radius++){px=(float)(radius+1);pz=(float)((radius%2)?radius/2:-radius/2);}}
            feet=(float)groundHeight((int)px,(int)pz);
            inventory[0]=creativeMode?Item{1,999999}:Item{1,32};inventory[1]=creativeMode?Item{2,999999}:Item{2,16};
            inventory[2]=creativeMode?Item{3,999999}:Item{3,12};inventory[3]=creativeMode?Item{4,999999}:Item{4,4};
            inventory[4]=creativeMode?Item{10,999999}:Item{10,4};inventory[5]=creativeMode?Item{6,999999}:Item{6,8};
            inventory[6]=creativeMode?Item{7,999999}:Item{7,0};inventory[7]=creativeMode?Item{8,999999}:Item{8,0};inventory[8]=creativeMode?Item{5,999999}:Item{5,8};inventory[9]=creativeMode?Item{11,999999}:Item{11,1};inventory[10]=creativeMode?Item{12,999999}:Item{12,1};
        }
        if(creativeMode)for(auto&i:inventory)if(i.id)i.count=999999;
        grafikX=6;grafikZ=-5;grafikFeet=(float)groundHeight(6,-5);skeletonX=px+8.0f;skeletonZ=pz+5.0f;skeletonFeet=(float)groundHeight((int)skeletonX,(int)skeletonZ);skeletonYaw=0;skeletonShootTimer=1.4f;skeletonHealth=8;skeletonAlive=true;skeletonHitTimer=0;skeletonRespawn=0;skeletonArrows.clear();khalilX=px+9.0f;khalilZ=pz+4.0f;khalilFeet=(float)groundHeight((int)khalilX,(int)khalilZ);khalilAlive=true;khalilHealth=8;khalilHitTimer=0;khalilPrimed=false;khalilFuse=0;khalilRespawn=0;explosionEffectTimer=0;deathMessage="GRAFIK ATE YOU";selected=0;inWorld=true;saveTimer=0;updateWorld();status=loaded?"WORLD LOADED":"WORLD CREATED";statusFrames=120;
    }
    uint8_t blockAt(int x,int y,int z)const{
        int cx=floorDiv(x,CS),cz=floorDiv(z,CS);auto it=chunks.find(chunkKey(cx,cz));
        return it==chunks.end()?0:it->second.get(modp(x,CS),y,modp(z,CS));
    }
    void addBlockFace(std::vector<float>&v,V3 a,V3 b,V3 c,V3 d,uint8_t block,int face,int gx,int gy,int gz,float blockLight=-1.0f){
        int tile=block==11?6:(block==10?5:(block==9?4:(block==3?2:(block==5?3:(block==4?7:(block==13?8:(block==14?9:(block==8?10:((block==1&&face==2)?0:1)))))))));
        float u0=(tile+.012f)/12.0f,u1=(tile+.988f)/12.0f,v0=.012f,v1=.988f;
        const float shade[6]={.78f,.86f,1.0f,.62f,.70f,.78f};
        float light=shade[face]*(blockLight>=0.0f?blockLight:1.0f);V3 color=block==18?V3{light,.20f*light,.24f*light}:((block==10||block==13||block==14||block==8)?V3{light,light,light}:(block==9?V3{.22f*light,.52f*light,.88f*light}:V3{light,light,light}));
        addTri(v,a,b,c,color,u0,v0,u1,v0,u1,v1);addTri(v,a,c,d,color,u0,v0,u1,v1,u0,v1);
    }
    bool solid(int x,int y,int z){
        int cx=floorDiv(x,CS),cz=floorDiv(z,CS);auto it=chunks.find(chunkKey(cx,cz));
        if(it==chunks.end())return y<0;
        uint8_t b=it->second.get(modp(x,CS),y,modp(z,CS));return b!=0&&b!=9;
    }
    float mobSurfaceHeight(float x,float z,float referenceFeet){
        int bx=(int)std::floor(x),bz=(int)std::floor(z);
        int top=std::min(HEIGHT-2,(int)std::floor(referenceFeet)+1);
        int bottom=std::max(0,(int)std::floor(referenceFeet)-2);
        for(int y=top;y>=bottom;y--)
            if(solid(bx,y,bz)&&!solid(bx,y+1,bz)&&!solid(bx,y+2,bz))return (float)(y+1);
        return -1000.0f;
    }
    bool mobCanStandAt(float x,float feetY,float z,float radius,float bodyHeight){
        int minY=(int)std::floor(feetY+.04f),maxY=(int)std::floor(feetY+bodyHeight-.04f);
        const float offsets[3]={-radius,0.0f,radius};
        for(int yi=minY;yi<=maxY;yi++)
            for(float ox:offsets)for(float oz:offsets)
                if(solid((int)std::floor(x+ox),(int)yi,(int)std::floor(z+oz)))return false;
        return true;
    }
    bool mobTryStep(float x,float z,float oldFeet,float radius,float bodyHeight,float& newFeet){
        float surface=mobSurfaceHeight(x,z,oldFeet);
        if(surface<0.0f||std::abs(surface-oldFeet)>1.05f)return false;
        if(!mobCanStandAt(x,surface,z,radius,bodyHeight))return false;
        newFeet=surface;return true;
    }
    void moveMobOnBlocks(float& x,float& z,float& feetY,float nx,float nz,float radius,float bodyHeight){
        float nextFeet=feetY;
        if(mobTryStep(nx,nz,feetY,radius,bodyHeight,nextFeet)){x=nx;z=nz;feetY=nextFeet;return;}
        if(mobTryStep(nx,z,feetY,radius,bodyHeight,nextFeet)){x=nx;feetY=nextFeet;}
        if(mobTryStep(x,nz,feetY,radius,bodyHeight,nextFeet)){z=nz;feetY=nextFeet;}
    }
    void markChunkDirty(int cx,int cz){
        auto it=chunks.find(chunkKey(cx,cz));
        if(it==chunks.end())return;
        if(!it->second.dirty){it->second.dirty=true;dirtyChunkQueue.push_back(chunkKey(cx,cz));}
    }
    void markBlockDirty(int x,int y,int z){
        (void)y;
        const int cx=floorDiv(x,CS),cz=floorDiv(z,CS),lx=modp(x,CS),lz=modp(z,CS);
        markChunkDirty(cx,cz);
        if(lx==0)markChunkDirty(cx-1,cz);
        if(lx==CS-1)markChunkDirty(cx+1,cz);
        if(lz==0)markChunkDirty(cx,cz-1);
        if(lz==CS-1)markChunkDirty(cx,cz+1);
        if(lx==0&&lz==0)markChunkDirty(cx-1,cz-1);
        if(lx==0&&lz==CS-1)markChunkDirty(cx-1,cz+1);
        if(lx==CS-1&&lz==0)markChunkDirty(cx+1,cz-1);
        if(lx==CS-1&&lz==CS-1)markChunkDirty(cx+1,cz+1);
    }
    void generate(Chunk&c){
        int ox=c.cx*CS,oz=c.cz*CS;
        for(int z=0;z<CS;z++)for(int x=0;x<CS;x++){
            int wx=ox+x,wz=oz+z,h=groundHeight(wx,wz);
            for(int y=0;y<h;y++){
                bool cave=false;
                if(y>2&&y<h-3){
                    float tunnel=terrainNoise((wx+y*1.65f)*.067f+31.0f,(wz-y*.92f)*.067f-19.0f);
                    float chamber=terrainNoise((wx-y*.48f)*.105f-73.0f,(wz+y*.62f)*.105f+47.0f);
                    cave=tunnel>.69f&&chamber>.48f;
                }
                if(y==0){c.set(x,y,z,19);continue;}if(!cave){uint8_t block=y==h-1?1:(y>=h-11?2:3);if(block==3&&y>2&&y<h-12&&terrainHash(wx*17+y*31,wz*13-y*7)%173u==0u)block=13;c.set(x,y,z,block);}
            }
            if(h<SEA_LEVEL)for(int y=h;y<SEA_LEVEL;y++)c.set(x,y,z,9);
        }
        auto put=[&](int x,int y,int z,uint8_t id){if(floorDiv(x,CS)==c.cx&&floorDiv(z,CS)==c.cz&&y>=0&&y<HEIGHT)c.set(modp(x,CS),y,modp(z,CS),id);};
        const int caveCell=52;
        for(int gz=floorDiv(oz-55,caveCell);gz<=floorDiv(oz+CS+55,caveCell);gz++)for(int gx=floorDiv(ox-55,caveCell);gx<=floorDiv(ox+CS+55,caveCell);gx++){
            uint32_t seed=terrainHash(gx*71+193,gz*97-251);if(seed%5u!=0u)continue;
            int ex=gx*caveCell+8+(int)((seed>>8)%36u),ez=gz*caveCell+8+(int)((seed>>16)%36u),surface=groundHeight(ex,ez);if(surface<SEA_LEVEL+2||surface>HEIGHT-9)continue;
            float angle=(float)((seed>>4)&1023u)/1024.0f*2.0f*PI,dx=std::cos(angle),dz=std::sin(angle);bool deep=(seed&0x100000u)!=0;int length=deep?27:10;
            for(int t=0;t<=length;t++){float cx=ex+dx*t*.62f,cz=ez+dz*t*.62f,cy=surface-1.0f-t*(deep?.52f:.38f),radius=t<3?2.45f:(deep?2.0f:1.45f);
                for(int vz=(int)cz-3;vz<=(int)cz+3;vz++)for(int vx=(int)cx-3;vx<=(int)cx+3;vx++)for(int vy=(int)cy-2;vy<=(int)cy+2;vy++){float nx=(vx+.5f-cx)/radius,nz=(vz+.5f-cz)/radius,ny=(vy+.5f-cy)/(radius*.78f);if(nx*nx+ny*ny+nz*nz<1.0f)put(vx,vy,vz,0);}}
            if(deep){float cx=ex+dx*length*.62f,cz=ez+dz*length*.62f,cy=surface-1.0f-length*.52f;for(int vz=(int)cz-5;vz<=(int)cz+5;vz++)for(int vx=(int)cx-5;vx<=(int)cx+5;vx++)for(int vy=(int)cy-4;vy<=(int)cy+4;vy++){float nx=(vx+.5f-cx)/4.2f,nz=(vz+.5f-cz)/4.2f,ny=(vy+.5f-cy)/3.3f;if(nx*nx+ny*ny+nz*nz<1.0f)put(vx,vy,vz,0);}}
        }
        // Four deterministic silhouettes: oak, pine, broad crown and slim birch-like tree.
        for(int z=oz-5;z<oz+CS+5;z++)for(int x=ox-5;x<ox+CS+5;x++){
            if(modp(x+3,8)||modp(z+5,8))continue;
            uint32_t h=terrainHash(x,z);if(h%4==0)continue;
            int ground=groundHeight(x,z);if(ground<SEA_LEVEL||ground>48)continue;
            int type=(h>>5)%4;
            int trunk=type==1?5+(int)((h>>9)%2):(type==3?4:3+(int)((h>>9)%2));
            for(int y=0;y<trunk;y++)put(x,ground+y,z,4);
            int top=ground+trunk;
            if(type==1){
                for(int layer=0;layer<4;layer++){
                    int radius=3-layer,yy=top-2+layer;
                    for(int dz=-radius;dz<=radius;dz++)for(int dx=-radius;dx<=radius;dx++)
                        if(std::abs(dx)+std::abs(dz)<=radius+1)put(x+dx,yy,z+dz,5);
                }
                put(x,top+2,z,5);
            }else if(type==2){
                for(int dy=-1;dy<=2;dy++){
                    int radius=(dy==2?1:(dy==-1?2:3));
                    for(int dz=-radius;dz<=radius;dz++)for(int dx=-radius;dx<=radius;dx++)
                        if(dx*dx+dz*dz<=radius*radius+1)put(x+dx,top+dy,z+dz,5);
                }
            }else if(type==3){
                for(int dy=-1;dy<=1;dy++)for(int dx=-1;dx<=1;dx++)for(int dz=-1;dz<=1;dz++)
                    if(std::abs(dx)+std::abs(dz)<=1)put(x+dx,top+dy,z+dz,5);
                put(x,top+1,z,5);
            }else{
                for(int dy=-1;dy<=1;dy++)for(int dz=-1;dz<=1;dz++)for(int dx=-1;dx<=1;dx++){
                    if(std::abs(dx)==1&&std::abs(dz)==1&&dy!=1)continue;
                    if(dx==0&&dz==0&&dy<=0)continue;
                    put(x+dx,top+dy,z+dz,5);
                }
                put(x,top+1,z,5);
            }
        }
        for(const auto& e:edits){
            int x=std::get<0>(e.first), y=std::get<1>(e.first), z=std::get<2>(e.first);
            if(floorDiv(x,CS)==c.cx && floorDiv(z,CS)==c.cz)
                c.set(modp(x,CS),y,modp(z,CS),e.second);
        }
        c.dirty=true;
    }
    Chunk& ensureChunk(int cx,int cz){
        uint64_t k=chunkKey(cx,cz);auto it=chunks.find(k);
        if(it==chunks.end()){
            Chunk c;c.cx=cx;c.cz=cz;generate(c);
            it=chunks.emplace(k,std::move(c)).first;
            dirtyChunkQueue.push_back(k);
        }
        return it->second;
    }
    V3 blockColor(uint8_t b,int face){
        if(b==19)return{.12f,.12f,.14f};if(b==9)return{.12f,.42f,.82f};if(b==1){if(face==2)return{.22f,.68f,.18f};return{.42f,.29f,.16f};}
        if(b==2)return{.48f,.31f,.17f};if(b==3)return{.43f,.45f,.48f};
        if(b==4)return{.43f,.27f,.12f};if(b==5)return{.12f,.48f,.13f};if(b==6)return{.67f,.46f,.24f};if(b==13)return{.72f,.68f,.62f};if(b==14)return{.78f,.80f,.83f};if(b==8)return{.70f,.48f,.25f};if(b==10||b==11)return{1,1,1};if(b==18)return{.92f,.20f,.24f};return{.72f,.62f,.35f};
    }
    float breakDuration(uint8_t b)const{uint8_t tool=selected>=0&&selected<(int)inventory.size()&&inventory[selected].count>0?inventory[selected].id:0;bool ironPick=tool==12,woodPick=tool==16,axe=tool==15;if((ironPick||woodPick)&&b==3)return ironPick?.24f:.95f;if(ironPick&&b==13)return .38f;if(axe&&b==4)return .28f;if((ironPick||woodPick)&&b==2)return ironPick?.30f:.70f;switch(b){case 1:return .65f;case 2:return .90f;case 3:return 2.10f;case 4:return 1.25f;case 5:return .30f;case 6:return 1.10f;case 8:return .90f;case 10:return .25f;case 11:return .25f;case 13:return 2.8f;case 14:return 3.0f;default:return .85f;}}
