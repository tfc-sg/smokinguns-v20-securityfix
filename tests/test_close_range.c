/* Compile the actual production helpers; mock only the engine trace syscall. */
#include <assert.h>
#include <stdio.h>
#include "../code/game/g_weapon.c"

level_locals_t level;
gentity_t g_entities[MAX_GENTITIES];
hit_data_t hit_data;
hit_info_t hit_info[NUM_HIT_LOCATIONS];
shaderInfo_t shaderInfo[MAX_BRUSHSIDES];
vmCvar_t g_debugWeapon;
static gclient_t clients[4];
static trace_t shotTrace, solidTrace;

void trap_Trace(trace_t *out, const vec3_t start, const vec3_t mins,
        const vec3_t maxs, const vec3_t end, int pass, int mask) {
    *out = mask == CONTENTS_SOLID ? solidTrace : shotTrace;
    if (out->fraction == 1) VectorCopy(end, out->endpos);
}
void QDECL G_Printf(const char *fmt, ...) { }
gentity_t *G_TempEntity(vec3_t origin, int event) { return &g_entities[100]; }

static void reset(void) {
    int i;
    hit_part_t *mesh;
    memset(&level, 0, sizeof(level));
    memset(g_entities, 0, sizeof(g_entities));
    memset(clients, 0, sizeof(clients));
    memset(&hit_data, 0, sizeof(hit_data));
    memset(&shotTrace, 0, sizeof(shotTrace));
    memset(&solidTrace, 0, sizeof(solidTrace));
    level.maxclients = 4;
    shotTrace.fraction = solidTrace.fraction = 1;
    shotTrace.entityNum = solidTrace.entityNum = ENTITYNUM_NONE;
    shotTrace.startsolid = qtrue;
    Weapon_Trace_ResetDebug(0);
    for (i = 0; i < NUM_HIT_LOCATIONS; i++) hit_info[i].hit_part = PART_LOWER;
    /* Vertical cylinder at x=5, radius=2, z=0..10. */
    mesh = &hit_data.meshes[HIT_CHEST];
    mesh->header.a1 = mesh->header.a2 = 128;
    mesh->header.length = 640;
    mesh->header.dir1[0] = mesh->header.dir2[1] = 64;
    mesh->pos[0].normal[2] = 64;
    mesh->pos[0].origin[0] = 320;
    for (i = 0; i < 4; i++) {
        g_entities[i].inuse = g_entities[i].takedamage = qtrue;
        g_entities[i].r.linked = qtrue;
        g_entities[i].r.contents = CONTENTS_BODY;
        g_entities[i].s.number = i;
        g_entities[i].client = &clients[i];
        VectorSet(g_entities[i].r.mins, -14, -14, -24);
        VectorSet(g_entities[i].r.maxs, 14, 14, 32);
    }
    g_entities[2].inuse = g_entities[3].inuse = qfalse;
}

