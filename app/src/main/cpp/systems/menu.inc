    void drawMenu(){
        glDisable(GL_DEPTH_TEST);std::vector<float>v;
        addFace(v,{-.78f,-.86f,0},{.78f,-.86f,0},{.78f,.86f,0},{-.78f,.86f,0},{.035f,.055f,.075f});
        addFace(v,{-.74f,-.82f,0},{.74f,-.82f,0},{.74f,.82f,0},{-.74f,.82f,0},{.10f,.15f,.19f});
        draw(v,GL_TRIANGLES,identity(),0,0,0,false);drawText("DEATH CRAFT",-.37f,.68f,.045f,{.45f,.88f,1});
        std::vector<float>b;auto button=[&](float x,float y,float w,float h,V3 c){addFace(b,{x,y,0},{x+w,y,0},{x+w,y+h,0},{x,y+h,0},c);};
        button(-.62f,.34f,.58f,.20f,creativeMode?V3{.16f,.52f,.25f}:V3{.08f,.10f,.12f});button(.04f,.34f,.58f,.20f,!creativeMode?V3{.16f,.52f,.25f}:V3{.08f,.10f,.12f});
        button(-.62f,.04f,.58f,.20f,flatWorld?V3{.16f,.52f,.25f}:V3{.08f,.10f,.12f});button(.04f,.04f,.58f,.20f,!flatWorld?V3{.16f,.52f,.25f}:V3{.08f,.10f,.12f});
        for(int i=0;i<3;i++)button(-.62f+i*.44f,-.36f,.38f,.18f,selectedSlot==i?V3{.55f,.37f,.12f}:V3{.08f,.10f,.12f});
        button(-.24f,-.70f,.48f,.22f,{.12f,.42f,.68f});
        button(-.45f,.47f,.90f,.11f,{.08f,.22f,.30f});draw(b,GL_TRIANGLES,identity(),0,0,0,false);
        drawText("NAME: "+playerName,-.40f,.51f,.017f,{1,1,1});
        drawText("CREATIVE",-.55f,.49f,.016f,{1,1,1});drawText("SURVIVAL",.10f,.49f,.016f,{1,1,1});
        drawText("FLAT",-.48f,.19f,.016f,{1,1,1});drawText("NORMAL",.10f,.19f,.016f,{1,1,1});
        for(int i=0;i<3;i++)drawText("SLOT "+std::to_string(i+1),-.59f+i*.44f,-.22f,.014f,{1,1,1});
        drawText("START",-.15f,-.55f,.016f,{1,1,1});drawText("MODE",-.08f,.60f,.016f,{.8f,.85f,.9f});
        if(nameEditing){std::vector<float> kb;addFace(kb,{-.72f,-.62f,0},{.72f,-.62f,0},{.72f,.40f,0},{-.72f,.40f,0},{.025f,.04f,.06f});draw(kb,GL_TRIANGLES,identity(),0,0,0,false);drawText("TYPE YOUR NAME",-.28f,.34f,.018f,{.5f,.85f,1});const char* rows[]={"ABCDEFG","HIJKLMN","OPQRSTU","VWXYZ"};for(int r=0;r<4;r++)for(int c=0;rows[r][c];c++)drawText(std::string(1,rows[r][c]),-.61f+c*.19f,.19f-r*.20f,.025f,{1,1,1});drawText("DONE",-.10f,-.53f,.025f,{.5f,1,.6f});drawText("DEL",.38f,-.53f,.025f,{1,.55f,.45f});}drawText("WORLD TYPE",-.20f,.27f,.014f,{.8f,.85f,.9f});drawText("SAVE SLOT",-.18f,-.12f,.014f,{.8f,.85f,.9f});glEnable(GL_DEPTH_TEST);
    }
