    void updateWorld(){
        int cx=floorDiv((int)std::floor(px),CS),cz=floorDiv((int)std::floor(pz),CS);
        for(int z=-drawDistance;z<=drawDistance;z++)for(int x=-drawDistance;x<=drawDistance;x++)ensureChunk(cx+x,cz+z);
        for(auto it=chunks.begin();it!=chunks.end();){
            if(std::abs(it->second.cx-cx)>drawDistance+1||std::abs(it->second.cz-cz)>drawDistance+1)it=chunks.erase(it);else ++it;
        }
        for(auto&entry:chunks)if(entry.second.dirty)rebuild(entry.second);
    }
    void updateSky(){
        float phase=dayClock/1200.0f;
        float daylight=std::clamp(std::sin(phase*2.0f*PI)*2.4f+.15f,0.0f,1.0f);
        V3 night{.0015f,.002f,.006f},day{.48f,.74f,.96f},dawn{.92f,.34f,.16f};
        skyTint={night.x+(day.x-night.x)*daylight,night.y+(day.y-night.y)*daylight,night.z+(day.z-night.z)*daylight};
        float sunHeight=std::sin(phase*2.0f*PI);
        float dusk=std::clamp(1.0f-std::abs(sunHeight)*3.2f,0.0f,1.0f)*(1.0f-daylight);
        skyTint={skyTint.x+(dawn.x-skyTint.x)*dusk*.42f,skyTint.y+(dawn.y-skyTint.y)*dusk*.42f,skyTint.z+(dawn.z-skyTint.z)*dusk*.42f};
    }
    void drawSkyDecor(const Mat4&vp){
        const float phase=dayClock/1200.0f,angle=phase*2.0f*PI;
        const float daylight=std::clamp(std::sin(angle)*2.4f+.15f,0.0f,1.0f);
        const float sunHeight=std::sin(angle);
        const float night=1.0f-daylight;
        const V3 camRight{std::cos(yaw),0,std::sin(yaw)};
        const V3 camUp{-std::sin(yaw)*std::sin(pitch),std::cos(pitch),std::cos(yaw)*std::sin(pitch)};
        const V3 origin{px,feet+1.62f,pz};
        std::vector<float>v;
        auto point=[&](float dx,float dy,float dz,float r,float u,float w){
            return V3{origin.x+dx*150.0f+camRight.x*u*r+camUp.x*w*r,
                      origin.y+dy*150.0f+camRight.y*u*r+camUp.y*w*r,
                      origin.z+dz*150.0f+camRight.z*u*r+camUp.z*w*r};
        };
        auto quad=[&](V3 center,float radius,V3 color){
            V3 right{camRight.x*radius,camRight.y*radius,camRight.z*radius};
            V3 up{camUp.x*radius,camUp.y*radius,camUp.z*radius};
            V3 a{center.x-right.x-up.x,center.y-right.y-up.y,center.z-right.z-up.z};
            V3 b{center.x+right.x-up.x,center.y+right.y-up.y,center.z+right.z-up.z};
            V3 c{center.x+right.x+up.x,center.y+right.y+up.y,center.z+right.z+up.z};
            V3 d{center.x-right.x+up.x,center.y-right.y+up.y,center.z-right.z+up.z};
            addFace(v,a,b,c,d,color);
        };
        auto disc=[&](V3 center,float radius,V3 color){
            for(int i=0;i<32;i++){
                float a=i*2*PI/32.0f,b=(i+1)*2*PI/32.0f;
                V3 p0{center.x,center.y,center.z};
                V3 p1{center.x+camRight.x*std::cos(a)*radius+camUp.x*std::sin(a)*radius,
                      center.y+camRight.y*std::cos(a)*radius+camUp.y*std::sin(a)*radius,
                      center.z+camRight.z*std::cos(a)*radius+camUp.z*std::sin(a)*radius};
                V3 p2{center.x+camRight.x*std::cos(b)*radius+camUp.x*std::sin(b)*radius,
                      center.y+camRight.y*std::cos(b)*radius+camUp.y*std::sin(b)*radius,
                      center.z+camRight.z*std::cos(b)*radius+camUp.z*std::sin(b)*radius};
                addTri(v,p0,p1,p2,color);
            }
        };
        if(night>.92f){
            for(int i=0;i<90;i++){
                float a=(float)((terrainHash(i*17+31,i*43+7)&0xffff)/65535.0f)*2.0f*PI;
                float y=.10f+(float)((terrainHash(i*29+3,i*11+91)&0xffff)/65535.0f)*.88f;
                float radial=std::sqrt(std::max(0.0f,1.0f-y*y));
                float dx=std::cos(a)*radial,dz=std::sin(a)*radial;
                V3 center=point(dx,y,dz,0,0,0);
                float size=(i%8==0?.42f:.22f);
                quad(center,size,{1.0f,.88f,.25f});
            }
        }
        if(sunHeight>-.03f){
            float dx=std::cos(angle),dy=std::max(.025f,sunHeight),dz=.30f;
            float len=std::sqrt(dx*dx+dy*dy+dz*dz);dx/=len;dy/=len;dz/=len;
            V3 center=point(dx,dy,dz,0,0,0);
            disc(center,3.2f,{1.0f,.83f,.42f});
        }
        if(sunHeight<-.03f){
            float dx=-std::cos(angle),dy=std::max(.025f,-sunHeight),dz=-.30f;
            float len=std::sqrt(dx*dx+dy*dy+dz*dz);dx/=len;dy/=len;dz/=len;
            V3 center=point(dx,dy,dz,0,0,0);
            disc(center,2.65f,{.88f,.91f,1.0f});
            V3 cut{center.x+camRight.x*1.05f+camUp.x*.35f,center.y+camRight.y*1.05f+camUp.y*.35f,center.z+camRight.z*1.05f+camUp.z*.35f};
            disc(cut,2.35f,skyTint);
        }
        glDisable(GL_DEPTH_TEST);draw(v,GL_TRIANGLES,vp,px,feet+1.62f,pz,false,false);glEnable(GL_DEPTH_TEST);
    }
    void spawnDroppedItem(uint8_t id,float x,float y,float z){
        if(!id)return;
        DroppedItem item{};item.id=id;item.x=x;item.y=y;item.z=z;item.vx=std::sin(yaw)*.9f;item.vy=1.6f;item.vz=-std::cos(yaw)*.9f;droppedItems.push_back(item);
        if(lanMode){LanPacket packet{};packet.type=8;packet.player=networkId;packet.block=id;packet.x=x;packet.y=y;packet.z=z;sendLan(packet);}
    }
    void spawnBlockEffects(int x,int y,int z,uint8_t id){
        DroppedItem item{};item.id=id;item.x=x+.5f;item.y=y+.48f;item.z=z+.5f;
        uint32_t h=terrainHash(x*7+id,y*13+z);
        item.vx=((int)(h&255)-127)/900.0f;item.vy=2.0f;item.vz=((int)((h>>8)&255)-127)/900.0f;
        droppedItems.push_back(item);
        for(int i=0;i<10;i++){
            uint32_t q=terrainHash(x+i*31+id,y*17+i*7+z);
            BlockParticle p{};p.id=id;p.x=x+.18f+(q&255)/320.0f;p.y=y+.18f+((q>>8)&255)/320.0f;p.z=z+.18f+((q>>16)&255)/320.0f;
            p.vx=((int)(q&1023)-511)/420.0f;p.vy=1.0f+((q>>10)&255)/180.0f;p.vz=((int)((q>>18)&1023)-511)/420.0f;p.life=.42f+(i%4)*.10f;
            blockParticles.push_back(p);
        }
    }
    void updateWorldEffects(float dt){
        for(auto it=droppedItems.begin();it!=droppedItems.end();){
            it->age+=dt;it->vy-=12.0f*dt;it->x+=it->vx*dt;it->y+=it->vy*dt;it->z+=it->vz*dt;
            it->vx*=.985f;it->vz*=.985f;
            int bx=(int)std::floor(it->x),bz=(int)std::floor(it->z);
            for(int by=(int)std::floor(it->y);by>=0;--by)if(solid(bx,by,bz)){float floorY=by+1.0f;if(it->y<floorY+.13f){it->y=floorY+.13f;it->vy=it->vy<-.8f?-.16f:0.0f;it->vx*=.72f;it->vz*=.72f;}break;}
            float dx=it->x-(px),dy=it->y-(feet+.75f),dz=it->z-pz;
            if(dx*dx+dy*dy+dz*dz<2.25f){if(addItem(it->id,1)){if(lanMode){LanPacket packet{};packet.type=9;packet.player=networkId;packet.block=it->id;packet.x=it->x;packet.y=it->y;packet.z=it->z;sendLan(packet);}it=droppedItems.erase(it);continue;}}
            if(it->age>120.0f){it=droppedItems.erase(it);continue;}
            ++it;
        }
        for(auto it=blockParticles.begin();it!=blockParticles.end();){
            it->life-=dt;it->vy-=8.0f*dt;it->x+=it->vx*dt;it->y+=it->vy*dt;it->z+=it->vz*dt;
            if(it->life<=0){it=blockParticles.erase(it);continue;}++it;
        }
    }
    void drawWorldEffects(const Mat4&vp){
        std::vector<float>v;
        auto cube=[&](float x,float y,float z,float s,V3 color){
            V3 a{x-s,y-s,z-s},b{x+s,y-s,z-s},c{x+s,y+s,z-s},d{x-s,y+s,z-s};
            V3 e{x-s,y-s,z+s},f{x+s,y-s,z+s},g{x+s,y+s,z+s},h{x-s,y+s,z+s};
            addFace(v,a,b,c,d,color);addFace(v,f,e,h,g,color);addFace(v,e,a,d,h,color);addFace(v,b,f,g,c,color);addFace(v,d,c,g,h,color);addFace(v,e,f,b,a,color);
        };
        for(const auto&i:droppedItems){float bob=std::sin(animationTime*3.0f+i.age*2.0f)*.045f;cube(i.x,i.y+bob,i.z,.13f,blockColor(i.id,2));}
        for(const auto&p:blockParticles){float s=.035f*std::clamp(p.life/.5f,.25f,1.0f);cube(p.x,p.y,p.z,s,blockColor(p.id,2));}
        draw(v,GL_TRIANGLES,vp,px,feet+1.62f,pz,true,false);
    }

    void updateTnt(float dt){
        for(auto it=primedTnts.begin();it!=primedTnts.end();){
            it->fuse-=dt;
            if(it->fuse>0.0f){++it;continue;}
            explosionX=it->x;explosionY=it->y;explosionZ=it->z;explosionEffectTimer=.62f;
            const int radius=4;
            for(int bz=(int)std::floor(it->z)-radius;bz<=(int)std::floor(it->z)+radius;bz++)
            for(int by=std::max(0,(int)std::floor(it->y)-radius);by<=std::min(HEIGHT-1,(int)std::floor(it->y)+radius);by++)
            for(int bx=(int)std::floor(it->x)-radius;bx<=(int)std::floor(it->x)+radius;bx++){
                float dx=bx+.5f-it->x,dy=by+.5f-it->y,dz=bz+.5f-it->z;
                if(dx*dx+dy*dy+dz*dz>12.25f)continue;
                uint8_t block=blockAt(bx,by,bz);if(!block)continue;
                Chunk&chunk=ensureChunk(floorDiv(bx,CS),floorDiv(bz,CS));
                chunk.set(modp(bx,CS),by,modp(bz,CS),0);edits[{bx,by,bz}]=0;
                if(lanMode==1){LanPacket packet{};packet.type=3;packet.player=networkId;packet.bx=bx;packet.by=by;packet.bz=bz;packet.block=0;sendLan(packet);}
            }
            float dx=px-it->x,dy=feet+.85f-it->y,dz=pz-it->z;
            float distance=std::sqrt(dx*dx+dy*dy+dz*dz);
            if(distance<4.5f&&!creativeMode){
                int damage=distance<2.0f?10:(distance<3.3f?6:3);
                int absorbed=std::min(armor,damage);armor-=absorbed;damage-=absorbed;
                health=std::max(0,health-damage);status="TNT EXPLOSION!";statusFrames=120;
                if(health<=0){playerDead=true;swallowTimer=.001f;deathMessage="KILLED BY TNT";}
            }else{status="TNT EXPLODED";statusFrames=100;}
            if(lanMode==1){LanPacket event{};event.type=5;event.player=networkId;event.x=explosionX;event.y=explosionY;event.z=explosionZ;sendLan(event);}
            for(auto&entry:chunks)entry.second.dirty=true;
            it=primedTnts.erase(it);
        }
    }
    void updateKhalil(float dt){
        khalilHitTimer=std::max(0.0f,khalilHitTimer-dt);
        if(explosionEffectTimer>0)explosionEffectTimer=std::max(0.0f,explosionEffectTimer-dt);
        if(lanMode==2)return;
        if(!khalilAlive){
            khalilRespawn+=dt;
            if(khalilRespawn>24.0f){
                float angle=(float)(terrainHash((int)px+(int)animationTime,(int)pz)&0xffff)/65535.0f*2.0f*PI;
                khalilX=px+std::cos(angle)*10.0f;khalilZ=pz+std::sin(angle)*10.0f;
                khalilFeet=(float)groundHeight((int)std::floor(khalilX),(int)std::floor(khalilZ));
                khalilAlive=true;khalilHealth=8;khalilHitTimer=0;khalilPrimed=false;khalilFuse=0;khalilRespawn=0;
            }
            return;
        }
        if(playerDead)return;
        float dx=px-khalilX,dz=pz-khalilZ,dist=std::sqrt(dx*dx+dz*dz);
        if(dist>36.0f){
            float angle=(float)(terrainHash((int)px+17,(int)pz-29)&0xffff)/65535.0f*2.0f*PI;
            khalilX=px-std::cos(angle)*10.0f;khalilZ=pz-std::sin(angle)*10.0f;
            khalilFeet=(float)groundHeight((int)std::floor(khalilX),(int)std::floor(khalilZ));
            dx=px-khalilX;dz=pz-khalilZ;dist=std::sqrt(dx*dx+dz*dz);
        }
        if(dist>2.15f){
            float step=std::min(3.15f*dt,dist-1.35f);
            float nx=khalilX+dx/dist*step,nz=khalilZ+dz/dist*step;
            int gh=groundHeight((int)std::floor(nx),(int)std::floor(nz));
            if(std::abs((float)gh-khalilFeet)<=1.1f){khalilX=nx;khalilZ=nz;khalilFeet=(float)gh;}
        }
        if(dist<2.65f){khalilPrimed=true;khalilFuse+=dt;}
        if(khalilPrimed&&khalilFuse>=1.0f){
            explosionX=khalilX;explosionY=khalilFeet+.75f;explosionZ=khalilZ;explosionEffectTimer=.62f;
            float pdx=px-explosionX,pdy=feet+0.85f-explosionY,pdz=pz-explosionZ;
            float blastDistance=std::sqrt(pdx*pdx+pdy*pdy+pdz*pdz);
            if(blastDistance<4.5f&&!creativeMode){
                int damage=blastDistance<2.0f?20:(blastDistance<3.3f?12:6);
                health=std::max(0,health-damage);status="KHALIL EXPLOSION!";statusFrames=120;
                if(health<=0){playerDead=true;swallowTimer=.001f;deathMessage="KHALIL EXPLODED";}
            }else{status="KHALIL EXPLODED";statusFrames=100;}
            if(lanMode==1){LanPacket event{};event.type=5;event.player=networkId;event.x=explosionX;event.y=explosionY;event.z=explosionZ;sendLan(event);}
            khalilAlive=false;khalilRespawn=0;khalilPrimed=false;khalilFuse=0;
        }
    }
    void drawKhalil(const Mat4&vp){
        if(khalilAlive){
            std::vector<float> sphere;
            const int latitudes=12,longitudes=20;
            auto point=[&](float lat,float lon){return V3{khalilX+.58f*std::cos(lat)*std::sin(lon),khalilFeet+.78f+.72f*std::sin(lat),khalilZ+.58f*std::cos(lat)*std::cos(lon)};};
            for(int iy=0;iy<latitudes;iy++){
                float a0=-PI*.5f+PI*iy/latitudes,a1=-PI*.5f+PI*(iy+1)/latitudes;
                for(int ix=0;ix<longitudes;ix++){
                    float b0=2*PI*ix/longitudes,b1=2*PI*(ix+1)/longitudes;
                    V3 p0=point(a0,b0),p1=point(a0,b1),p2=point(a1,b1),p3=point(a1,b0);
                    float u0=(float)ix/longitudes,u1=(float)(ix+1)/longitudes;
                    float v0=(float)iy/latitudes,v1=(float)(iy+1)/latitudes;
                    V3 tint=khalilHitTimer>0?V3{1.0f,.12f,.12f}:V3{1,1,1};
                    addTri(sphere,p0,p1,p2,tint,u0,v0,u1,v0,u1,v1);
                    addTri(sphere,p0,p2,p3,tint,u0,v0,u1,v1,u0,v1);
                }
            }
            float pulse=khalilPrimed?.72f+.28f*std::sin(animationTime*18.0f):1.0f;
            if(khalilPrimed)for(size_t i=0;i<sphere.size();i+=8){sphere[i+3]*=pulse;sphere[i+4]*=pulse;sphere[i+5]*=pulse;}
            draw(sphere,GL_TRIANGLES,vp,px,feet+1.62f,pz,true,true,khalilTexture);
            float labelY=khalilFeet+1.72f;
            float cx=vp.m[0]*khalilX+vp.m[4]*labelY+vp.m[8]*khalilZ+vp.m[12];
            float cy=vp.m[1]*khalilX+vp.m[5]*labelY+vp.m[9]*khalilZ+vp.m[13];
            float cw=vp.m[3]*khalilX+vp.m[7]*labelY+vp.m[11]*khalilZ+vp.m[15];
            if(cw>.05f){float sx=cx/cw,sy=cy/cw;drawText("KHALIL",sx-.05f,sy+.055f,.016f,{1,1,1});}
        }
        if(explosionEffectTimer>0){
            std::vector<float> blast;float radius=4.5f*(1.0f-explosionEffectTimer/.62f);
            V3 center{explosionX,explosionY,explosionZ};
            for(int iy=0;iy<10;iy++){float a0=-PI*.5f+PI*iy/10.0f,a1=-PI*.5f+PI*(iy+1)/10.0f;
                for(int ix=0;ix<16;ix++){float b0=2*PI*ix/16.0f,b1=2*PI*(ix+1)/16.0f;
                    auto pt=[&](float a,float b){return V3{center.x+radius*std::cos(a)*std::sin(b),center.y+radius*std::sin(a),center.z+radius*std::cos(a)*std::cos(b)};};
                    V3 p0=pt(a0,b0),p1=pt(a0,b1),p2=pt(a1,b1),p3=pt(a1,b0);
                    addTri(blast,p0,p1,p2,{1.0f,.32f,.04f});addTri(blast,p0,p2,p3,{1.0f,.32f,.04f});
                }
            }
            draw(blast,GL_TRIANGLES,vp,px,feet+1.62f,pz,true,false);
        }
    }
    void frame(float dt){
        fpsAccumulator+=dt;fpsFrames++;if(fpsAccumulator>=0.5f){fpsValue=fpsFrames/fpsAccumulator;fpsFrames=0;fpsAccumulator=0;}
        if(!running||!eglMakeCurrent(display,surface,surface,context))return;dt=std::min(dt,.04f);swordCooldown=std::max(0.0f,swordCooldown-dt);
        if(!inWorld){glViewport(0,0,width,height);glClearColor(.10f,.18f,.25f,1);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);if(settingsOpen)drawSettings();else drawMenu();eglSwapBuffers(display,surface);return;}
        if(playerDead){swallowTimer+=dt;if(swallowTimer>2.8f){playerDead=false;swallowTimer=0;health=10;armor=10;px=0;pz=0;feet=(float)groundHeight(0,0);vy=0;grounded=true;grafikX=6;grafikZ=-5;grafikFeet=(float)groundHeight(6,-5);grafikAlive=true;grafikHealth=6;grafikHitTimer=0;khalilHealth=8;khalilAlive=true;khalilHitTimer=0;deathMessage="GRAFIK ATE YOU";} }
        dayClock=std::fmod(dayClock+dt,1200.0f);animationTime+=dt;updateSky();
        if(!playerDead){
        float forward=joyY,side=joyX,mag=std::sqrt(forward*forward+side*side);if(mag>1){forward/=mag;side/=mag;}
        bool swimming=inWater();float speed=swimming?2.8f:5.2f;movePlayer((std::sin(yaw)*forward+std::cos(yaw)*side)*speed*dt,(-std::cos(yaw)*forward+std::sin(yaw)*side)*speed*dt);
        if(swimming){grounded=false;fallTracking=false;vy-=2.0f*dt;if(feet+1.2f<SEA_LEVEL)vy+=5.0f*dt;vy*=.94f;vy=std::clamp(vy,-2.0f,3.0f);feet+=vy*dt;}
        else {if(grounded&&!supported()){grounded=false;fallStartFeet=feet;fallPeakFeet=feet;fallTracking=true;}}
        if(!grounded&&!swimming){fallPeakFeet=std::max(fallPeakFeet,feet);vy-=22*dt;float next=feet+vy*dt;if(vy<=0){int bx=(int)std::floor(px),bz=(int)std::floor(pz),by=(int)std::floor(next-.01f);
            if(solid(bx,by,bz)||solid((int)std::floor(px-.25f),by,(int)std::floor(pz-.25f))||
               solid((int)std::floor(px+.25f),by,(int)std::floor(pz+.25f))||
               solid((int)std::floor(px-.25f),by,(int)std::floor(pz+.25f))||
               solid((int)std::floor(px+.25f),by,(int)std::floor(pz-.25f))){
                next=by+1;vy=0;grounded=true;
                if(fallTracking){
                    float fallDistance=std::max(0.0f,fallPeakFeet-next);
                    if(!creativeMode&&fallDistance>6.0f){
                        int damage=std::max(1,(int)std::floor(fallDistance-3.0f));
                        int absorbed=std::min(armor,damage);armor-=absorbed;damage-=absorbed;
                        health=std::max(0,health-damage);
                        if(health<=0){playerDead=true;swallowTimer=.001f;}
                        else {status="FALL DAMAGE";statusFrames=90;}
                    }
                    fallTracking=false;fallPeakFeet=next;
                }
            }}
            else if(vy>0){int by=(int)std::floor(next+1.8f);if(solid((int)std::floor(px),by,(int)std::floor(pz))){next=by-1.8f;vy=0;}}
            feet=next;if(feet< -20){feet=5;vy=0;fallTracking=false;}}
        // Keep the feet on the surface of the generated grass layer.
        if(grounded&&feet<1.001f)feet=1.0f;
        if(breaking){int tx,ty,tz,ax,ay,az;if(raycast(tx,ty,tz,ax,ay,az)){if(tx!=breakX||ty!=breakY||tz!=breakZ){breakX=tx;breakY=ty;breakZ=tz;breakTimer=0;}auto it=chunks.find(chunkKey(floorDiv(tx,CS),floorDiv(tz,CS)));uint8_t q=it==chunks.end()?0:it->second.get(modp(tx,CS),ty,modp(tz,CS));breakTimer+=dt;if(q&&breakTimer>=breakDuration(q)){editBlock(false);breaking=false;breakTimer=0;}}else breakTimer=0;}
        }
        updateGrafik(dt);
        updateKhalil(dt);
        updateTnt(dt);
        updateWorldEffects(dt);
        updateLan(dt);
        if(inWorld){saveTimer+=dt;if(saveTimer>=12.0f){saveWorld();saveTimer=0;}}
        updateWorld();
        glViewport(0,0,width,height);glClearColor(skyTint.x,skyTint.y,skyTint.z,1);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
        Mat4 vp=mul(perspective(1.02f,(float)width/height,.1f,std::max(120.0f,(float)drawDistance*CS*2.5f)),camera(px,feet+1.62f,pz,yaw,pitch));drawSkyDecor(vp);glClear(GL_DEPTH_BUFFER_BIT);
        for(auto&entry:chunks)draw(entry.second.mesh,GL_TRIANGLES,vp,px,feet+1.62f,pz,true,true);
        std::vector<float> clouds;
        auto cloudCube=[&](float x,float y,float z,float w,float h,float d){V3 a{x,y,z},b{x+w,y,z},c{x+w,y+h,z},dd{x,y+h,z},e{x,y,z+d},ff{x+w,y,z+d},g{x+w,y+h,z+d},hh{x,y+h,z+d};V3 col{.94f,.97f,1.0f};addFace(clouds,a,b,c,dd,col);addFace(clouds,ff,e,hh,g,col);addFace(clouds,e,a,dd,hh,col);addFace(clouds,b,ff,g,c,col);addFace(clouds,dd,c,g,hh,col);addFace(clouds,e,ff,b,a,col);};
        for(int iz=-2;iz<=2;iz++)for(int ix=-3;ix<=3;ix++){float drift=std::fmod(animationTime*.18f,24.0f);float x=std::floor(px/24.0f)*24.0f+ix*24.0f+((ix+iz)&1?5.0f:0.0f)+drift,z=std::floor(pz/24.0f)*24.0f+iz*24.0f;float y=19.0f+((ix*3+iz*5)&1)*2.0f;cloudCube(x,y,z,7.0f,1.4f,4.0f);cloudCube(x+3.0f,y+.5f,z-1.0f,5.0f,1.5f,5.0f);cloudCube(x-2.0f,y+.3f,z+1.0f,4.0f,1.2f,3.0f);}
        draw(clouds,GL_TRIANGLES,vp,px,feet+1.62f,pz,true,false);
        drawWorldEffects(vp);
        drawKhalil(vp);
        if(remoteValid){
            std::vector<float> avatar;
            float rx=remoteX,rz=remoteZ,ry=remoteY;
            float cs=std::cos(remoteYaw),sn=std::sin(remoteYaw);
            auto rotatePoint=[&](V3 p){float lx=p.x-rx,lz=p.z-rz;return V3{rx+lx*cs-lz*sn,p.y,rz+lx*sn+lz*cs};};
            auto cube=[&](float x,float y,float z,float w,float h,float d,V3 c){
                V3 a=rotatePoint({x,y,z}),b=rotatePoint({x+w,y,z}),cc=rotatePoint({x+w,y+h,z}),dd=rotatePoint({x,y+h,z});
                V3 e=rotatePoint({x,y,z+d}),ff=rotatePoint({x+w,y,z+d}),g=rotatePoint({x+w,y+h,z+d}),hh=rotatePoint({x,y+h,z+d});
                addFace(avatar,a,b,cc,dd,c);addFace(avatar,ff,e,hh,g,c);
                addFace(avatar,e,a,dd,hh,c);addFace(avatar,b,ff,g,cc,c);
                addFace(avatar,dd,cc,g,hh,c);addFace(avatar,e,ff,b,a,c);
            };
            // Classic blocky player silhouette: blue shirt, indigo trousers, skin and hair.
            cube(rx-.30f,ry+.78f,rz-.15f,.60f,.78f,.30f,{.08f,.28f,.82f});
            cube(rx-.30f,ry+.25f,rz-.15f,.24f,.55f,.30f,{.10f,.18f,.62f});
            cube(rx+.06f,ry+.25f,rz-.15f,.24f,.55f,.30f,{.10f,.18f,.62f});
            cube(rx-.43f,ry+.78f,rz-.13f,.13f,.70f,.26f,{.72f,.48f,.32f});
            cube(rx+.30f,ry+.78f,rz-.13f,.13f,.70f,.26f,{.72f,.48f,.32f});
            cube(rx-.28f,ry+1.58f,rz-.25f,.56f,.55f,.50f,{.72f,.48f,.32f});
            cube(rx-.285f,ry+1.78f,rz-.258f,.57f,.12f,.515f,{.34f,.20f,.10f});
            cube(rx-.16f,ry+1.79f,rz-.275f,.08f,.07f,.025f,{.04f,.04f,.04f});
            cube(rx+.08f,ry+1.79f,rz-.275f,.08f,.07f,.025f,{.04f,.04f,.04f});
            cube(rx-.045f,ry+1.66f,rz-.278f,.09f,.08f,.025f,{.52f,.31f,.22f});
            cube(rx-.10f,ry+1.61f,rz-.278f,.20f,.035f,.025f,{.30f,.12f,.10f});
            draw(avatar,GL_TRIANGLES,vp,px,feet+1.62f,pz,true,false);
            float labelY=ry+2.28f;
            float cx=vp.m[0]*rx+vp.m[4]*labelY+vp.m[8]*rz+vp.m[12];
            float cy=vp.m[1]*rx+vp.m[5]*labelY+vp.m[9]*rz+vp.m[13];
            float cw=vp.m[3]*rx+vp.m[7]*labelY+vp.m[11]*rz+vp.m[15];
            if(cw>.05f){float sx=cx/cw,sy=cy/cw;drawText(remoteName,sx-.042f*remoteName.size(),sy+.045f,.014f,{1,1,1});}
        }
        if(grafikAlive){
            std::vector<float> sphere;auto point=[&](float lat,float lon){return V3{grafikX+.58f*std::cos(lat)*std::sin(lon),grafikFeet+.62f+.58f*std::sin(lat),grafikZ+.58f*std::cos(lat)*std::cos(lon)};};
            for(int iy=0;iy<10;iy++){float a0=-PI*.5f+PI*iy/10.0f,a1=-PI*.5f+PI*(iy+1)/10.0f;for(int ix=0;ix<16;ix++){float b0=2*PI*ix/16.0f,b1=2*PI*(ix+1)/16.0f;V3 p0=point(a0,b0),p1=point(a0,b1),p2=point(a1,b1),p3=point(a1,b0);float shade=.72f+.28f*std::max(0.0f,std::sin((a0+a1)*.5f));V3 c=grafikHitTimer>0?V3{1.0f,.06f,.06f}:V3{.48f*shade,.12f*shade,.68f*shade};addTri(sphere,p0,p1,p2,c);addTri(sphere,p0,p2,p3,c);}}
            draw(sphere,GL_TRIANGLES,vp,px,feet+1.62f,pz,true,false);
        }
        // Target outline around the block under the crosshair.
        int x,y,z,ax,ay,az;if(raycast(x,y,z,ax,ay,az)){
            float X=x,Y=y,Z=z;std::vector<float> edge;
            V3 white{1,.95f,.45f};
            V3 p[8]={{X,Y,Z},{X+1,Y,Z},{X+1,Y+1,Z},{X,Y+1,Z},{X,Y,Z+1},{X+1,Y,Z+1},{X+1,Y+1,Z+1},{X,Y+1,Z+1}};
            int e[24]={0,1,1,2,2,3,3,0,4,5,5,6,6,7,7,4,0,4,1,5,2,6,3,7};
            for(int i=0;i<24;i+=2){for(int j:{e[i],e[i+1]})edge.insert(edge.end(),{p[j].x,p[j].y,p[j].z,white.x,white.y,white.z,0,0});}
            glDisable(GL_CULL_FACE);draw(edge,GL_LINES,vp,px,feet+1.62f,pz,true);
        }
        if(settingsOpen)drawSettings();else if(inventoryOpen)drawInventory();else drawHud();
        int gameMinutes=(int)(dayClock/1200.0f*1440.0f)%1440;
        std::string clockText=(gameMinutes/60<10?"0":"")+std::to_string(gameMinutes/60)+":"+(gameMinutes%60<10?"0":"")+std::to_string(gameMinutes%60);
        drawText(clockText,-.045f,.91f,.010f,{1,1,1});
        eglSwapBuffers(display,surface);
    }
