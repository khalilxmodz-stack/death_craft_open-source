    int groundHeight(int x,int z)const{return flatWorld?8:terrainHeight(x,z);}
    bool inWater()const{int x=(int)std::floor(px),z=(int)std::floor(pz);for(int y=(int)std::floor(feet+.15f);y<=(int)std::floor(feet+1.65f);y++)if(blockAt(x,y,z)==9)return true;return false;}
    std::string savePath()const{return saveDirectory+"/deathcraft_world_"+std::to_string(selectedSlot)+".dat";}
    void savePlayerName()const{if(saveDirectory.empty())return;std::ofstream out(saveDirectory+"/deathcraft_player_name.txt",std::ios::trunc);if(out)out<<playerName;}
    void loadPlayerName(){if(saveDirectory.empty())return;std::ifstream in(saveDirectory+"/deathcraft_player_name.txt");std::string value;if(std::getline(in,value)&&!value.empty()){if(value.size()>16)value.resize(16);playerName=value;}}
    void saveWorld(){
        if(!inWorld||saveDirectory.empty())return;
        std::ofstream out(savePath(),std::ios::binary|std::ios::trunc);if(!out)return;
        const char magic[8]={'D','C','W','O','R','L','D','1'};out.write(magic,8);
        int32_t version=1,mode=creativeMode?1:0,flat=flatWorld,slot=selectedSlot,hp=health,ap=armor,sel=selected;uint32_t count=(uint32_t)edits.size();
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
        if(!in||version!=1)return false;
        in.read((char*)&px,sizeof(px));in.read((char*)&feet,sizeof(feet));in.read((char*)&pz,sizeof(pz));in.read((char*)&yaw,sizeof(yaw));in.read((char*)&pitch,sizeof(pitch));
        in.read((char*)&hp,4);in.read((char*)&ap,4);in.read((char*)&sel,4);
        for(auto&i:inventory){int32_t n=0;in.read((char*)&i.id,1);in.read((char*)&n,4);i.count=std::clamp(n,0,999999);}
        in.read((char*)&count,4);if(!in||count>2000000)return false;
        edits.clear();for(uint32_t i=0;i<count;i++){int32_t x,y,z;uint8_t b;in.read((char*)&x,4);in.read((char*)&y,4);in.read((char*)&z,4);in.read((char*)&b,1);if(!in)return false;edits[{x,y,z}]=b;}
        creativeMode=mode!=0;flatWorld=flat!=0;health=std::clamp(hp,0,10);armor=std::clamp(ap,0,10);selected=std::clamp(sel,0,4);return true;
    }
    void startWorld(){
        chunks.clear();edits.clear();waterFlowDepth.clear();waterSimulationTimer=0;inventory={};health=10;armor=10;grafikHealth=6;grafikHitTimer=0;grafikAlive=true;playerDead=false;swallowTimer=0;
        px=0;pz=0;yaw=0;pitch=0;vy=0;feet=(float)groundHeight(0,0);grounded=true;
        bool loaded=loadWorld();
        if(loaded){bool hasTnt=false,hasSword=false;for(const auto&i:inventory){if(i.id==10&&i.count>0)hasTnt=true;if(i.id==11&&i.count>0)hasSword=true;}if(!hasTnt){if(inventory[4].id==5)inventory[8]=inventory[4];inventory[4]=Item{10,creativeMode?999999:4};}if(!hasSword)inventory[9]=Item{11,creativeMode?999999:1};}
        if(!loaded){
            px=0;pz=0;
            if(!flatWorld){for(int radius=0;radius<80&&groundHeight((int)px,(int)pz)<=SEA_LEVEL;radius++){px=(float)(radius+1);pz=(float)((radius%2)?radius/2:-radius/2);}}
            feet=(float)groundHeight((int)px,(int)pz);
            inventory[0]=creativeMode?Item{1,999999}:Item{1,32};inventory[1]=creativeMode?Item{2,999999}:Item{2,16};
            inventory[2]=creativeMode?Item{3,999999}:Item{3,12};inventory[3]=creativeMode?Item{4,999999}:Item{4,4};
            inventory[4]=creativeMode?Item{10,999999}:Item{10,4};inventory[5]=creativeMode?Item{6,999999}:Item{6,8};
            inventory[6]=creativeMode?Item{7,999999}:Item{7,0};inventory[7]=creativeMode?Item{8,999999}:Item{8,0};inventory[8]=creativeMode?Item{5,999999}:Item{5,8};inventory[9]=creativeMode?Item{11,999999}:Item{11,1};
        }
        if(creativeMode)for(auto&i:inventory)if(i.id)i.count=999999;
        grafikX=6;grafikZ=-5;grafikFeet=(float)groundHeight(6,-5);khalilX=px+9.0f;khalilZ=pz+4.0f;khalilFeet=(float)groundHeight((int)khalilX,(int)khalilZ);khalilAlive=true;khalilHealth=8;khalilHitTimer=0;khalilPrimed=false;khalilFuse=0;khalilRespawn=0;explosionEffectTimer=0;deathMessage="GRAFIK ATE YOU";selected=0;inWorld=true;saveTimer=0;updateWorld();status=loaded?"WORLD LOADED":"WORLD CREATED";statusFrames=120;
    }
    uint8_t blockAt(int x,int y,int z)const{
        int cx=floorDiv(x,CS),cz=floorDiv(z,CS);auto it=chunks.find(chunkKey(cx,cz));
        return it==chunks.end()?0:it->second.get(modp(x,CS),y,modp(z,CS));
    }
    void addBlockFace(std::vector<float>&v,V3 a,V3 b,V3 c,V3 d,uint8_t block,int face,int gx,int gy,int gz){
        int tile=block==11?6:(block==10?5:(block==9?4:(block==3?2:(block==5?3:((block==1&&face==2)?0:1)))));
        float u0=(tile+.012f)/7.0f,u1=(tile+.988f)/7.0f,v0=.012f,v1=.988f;
        const float shade[6]={.72f,.84f,1.0f,.38f,.62f,.78f};float light=shade[face];
        if(block==1&&face==2){
            bool underTree=false;
            for(int dz=-2;dz<=2&&!underTree;dz++)for(int dx=-2;dx<=2&&!underTree;dx++)
                for(int dy=2;dy<=5;dy++)if(blockAt(gx+dx,gy+dy,gz+dz)==5){underTree=true;break;}
            if(underTree)light*=.52f;
        }
        V3 color=block==10?V3{1,1,1}:(block==9?V3{.22f*light,.52f*light,.88f*light}:V3{light,light,light});
        addTri(v,a,b,c,color,u0,v0,u1,v0,u1,v1);addTri(v,a,c,d,color,u0,v0,u1,v1,u0,v1);
    }
    bool solid(int x,int y,int z){
        int cx=floorDiv(x,CS),cz=floorDiv(z,CS);auto it=chunks.find(chunkKey(cx,cz));
        if(it==chunks.end())return y<0;
        uint8_t b=it->second.get(modp(x,CS),y,modp(z,CS));return b!=0&&b!=9;
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
                if(!cave)c.set(x,y,z,y==h-1?1:(y>=h-4?2:3));
            }
            if(h<SEA_LEVEL)for(int y=h;y<SEA_LEVEL;y++)c.set(x,y,z,9);
        }
        auto put=[&](int x,int y,int z,uint8_t id){if(floorDiv(x,CS)==c.cx&&floorDiv(z,CS)==c.cz)c.set(modp(x,CS),y,modp(z,CS),id);};
        // Four deterministic silhouettes: oak, pine, broad crown and slim birch-like tree.
        for(int z=oz-5;z<oz+CS+5;z++)for(int x=ox-5;x<ox+CS+5;x++){
            if(modp(x+3,8)||modp(z+5,8))continue;
            uint32_t h=terrainHash(x,z);if(h%4==0)continue;
            int ground=groundHeight(x,z);if(ground<SEA_LEVEL||ground>12)continue;
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
        if(it==chunks.end()){Chunk c;c.cx=cx;c.cz=cz;generate(c);it=chunks.emplace(k,std::move(c)).first;}
        return it->second;
    }
    V3 blockColor(uint8_t b,int face){
        if(b==9)return{.12f,.42f,.82f};if(b==1){if(face==2)return{.22f,.68f,.18f};return{.42f,.29f,.16f};}
        if(b==2)return{.48f,.31f,.17f};if(b==3)return{.43f,.45f,.48f};
        if(b==4)return{.43f,.27f,.12f};if(b==5)return{.12f,.48f,.13f};if(b==6)return{.67f,.46f,.24f};if(b==10||b==11)return{1,1,1};return{.72f,.62f,.35f};
    }
    float breakDuration(uint8_t b)const{bool pickaxe=selected>=0&&selected<(int)inventory.size()&&inventory[selected].count>0&&inventory[selected].id==12;if(pickaxe&&b==3)return .24f;if(pickaxe&&b==2)return .42f;switch(b){case 1:return .65f;case 2:return .90f;case 3:return 2.10f;case 4:return 1.25f;case 5:return .30f;case 6:return 1.10f;case 10:return .25f;case 11:return .25f;default:return .85f;}}
