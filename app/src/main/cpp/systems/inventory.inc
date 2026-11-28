    bool addItem(uint8_t id,int n){
        if(!id||n<=0)return true;
        for(auto &i:inventory)if(i.id==id&&i.count>0&&i.count<64&&n>0){int add=std::min(64-i.count,n);i.count+=add;n-=add;}
        while(n>0){auto it=std::find_if(inventory.begin(),inventory.end(),[](const Item&i){return i.count<=0;});if(it==inventory.end()){status="Inventory full";statusFrames=90;return false;}int put=std::min(64,n);*it={id,put};n-=put;}
        return true;
    }
    uint8_t selectedBlock()const{return inventory[selected].count?inventory[selected].id:1;}
    void consumeSelected(){if(!creativeMode&&inventory[selected].count>0&&--inventory[selected].count==0)inventory[selected].id=0;}
    void craft(int r){uint8_t src=r==0?4:6;int n=r==0?1:(r==1?2:4);for(auto&i:inventory)if(i.id==src&&n){int k=std::min(n,i.count);i.count-=k;n-=k;if(!i.count)i.id=0;}if(n){status=r==0?"Need wood":"Need planks";statusFrames=90;return;}addItem(r==0?6:(r==1?7:8),r==0?4:(r==1?4:1));}
    int inventorySlotAt(float x,float y)const{for(int r=0;r<4;r++)for(int c=0;c<5;c++){float bx=-.58f+c*.235f,by=.30f-r*.245f;if(x>=bx&&x<=bx+.19f&&y>=by-.19f&&y<=by+.02f)return r*5+c;}return -1;}
