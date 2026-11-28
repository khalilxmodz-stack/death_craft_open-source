    // Subsystem: Player collision, movement, targeting, block interaction, and combat.
    // Function comments describe each method's responsibility and its place in the game systems.

    // Tests whether the player's body intersects any solid block.
    float x, float y, float z) {
        const float radius = .30f;
        for (int bx = (int)std::floor(x - radius); bx <= (int)std::floor(x + radius); bx++)
            for (int by = (int)std::floor(y); by <= (int)std::floor(y + 1.8f); by++)
                for (int bz = (int)std::floor(z - radius); bz <= (int)std::floor(z + radius); bz++)
                    if (solid(bx, by, bz))
                        return true;
        return false;
    }
    // Finds the ground height beneath a candidate horizontal position.
    float x, float z, float reference) {
        int bx = (int)std::floor(x), bz = (int)std::floor(z);
        for (int y = (int)std::floor(reference + 1.05f); y >= (int)std::floor(reference - .20f);
             y--)
            if (solid(bx, y, bz))
                return (float)y + 1.0f;
        return reference;
    }
    // Checks whether the player's feet are supported by nearby blocks.
    ) {
        const float r = .23f;
        float samples[5][2] = {
            {px, pz}, {px - r, pz - r}, {px + r, pz - r}, {px - r, pz + r}, {px + r, pz + r}};
        for (auto& q : samples)
            if (solid((int)std::floor(q[0]), (int)std::floor(feet - .06f), (int)std::floor(q[1])))
                return true;
        return false;
    }
    // Attempts to step the player onto a slightly higher block.
    float nx, float nz) {
        const float r = .23f;
        float samples[5][2] = {
            {nx, nz}, {nx - r, nz - r}, {nx + r, nz - r}, {nx - r, nz + r}, {nx + r, nz + r}};
        float target = feet;
        for (auto& q : samples)
            target = std::max(target, groundAt(q[0], q[1], feet));
        if (target > feet + .02f && target <= feet + 1.01f && !overlaps(nx, target, nz)) {
            px = nx;
            pz = nz;
            feet = target;
            vy = 0;
            grounded = true;
            return true;
        }
        return false;
    }
    // Moves the player in small collision-tested horizontal steps.
    float dx, float dz) {
        float distance = std::max(std::abs(dx), std::abs(dz));
        int steps = std::max(1, (int)std::ceil(distance / .14f));
        float sx = dx / steps, sz = dz / steps;
        for (int i = 0; i < steps; i++) {
            float nx = px + sx;
            if (!overlaps(nx, feet, pz))
                px = nx;
            else
                tryStep(nx, pz);
            float nz = pz + sz;
            if (!overlaps(px, feet, nz))
                pz = nz;
            else
                tryStep(px, nz);
        }
    }
    // Traces the view direction to find a target block and adjacent cell.
    int& hx, int& hy, int& hz, int& px0, int& py0, int& pz0) {
        V3 d{std::sin(yaw) * std::cos(pitch), std::sin(pitch), -std::cos(yaw) * std::cos(pitch)};
        int lastX = 0, lastY = 0, lastZ = 0;
        for (float t = .15f; t < 6.0f; t += .06f) {
            int x = (int)std::floor(px + 0.0f + d.x * t),
                y = (int)std::floor(feet + 1.62f + d.y * t), z = (int)std::floor(pz + d.z * t);
            if (solid(x, y, z)) {
                hx = x;
                hy = y;
                hz = z;
                px0 = lastX;
                py0 = lastY;
                pz0 = lastZ;
                return true;
            }
            lastX = x;
            lastY = y;
            lastZ = z;
        }
        return false;
    }
    // Places a selected block or breaks the targeted block.
    bool place) {
        int x, y, z, ax, ay, az;
        if (!raycast(x, y, z, ax, ay, az)) {
            status = "No block in reach";
            statusFrames = 90;
            return;
        }
        if (place) {
            if (selectedBlock() == 11 || selectedBlock() == 12 || selectedBlock() == 15 ||
                selectedBlock() == 16 || selectedBlock() == 17) {
                status = "TOOLS AND ITEMS CANNOT BE PLACED";
                statusFrames = 70;
                return;
            }
            if (!inventory[selected].count) {
                status = "Select a block";
                statusFrames = 90;
                return;
            }
            uint8_t item = selectedBlock();
            if (ay < 0 || ay >= HEIGHT)
                return;
            if (overlaps(ax + .5f, ay, az + .5f)) {
                status = "Cannot place inside player";
                statusFrames = 90;
                return;
            }
            if (item == 20 && (ay + 1 >= HEIGHT || blockAt(ax, ay + 1, az) != 0 ||
                               overlaps(ax + .5f, ay + 1, az + .5f))) {
                status = "NEED TWO CLEAR BLOCKS";
                statusFrames = 75;
                return;
            }
            int cx = floorDiv(ax, CS), cz = floorDiv(az, CS);
            Chunk& c = ensureChunk(cx, cz);
            c.set(modp(ax, CS), ay, modp(az, CS), item);
            edits[{ax, ay, az}] = item;
            if (item == 20) {
                c.set(modp(ax, CS), ay + 1, modp(az, CS), 22);
                edits[{ax, ay + 1, az}] = 22;
            }
            markBlockDirty(ax, ay, az);
            consumeSelected();
            if (lanMode) {
                LanPacket packet{};
                packet.type = 3;
                packet.player = networkId;
                packet.bx = ax;
                packet.by = ay;
                packet.bz = az;
                packet.block = item;
                sendLan(packet);
            }
        } else {
            uint8_t broken = 0;
            auto it = chunks.find(chunkKey(floorDiv(x, CS), floorDiv(z, CS)));
            if (it != chunks.end())
                broken = it->second.get(modp(x, CS), y, modp(z, CS));
            if (broken == 19) {
                breaking = false;
                breakTimer = 0;
                return;
            }
            if (broken == 20 || broken == 21 || broken == 22 || broken == 23) {
                int baseY = (broken == 22 || broken == 23) ? y - 1 : y;
                Chunk& dc = ensureChunk(floorDiv(x, CS), floorDiv(z, CS));
                for (int dy = 0; dy < 2; dy++) {
                    dc.set(modp(x, CS), baseY + dy, modp(z, CS), 0);
                    edits[{x, baseY + dy, z}] = 0;
                }
                markBlockDirty(x, baseY, z);
            }
            int cx = floorDiv(x, CS), cz = floorDiv(z, CS);
            Chunk& c = ensureChunk(cx, cz);
            c.set(modp(x, CS), y, modp(z, CS), 0);
            edits[{x, y, z}] = 0;
            waterFlowDepth.erase({x, y, z});
            markBlockDirty(x, y, z);
            uint8_t dropId =
                broken == 4
                    ? 4
                    : (broken == 5
                           ? 5
                           : (broken == 13 ? 17
                                           : (broken == 3 ? 3
                                                          : ((broken == 1 || broken == 2)
                                                                 ? 2
                                                                 : (broken == 6 ? 6 : broken)))));
            if (dropId)
                spawnBlockEffects(x, y, z, dropId);
            if (lanMode) {
                LanPacket packet{};
                packet.type = 3;
                packet.player = networkId;
                packet.bx = x;
                packet.by = y;
                packet.bz = z;
                packet.block = 0;
                sendLan(packet);
            }
        }
    }
    // Uses the targeted object, such as a door, bed, table, or TNT.
    ) {
        int x, y, z, ax, ay, az;
        if (raycast(x, y, z, ax, ay, az) && (blockAt(x, y, z) == 20 || blockAt(x, y, z) == 21 ||
                                             blockAt(x, y, z) == 22 || blockAt(x, y, z) == 23)) {
            int baseY = (blockAt(x, y, z) == 22 || blockAt(x, y, z) == 23) ? y - 1 : y;
            uint8_t lower = blockAt(x, baseY, z), upper = blockAt(x, baseY + 1, z);
            uint8_t nextLower = lower == 20 ? 21 : 20, nextUpper = upper == 22 ? 23 : 22;
            Chunk& doorChunk = ensureChunk(floorDiv(x, CS), floorDiv(z, CS));
            doorChunk.set(modp(x, CS), baseY, modp(z, CS), nextLower);
            doorChunk.set(modp(x, CS), baseY + 1, modp(z, CS), nextUpper);
            edits[{x, baseY, z}] = nextLower;
            edits[{x, baseY + 1, z}] = nextUpper;
            markBlockDirty(x, baseY, z);
            if (lanMode) {
                for (int dy = 0; dy < 2; dy++) {
                    LanPacket packet{};
                    packet.type = 3;
                    packet.player = networkId;
                    packet.bx = x;
                    packet.by = baseY + dy;
                    packet.bz = z;
                    packet.block = dy ? nextUpper : nextLower;
                    sendLan(packet);
                }
            }
            return;
        }
        if (raycast(x, y, z, ax, ay, az) && blockAt(x, y, z) == 18) {
            if (!isNight()) {
                status = "YOU CAN ONLY SLEEP AT NIGHT";
                statusFrames = 100;
                return;
            }
            if (lanMode == 2) {
                status = "ONLY HOST CAN SLEEP";
                statusFrames = 100;
                return;
            }
            dayClock = 300.0f;
            nightActive = false;
            grafikAlive = false;
            khalilAlive = false;
            skeletonAlive = false;
            skeletonArrows.clear();
            khalilPrimed = false;
            khalilFuse = 0;
            updateSky();
            saveWorld();
            status = "GOOD MORNING";
            statusFrames = 120;
            return;
        }
        // Applies the player's melee attack to a monster in reach.
        if (inventory[selected].count > 0 && inventory[selected].id == 11) {
            attackMonster();
            return;
        }
        if (inventory[selected].count > 0 &&
            (inventory[selected].id == 12 || inventory[selected].id == 15 ||
             inventory[selected].id == 16)) {
            status = "TOOL EQUIPPED";
            statusFrames = 45;
            return;
        }
        if (raycast(x, y, z, ax, ay, az) && blockAt(x, y, z) == 8) {
            inventoryOpen = true;
            craftingTableOpen = true;
            dragIndex = -1;
            status = "CRAFTING TABLE OPEN";
            statusFrames = 70;
            return;
        }
        if (raycast(x, y, z, ax, ay, az) && blockAt(x, y, z) == 10) {
            auto it = chunks.find(chunkKey(floorDiv(x, CS), floorDiv(z, CS)));
            if (it != chunks.end())
                it->second.set(modp(x, CS), y, modp(z, CS), 0);
            edits[{x, y, z}] = 0;
            markBlockDirty(x, y, z);
            primedTnts.push_back({x + .5f, y + .5f, z + .5f, 2.0f});
            status = "TNT PRIMED - 2 SECONDS";
            statusFrames = 100;
            if (lanMode) {
                LanPacket packet{};
                packet.type = 3;
                packet.player = networkId;
                packet.bx = x;
                packet.by = y;
                packet.bz = z;
                packet.block = 0;
                sendLan(packet);
            }
            return;
        }
        attackMonster();
    }
    // Applies the player's melee attack to a monster in reach.
    ) {
        if (playerDead || swordCooldown > 0)
            return;
        playSound(0);
        bool swordEquipped = inventory[selected].count > 0 && inventory[selected].id == 11;
        float skx = skeletonX - px, skz = skeletonZ - pz, skDist = std::sqrt(skx * skx + skz * skz);
        float fx0 = std::sin(yaw), fz0 = -std::cos(yaw);
        float skFacing = skDist > .001f ? (skx * fx0 + skz * fz0) / skDist : 1.0f;
        if (skeletonAlive && skDist <= 3.6f && skFacing > .30f) {
            skeletonHealth = std::max(0, skeletonHealth - (swordEquipped ? 3 : 1));
            skeletonHitTimer = .28f;
            if (skeletonHealth <= 0) {
                skeletonAlive = false;
                skeletonRespawn = 0;
            }
            swordCooldown = swordEquipped ? .34f : .55f;
            status = skeletonHealth <= 0 ? "SKELETON DEFEATED" : "SKELETON HIT";
            statusFrames = 80;
            return;
        }
        int damage = swordEquipped ? 3 : 1;
        float fx = std::sin(yaw), fz = -std::cos(yaw);
        float gdX = grafikX - px, gdZ = grafikZ - pz, gDist = std::sqrt(gdX * gdX + gdZ * gdZ);
        float kdX = khalilX - px, kdZ = khalilZ - pz, kDist = std::sqrt(kdX * kdX + kdZ * kdZ);
        bool hitGrafik = grafikAlive && gDist <= 3.6f && gDist <= kDist;
        bool hitKhalil = khalilAlive && kDist <= 3.6f && !hitGrafik;
        float dx = hitGrafik ? gdX : kdX, dz = hitGrafik ? gdZ : kdZ,
              dist = hitGrafik ? gDist : kDist;
        float facing = dist > .001f ? (dx * fx + dz * fz) / dist : 1.0f;
        if ((!hitGrafik && !hitKhalil) || facing < .30f) {
            status = "MONSTER OUT OF REACH";
            statusFrames = 70;
            return;
        }
        swordCooldown = swordEquipped ? .34f : .55f;
        if (lanMode == 2) {
            LanPacket hit{};
            hit.type = 6;
            hit.player = networkId;
            hit.x = px;
            hit.y = feet;
            hit.z = pz;
            hit.yaw = yaw;
            hit.flags = hitGrafik ? 1 : 0;
            hit.block = (uint8_t)damage;
            sendLan(hit);
            status = "SWORD SWING";
            statusFrames = 35;
            return;
        }
        if (hitGrafik) {
            grafikHealth = std::max(0, grafikHealth - damage);
            grafikHitTimer = .28f;
            if (lanMode == 1) {
                LanPacket state{};
                state.type = 7;
                state.player = networkId;
                state.flags = 1;
                state.block = (uint8_t)grafikHealth;
                sendLan(state);
            }
            status = grafikHealth <= 0 ? "GRAFIK DEFEATED" : "GRAFIK HIT";
            statusFrames = 90;
            if (grafikHealth <= 0) {
                grafikAlive = false;
                grafikRespawn = 0;
            }
        } else {
            khalilHealth = std::max(0, khalilHealth - damage);
            khalilHitTimer = .28f;
            status = khalilHealth <= 0 ? "KHALIL DEFEATED" : "KHALIL HIT";
            statusFrames = 90;
            if (khalilHealth <= 0) {
                khalilAlive = false;
                khalilRespawn = 0;
                khalilPrimed = false;
                khalilFuse = 0;
            }
        }
    }
    // Updates Grafik's night behavior, movement, and respawning.
    float dt) {
        grafikHitTimer = std::max(0.0f, grafikHitTimer - dt);
        if (!isNight()) {
            grafikAlive = false;
            return;
        }
        if (lanMode == 2)
            return;
        if (!grafikAlive) {
            grafikRespawn += dt;
            if (grafikRespawn > 18.0f) {
                grafikAlive = true;
                grafikHealth = 6;
                grafikHitTimer = 0;
                grafikX = px + 6.0f;
                grafikZ = pz - 5.0f;
                grafikFeet = mobSurfaceHeight(grafikX, grafikZ, feet);
                if (grafikFeet < 0.0f)
                    grafikFeet =
                        (float)groundHeight((int)std::floor(grafikX), (int)std::floor(grafikZ));
            }
            return;
        }
        if (playerDead || creativeMode)
            return;
        float dx = px - grafikX, dz = pz - grafikZ, dist = std::sqrt(dx * dx + dz * dz);
        if (dist < .82f) {
            playerDead = true;
            swallowTimer = .001f;
            deathMessage = "GRAFIK ATE YOU";
            return;
        }
        if (dist > 1.0f) {
            float step = std::min(2.35f * dt, dist - .72f);
            float nx = grafikX + dx / dist * step, nz = grafikZ + dz / dist * step;
            moveMobOnBlocks(grafikX, grafikZ, grafikFeet, nx, nz, .38f, 1.30f);
        }
    }