int main(void) {
    vec3_t start = {0, 0, 5}, end = {100, 0, 5};
    trace_t result;
    float fraction;
    int location, checks = 0;
#define CHECK(expr) do { assert(expr); checks++; } while (0)
    reset();
    Weapon_Trace(&result, start, end, ENTITYNUM_NONE, &location);
    CHECK(result.entityNum == 1 && location == HIT_CHEST);
    CHECK(fabs(result.endpos[0] - 3) < 0.01f);
    CHECK(result.startsolid && !result.allsolid);

    /* A finite segment must not hit a cylinder behind the muzzle. */
    end[0] = -100;
    Weapon_Trace(&result, start, end, ENTITYNUM_NONE, &location);
    CHECK(location == -1 && result.entityNum == ENTITYNUM_NONE);
    end[0] = 100;

    /* A prior wall or own projectile remains the first obstruction. */
    shotTrace.fraction = 0.02f;
    shotTrace.entityNum = ENTITYNUM_WORLD;
    Weapon_Trace(&result, start, end, ENTITYNUM_NONE, &location);
    CHECK(location == -1 && result.entityNum == ENTITYNUM_WORLD);
    shotTrace.entityNum = 80;
    Weapon_Trace(&result, start, end, ENTITYNUM_NONE, &location);
    CHECK(location == -1 && result.entityNum == 80);

    /* Solid-world overlap cannot be turned into a player hit. */
    reset(); solidTrace.startsolid = qtrue;
    Weapon_Trace(&result, start, end, ENTITYNUM_NONE, &location);
    CHECK(location == -1);
    reset(); shotTrace.allsolid = qtrue;
    Weapon_Trace(&result, start, end, ENTITYNUM_NONE, &location);
    CHECK(location == -1);

    /* Ordinary non-overlap traces and excluded clients stay untouched. */
    reset(); shotTrace.startsolid = qfalse;
    Weapon_Trace(&result, start, end, ENTITYNUM_NONE, &location);
    CHECK(location == -1);
    reset();
    Weapon_Trace(&result, start, end, 1, &location);
    CHECK(location == -1);
    reset(); g_entities[1].inuse = qfalse;
    Weapon_Trace(&result, start, end, ENTITYNUM_NONE, &location);
    CHECK(location == -1); /* the shooter's identical model is ignored */

    /* Direction parallel to the cylinder axis, through a cap. */
    reset(); VectorSet(start, 5, 0, -5); VectorSet(end, 5, 0, 20);
    fraction = 1;
    CHECK(G_HitModelCheckSegment(&g_entities[1], start, end, &fraction) == HIT_CHEST);
    CHECK(fabs(fraction - 0.2f) < 0.001f);
    VectorSet(start, 9, 0, -5); VectorSet(end, 9, 0, 20);
    fraction = 1;
    CHECK(G_HitModelCheckSegment(&g_entities[1], start, end, &fraction) == -1);

    /* Zero length and starting in the actual body volume. */
    VectorCopy(start, end); fraction = 1;
    CHECK(G_HitModelCheckSegment(&g_entities[1], start, end, &fraction) == -1);
    VectorSet(start, 5, 0, 5); VectorSet(end, 100, 0, 5); fraction = 1;
    CHECK(G_HitModelCheckSegment(&g_entities[1], start, end, &fraction) == HIT_CHEST);
    CHECK(fraction == 0);

    /* Overlapping clients: nearest model wins, regardless of client index. */
    reset(); VectorSet(start, 0, 0, 5); VectorSet(end, 100, 0, 5);
    g_entities[2].inuse = qtrue; g_entities[2].r.currentOrigin[0] = -1;
    Weapon_Trace(&result, start, end, ENTITYNUM_NONE, &location);
    CHECK(result.entityNum == 2 && fabs(result.endpos[0] - 2) < 0.01f);
    /* Model follows the reconciled collision origin, not stale trBase. */
    CHECK(g_entities[2].s.pos.trBase[0] == 0);

    /* The box alone must never count as a body hit. */
    reset(); VectorSet(start, 0, 8, 5); VectorSet(end, 100, 8, 5);
    Weapon_Trace(&result, start, end, ENTITYNUM_NONE, &location);
    CHECK(location == -1);
    reset(); g_entities[1].r.linked = qfalse;
    VectorSet(start, 0, 0, 5); VectorSet(end, 100, 0, 5);
    Weapon_Trace(&result, start, end, ENTITYNUM_NONE, &location);
    CHECK(location == -1);
    reset(); g_entities[1].r.contents = 0;
    Weapon_Trace(&result, start, end, ENTITYNUM_NONE, &location);
    CHECK(location == -1);

    /* Actual weapon range, cap miss and a segment ending before the body. */
    reset(); end[0] = 8192*16;
    Weapon_Trace(&result, start, end, ENTITYNUM_NONE, &location);
    CHECK(location == HIT_CHEST && fabs(result.endpos[0] - 3) < 0.01f);
    start[2] = end[2] = 12; fraction = 1;
    CHECK(G_HitModelCheckSegment(&g_entities[1], start, end, &fraction) == -1);
    start[2] = end[2] = 5; end[0] = 2; fraction = 1;
    CHECK(G_HitModelCheckSegment(&g_entities[1], start, end, &fraction) == -1);

    /* Torso attachment offsets and yaw rotation use the animated model. */
    reset(); hit_info[HIT_CHEST].hit_part = PART_UPPER;
    VectorSet(hit_data.tag_torso[0].origin, 2, 0, 0);
    VectorSet(start, 0, 0, 5); VectorSet(end, 100, 0, 5); fraction = 1;
    CHECK(G_HitModelCheckSegment(&g_entities[1], start, end, &fraction) == HIT_CHEST);
    CHECK(fabs(fraction - 0.05f) < 0.001f);
    reset(); clients[1].legs_angles[YAW] = 90;
    VectorSet(start, 0, 0, 5); VectorSet(end, 0, 100, 5); fraction = 1;
    CHECK(G_HitModelCheckSegment(&g_entities[1], start, end, &fraction) == HIT_CHEST);
    CHECK(fabs(fraction - 0.03f) < 0.001f);
    printf("PASS: %d close-range regression checks\n", checks);
    return 0;
}
