// Subsystem: Main-menu and settings-screen rendering.
// Function comments describe each method's responsibility and its place in the game systems.

    void drawSettings(){
        glDisable(GL_DEPTH_TEST);std::vector<float>v;
        addFace(v,{-.78f,-.72f,0},{.78f,-.72f,0},{.78f,.72f,0},{-.78f,.72f,0},{.025f,.035f,.05f});
        addFace(v,{-.74f,-.68f,0},{.74f,-.68f,0},{.74f,.68f,0},{-.74f,.68f,0},{.12f,.15f,.18f});
        addFace(v,{-.48f,.05f,0},{.48f,.05f,0},{.48f,.30f,0},{-.48f,.30f,0},smoothGraphics?V3{.12f,.48f,.25f}:V3{.32f,.16f,.12f});
        addFace(v,{-.48f,-.35f,0},{.48f,-.35f,0},{.48f,-.02f,0},{-.48f,-.02f,0},{.05f,.08f,.12f});
        addFace(v,{.62f,.55f,0},{.76f,.55f,0},{.76f,.70f,0},{.62f,.70f,0},{.42f,.12f,.12f});
        draw(v,GL_TRIANGLES,identity(),0,0,0,false);
        drawText("SETTINGS",-.22f,.52f,.026f,{.55f,.88f,1});
        drawText(smoothGraphics?"SMOOTH GRAPHICS ON":"SMOOTH GRAPHICS OFF",-.36f,.16f,.018f,{1,1,1});
        drawText("DRAW DISTANCE",-.25f,-.10f,.018f,{1,1,1});
        drawText("-",-.34f,-.27f,.026f,{1,1,1});drawText(std::to_string(drawDistance),-.02f,-.27f,.024f,{1,1,1});drawText("+",.25f,-.27f,.026f,{1,1,1});
        drawText("SMOOTHER TEXTURES  |  3-10 CHUNKS",-.43f,-.52f,.012f,{.8f,.85f,.9f});drawText("X",.665f,.60f,.025f,{1,.5f,.5f});
        glEnable(GL_DEPTH_TEST);
    }
    void drawMenu(){
        glDisable(GL_DEPTH_TEST);
        std::vector<float>v;
        addFace(v,{-.92f,-.96f,0},{.92f,-.96f,0},{.92f,.94f,0},{-.92f,.94f,0},{.025f,.040f,.055f});
        addFace(v,{-.88f,-.92f,0},{.88f,-.92f,0},{.88f,.90f,0},{-.88f,.90f,0},{.085f,.125f,.16f});
        draw(v,GL_TRIANGLES,identity(),0,0,0,false);
        drawText("DEATH CRAFT",-.30f,.82f,.030f,{.45f,.88f,1});drawText("SET",.59f,.76f,.014f,{1,1,1});
        std::vector<float>b;
        auto button=[&](float x,float y,float w,float h,V3 c){addFace(b,{x,y,0},{x+w,y,0},{x+w,y+h,0},{x,y+h,0},c);};
        // Name entry gets its own full-width row with generous spacing.
        button(-.72f,.50f,1.44f,.15f,{.035f,.065f,.085f});
        button(-.62f,.17f,.58f,.15f,creativeMode?V3{.16f,.52f,.25f}:V3{.08f,.10f,.12f});
        button(.04f,.17f,.58f,.15f,!creativeMode?V3{.16f,.52f,.25f}:V3{.08f,.10f,.12f});
        button(-.62f,-.18f,.58f,.15f,flatWorld?V3{.16f,.52f,.25f}:V3{.08f,.10f,.12f});
        button(.04f,-.18f,.58f,.15f,!flatWorld?V3{.16f,.52f,.25f}:V3{.08f,.10f,.12f});
        for(int i=0;i<3;i++)button(-.62f+i*.44f,-.54f,.38f,.15f,selectedSlot==i?V3{.55f,.37f,.12f}:V3{.08f,.10f,.12f});
        button(-.24f,-.82f,.48f,.17f,{.12f,.42f,.68f});button(.53f,.72f,.22f,.13f,{.18f,.30f,.38f});
        draw(b,GL_TRIANGLES,identity(),0,0,0,false);
        drawText("PLAYER NAME",-.68f,.72f,.012f,{.8f,.85f,.9f});
        drawText(playerName,-.62f,.55f,.014f,{1,1,1});
        drawText("MODE",-.08f,.39f,.012f,{.8f,.85f,.9f});
        drawText("CREATIVE",-.53f,.22f,.012f,{1,1,1});
        drawText("SURVIVAL",.13f,.22f,.012f,{1,1,1});
        drawText("WORLD TYPE",-.20f,.04f,.012f,{.8f,.85f,.9f});
        drawText("FLAT",-.43f,-.13f,.012f,{1,1,1});
        drawText("NORMAL",.10f,-.13f,.012f,{1,1,1});
        drawText("SAVE SLOT",-.18f,-.29f,.012f,{.8f,.85f,.9f});
        for(int i=0;i<3;i++)drawText("SLOT "+std::to_string(i+1),-.59f+i*.44f,-.49f,.010f,{1,1,1});
        drawText("START",-.15f,-.76f,.014f,{1,1,1});
        if(nameEditing){
            std::vector<float>kb;
            addFace(kb,{-.76f,-.72f,0},{.76f,-.72f,0},{.76f,.48f,0},{-.76f,.48f,0},{.025f,.04f,.06f});
            draw(kb,GL_TRIANGLES,identity(),0,0,0,false);
            drawText("TYPE YOUR NAME",-.28f,.40f,.014f,{.5f,.85f,1});
            const char* rows[]={"ABCDEFG","HIJKLMN","OPQRSTU","VWXYZ"};
            for(int r=0;r<4;r++)for(int c=0;rows[r][c];c++)drawText(std::string(1,rows[r][c]),-.61f+c*.19f,.19f-r*.20f,.020f,{1,1,1});
            drawText("DONE",-.10f,-.60f,.020f,{.5f,1,.6f});
            drawText("DEL",.38f,-.60f,.020f,{1,.55f,.45f});
        }
        glEnable(GL_DEPTH_TEST);
    }
