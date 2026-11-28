    void updateWorld(){
        int cx=floorDiv((int)std::floor(px),CS),cz=floorDiv((int)std::floor(pz),CS);
        for(int z=-LOAD_RADIUS;z<=LOAD_RADIUS;z++)for(int x=-LOAD_RADIUS;x<=LOAD_RADIUS;x++)ensureChunk(cx+x,cz+z);
        for(auto it=chunks.begin();it!=chunks.end();){
            if(std::abs(it->second.cx-cx)>LOAD_RADIUS+1||std::abs(it->second.cz-cz)>LOAD_RADIUS+1)it=chunks.erase(it);else ++it;
        }
        for(auto&entry:chunks)if(entry.second.dirty)rebuild(entry.second);
    }
    void frame(float dt){
        fpsAccumulator+=dt;fpsFrames++;if(fpsAccumulator>=0.5f){fpsValue=fpsFrames/fpsAccumulator;fpsFrames=0;fpsAccumulator=0;}
        if(!running||!eglMakeCurrent(display,surface,surface,context))return;dt=std::min(dt,.04f);
        if(!inWorld){glViewport(0,0,width,height);glClearColor(.10f,.18f,.25f,1);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);drawMenu();eglSwapBuffers(display,surface);return;}
        if(playerDead){swallowTimer+=dt;if(swallowTimer>2.8f){playerDead=false;swallowTimer=0;health=10;armor=10;px=0;pz=0;feet=(float)groundHeight(0,0);vy=0;grounded=true;grafikX=6;grafikZ=-5;grafikFeet=(float)groundHeight(6,-5);grafikAlive=true;grafikHealth=2;} }
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
        updateLan(dt);
        if(inWorld){saveTimer+=dt;if(saveTimer>=12.0f){saveWorld();saveTimer=0;}}
        updateWorld();
        glViewport(0,0,width,height);glClearColor(.48f,.74f,.96f,1);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
        Mat4 vp=mul(perspective(1.02f,(float)width/height,.1f,240),camera(px,feet+1.62f,pz,yaw,pitch));
        for(auto&entry:chunks)draw(entry.second.mesh,GL_TRIANGLES,vp,px,feet+1.62f,pz,true,true);
        std::vector<float> clouds;
        auto cloudCube=[&](float x,float y,float z,float w,float h,float d){V3 a{x,y,z},b{x+w,y,z},c{x+w,y+h,z},dd{x,y+h,z},e{x,y,z+d},ff{x+w,y,z+d},g{x+w,y+h,z+d},hh{x,y+h,z+d};V3 col{.94f,.97f,1.0f};addFace(clouds,a,b,c,dd,col);addFace(clouds,ff,e,hh,g,col);addFace(clouds,e,a,dd,hh,col);addFace(clouds,b,ff,g,c,col);addFace(clouds,dd,c,g,hh,col);addFace(clouds,e,ff,b,a,col);};
        for(int iz=-2;iz<=2;iz++)for(int ix=-2;ix<=2;ix++){float x=std::floor(px/24.0f)*24.0f+ix*24.0f+((ix+iz)&1?5.0f:0.0f),z=std::floor(pz/24.0f)*24.0f+iz*24.0f;float y=19.0f+((ix*3+iz*5)&1)*2.0f;cloudCube(x,y,z,7.0f,1.4f,4.0f);cloudCube(x+3.0f,y+.5f,z-1.0f,5.0f,1.5f,5.0f);cloudCube(x-2.0f,y+.3f,z+1.0f,4.0f,1.2f,3.0f);}
        draw(clouds,GL_TRIANGLES,vp,px,feet+1.62f,pz,true,false);
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
            for(int iy=0;iy<10;iy++){float a0=-PI*.5f+PI*iy/10.0f,a1=-PI*.5f+PI*(iy+1)/10.0f;for(int ix=0;ix<16;ix++){float b0=2*PI*ix/16.0f,b1=2*PI*(ix+1)/16.0f;V3 p0=point(a0,b0),p1=point(a0,b1),p2=point(a1,b1),p3=point(a1,b0);float shade=.72f+.28f*std::max(0.0f,std::sin((a0+a1)*.5f));V3 c{.48f*shade,.12f*shade,.68f*shade};addTri(sphere,p0,p1,p2,c);addTri(sphere,p0,p2,p3,c);}}
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
        if(inventoryOpen)drawInventory();else drawHud();eglSwapBuffers(display,surface);
    }
