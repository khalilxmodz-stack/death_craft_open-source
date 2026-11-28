    bool addItem(uint8_t id,int n){
        if(!id||n<=0)return true;
        for(auto &i:inventory)if(i.id==id&&i.count>0&&i.count<64&&n>0){int add=std::min(64-i.count,n);i.count+=add;n-=add;}
        while(n>0){auto it=std::find_if(inventory.begin(),inventory.end(),[](const Item&i){return i.count<=0;});if(it==inventory.end()){status="Inventory full";statusFrames=90;return false;}int put=std::min(64,n);*it={id,put};n-=put;}
        return true;
    }
    uint8_t selectedBlock()const{return inventory[selected].count?inventory[selected].id:1;}
    void consumeSelected(){if(!creativeMode&&inventory[selected].count>0&&--inventory[selected].count==0)inventory[selected].id=0;}
    int itemCount(uint8_t id)const{int n=0;for(const auto&i:inventory)if(i.id==id)n+=i.count;return n;}
    bool consumeItem(uint8_t id,int amount){if(creativeMode)return true;if(itemCount(id)<amount)return false;for(auto&i:inventory)if(i.id==id&&amount>0){int n=std::min(amount,i.count);i.count-=n;amount-=n;if(!i.count)i.id=0;}return amount==0;}
    void craft(int r){uint8_t src=0,out=0;int need=0,give=0;if(r==0){src=4;need=1;out=6;give=4;}else if(r==1){src=6;need=2;out=7;give=4;}else if(r==2){src=6;need=4;out=8;give=1;}else if(r==3){if(itemCount(6)<3||itemCount(7)<2){status="Need 3 PLANKS + 2 STICKS";statusFrames=100;return;}consumeItem(6,3);consumeItem(7,2);addItem(15,1);status="WOODEN AXE CRAFTED";statusFrames=90;return;}else if(r==4){if(itemCount(6)<3||itemCount(7)<2){status="Need 3 PLANKS + 2 STICKS";statusFrames=100;return;}consumeItem(6,3);consumeItem(7,2);addItem(16,1);status="WOODEN PICKAXE CRAFTED";statusFrames=90;return;}else if(r==5){if(itemCount(17)<9){status="NEED 9 IRON INGOTS";statusFrames=100;return;}consumeItem(17,9);addItem(14,1);status="IRON BLOCK CRAFTED";statusFrames=90;return;}else if(r==6){if(itemCount(6)<3||itemCount(7)<2){status="BED NEEDS 3 PLANKS + 2 STICKS";statusFrames=100;return;}consumeItem(6,3);consumeItem(7,2);addItem(18,1);status="BED CRAFTED";statusFrames=90;return;}if(!consumeItem(src,need)){status=src==4?"Need wood":"Need planks";statusFrames=90;return;}addItem(out,give);status=out==8?"CRAFTING TABLE MADE":"CRAFTED";statusFrames=75;}
    int inventorySlotAt(float x,float y)const{for(int r=0;r<4;r++)for(int c=0;c<5;c++){float bx=-.58f+c*.235f,by=.30f-r*.245f;if(x>=bx&&x<=bx+.19f&&y>=by-.19f&&y<=by+.02f)return r*5+c;}return -1;}
