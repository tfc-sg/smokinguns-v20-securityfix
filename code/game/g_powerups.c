/*
 * Map-specific entity overlays for Smokin' Guns.
 * Copyright (C) 2026. Distributed under GPL-2.0-or-later, as the game code.
 * Historical powerups files are input data; this is a new implementation.
 */
#include "g_local.h"

#ifdef SMOKINGUNS
#define PU_MAX_FILE 131072
#define PU_MAX_BLOCKS 256
static char powerupFile[PU_MAX_FILE];
static char powerupStatus[256];
static qboolean powerupApplying;
static int powerupLoaded, powerupRemoved;

typedef struct {
    const char *cursor;
    int line;
    char error[128];
} powerupParser_t;

static int PU_Error(powerupParser_t *p, const char *message) {
    Q_strncpyz(p->error, message, sizeof(p->error));
    return -1;
}

/* Return 0=EOF, 1=word (including quoted empty words), 2={, 3=}, -1=error.
 * Do not use COM_Parse's empty string to distinguish EOF from model "". */
static int PU_Token(powerupParser_t *p, char *out) {
    const char *s = p->cursor;
    int n = 0;
    for (;;) {
        while (*s && (unsigned char)*s <= ' ') {
            if (*s++ == '\n') p->line++;
        }
        if (s[0] == '/' && s[1] == '/') {
            while (*s && *s != '\n') s++;
        } else if (s[0] == '/' && s[1] == '*') {
            s += 2;
            while (*s && !(s[0] == '*' && s[1] == '/')) {
                if (*s++ == '\n') p->line++;
            }
            if (!*s) return PU_Error(p, "unterminated block comment");
            s += 2;
        } else break;
    }
    out[0] = 0;
    if (!*s) { p->cursor = s; return 0; }
    if (*s == '{' || *s == '}') {
        int kind = *s == '{' ? 2 : 3;
        p->cursor = s + 1;
        return kind;
    }
    if (*s == '"') {
        s++;
        while (*s && *s != '"') {
            if (*s == '\n' || *s == '\r')
                return PU_Error(p, "newline in quoted token");
            if (n >= MAX_TOKEN_CHARS - 1) return PU_Error(p, "token too long");
            out[n++] = *s++;
        }
        if (!*s) return PU_Error(p, "unterminated quoted token");
        s++;
    } else {
        while (*s && (unsigned char)*s > ' ' && *s != '{' && *s != '}') {
            if (s[0] == '/' && (s[1] == '/' || s[1] == '*')) break;
            if (n >= MAX_TOKEN_CHARS - 1) return PU_Error(p, "token too long");
            out[n++] = *s++;
        }
    }
    out[n] = 0;
    p->cursor = s;
    return 1;
}

static int PU_Block(powerupParser_t *p) {
    char key[MAX_TOKEN_CHARS], value[MAX_TOKEN_CHARS];
    int token, k, v;
    level.numSpawnVars = level.numSpawnVarChars = 0;
    token = PU_Token(p, key);
    if (token <= 0) return token;
    if (token != 2) return PU_Error(p, "expected opening brace");
    for (;;) {
        token = PU_Token(p, key);
        if (token == 3) return 1;
        if (token != 1 || !key[0]) return PU_Error(p, "expected key or closing brace");
        if (PU_Token(p, value) != 1) return PU_Error(p, "expected value");
        k = strlen(key) + 1; v = strlen(value) + 1;
        if (level.numSpawnVars >= MAX_SPAWN_VARS ||
                level.numSpawnVarChars + k + v > MAX_SPAWN_VARS_CHARS)
            return PU_Error(p, "entity exceeds spawn variable limits");
        level.spawnVars[level.numSpawnVars][0] = level.spawnVarChars + level.numSpawnVarChars;
        memcpy(level.spawnVarChars + level.numSpawnVarChars, key, k);
        level.numSpawnVarChars += k;
        level.spawnVars[level.numSpawnVars][1] = level.spawnVarChars + level.numSpawnVarChars;
        memcpy(level.spawnVarChars + level.numSpawnVarChars, value, v);
        level.numSpawnVarChars += v;
        level.numSpawnVars++;
    }
}

/* Last key wins, consistently with G_ParseField. */
static const char *PU_Value(const char *key, const char *fallback) {
    int i;
    for (i = level.numSpawnVars - 1; i >= 0; i--)
        if (!Q_stricmp(level.spawnVars[i][0], key)) return level.spawnVars[i][1];
    return fallback;
}

static const char *PU_Class(void) {
    const char *name = PU_Value("classname", "");
    if (!strcmp(name, "info_weapon_sawedoff")) return "weapon_sawedoff";
    if (!strcmp(name, "info_item_belt")) return "item_belt";
    if (!strcmp(name, "bot_roam")) return "item_botroam";
    return name;
}

static qboolean PU_PointClass(const char *name) {
    static const char *classes[] = {
        "info_player_deathmatch", "info_player_start", "info_player_intermission",
        "team_CTF_blueplayer", "team_CTF_redplayer", "team_CTF_bluespawn",
        "team_CTF_redspawn", "item_botroam", "target_speaker", NULL
    };
    int i;
    for (i = 0; classes[i]; i++) if (!strcmp(classes[i], name)) return qtrue;
    return BG_FindItemForClassname(name) != NULL;
}

static const char *PU_RemoveClass(void) {
    const char *name = PU_Class();
    if (!strcmp(name, "remove_target_entities")) return PU_Value("target", "");
    if (!Q_strncmp(name, "remove_", 7)) return name + 7;
    return NULL;
}

/* QVM sscanf does not report assignment counts. Validate decimal syntax here
 * rather than relying on the host C library's different sscanf behavior. */
static qboolean PU_Number(const char *s, int dimensions) {
    int i, digits, sign;
    float value, fraction;
    for (i = 0; i < dimensions; i++) {
        while (*s && (unsigned char)*s <= ' ') s++;
        sign = 1; digits = 0; value = 0;
        if (*s == '+' || *s == '-') { if (*s == '-') sign = -1; s++; }
        while (*s >= '0' && *s <= '9') {
            value = value * 10 + (*s++ - '0'); digits++;
            if (value > 1000000) return qfalse;
        }
        if (*s == '.') {
            s++; fraction = 0.1f;
            while (*s >= '0' && *s <= '9') {
                value += (*s++ - '0') * fraction; fraction *= 0.1f; digits++;
            }
        }
        if (!digits || value * sign > 1000000 || value > 1000000) return qfalse;
        if (*s && (unsigned char)*s > ' ') return qfalse;
    }
    while (*s && (unsigned char)*s <= ' ') s++;
    return !*s;
}

static qboolean PU_Model(char *out) {
    const char *model = PU_Value("model", "");
    float scale = atof(PU_Value("modelscale", "1"));
    char base[MAX_QPATH];
    int len = strlen(model), size;
    fileHandle_t f = 0;
    if (Q_strncmp(model, "models/", 7) || strstr(model, "..") ||
            len >= MAX_QPATH - 16 || scale < 0.125f || scale > 8) return qfalse;
    Q_strncpyz(base, model, sizeof(base));
    if (len > 4 && !Q_stricmp(base + len - 4, ".md3")) base[len - 4] = 0;
    if (scale == 1) Com_sprintf(out, MAX_QPATH, "%s.md3", base);
    else Com_sprintf(out, MAX_QPATH, "%s_pu%d.md3", base, (int)(scale * 1000 + 0.5f));
    size = trap_FS_FOpenFile(out, &f, FS_READ);
    if (f) trap_FS_FCloseFile(f);
    return size > 0;
}

static qboolean PU_Validate(powerupParser_t *p) {
    const char *name = PU_Class(), *remove = PU_RemoveClass();
    char model[MAX_QPATH];
    vec3_t mins, maxs;
    int i;
    static const char *vectors[] = {"origin", "angles", "mins", "maxs", "color", NULL};
    static const char *numbers[] = {"angle", "count", "health", "dmg", "wait", "random",
        "spawnflags", "modelscale", "animationStart", "animationEnd", "animationFPS",
        "nobots", "nohumans", "radius", "minsize", "maxsize", "trans", "weight", NULL};
    for (i = 0; vectors[i]; i++) {
        const char *v = PU_Value(vectors[i], NULL);
        if (v && !PU_Number(v, 3)) { PU_Error(p, "invalid vector"); return qfalse; }
    }
    for (i = 0; numbers[i]; i++) {
        const char *v = PU_Value(numbers[i], NULL);
        if (v && !PU_Number(v, 1)) { PU_Error(p, "invalid number"); return qfalse; }
    }
    if (remove) {
        if (!PU_PointClass(remove) || !strcmp(remove, "target_speaker") ||
                !strcmp(remove, "info_player_intermission") || !strcmp(remove, "item_botroam")) {
            PU_Error(p, "unsupported removal class"); return qfalse;
        }
        return qtrue;
    }
    if (!strcmp(name, "message")) {
        if (!PU_Value("message", "")[0]) { PU_Error(p, "empty message"); return qfalse; }
        return qtrue;
    }
    if (!PU_Value("origin", NULL)) { PU_Error(p, "missing origin"); return qfalse; }
    if (!strcmp(name, "target_speaker") && !PU_Value("noise", "")[0]) {
        PU_Error(p, "speaker without noise"); return qfalse;
    }
    if (!strcmp(name, "misc_model") || !strcmp(name, "misc_externalmodel") ||
            !strcmp(name, "func_breakable") || !strcmp(name, "func_flare")) {
        if (!PU_Model(model)) { PU_Error(p, "missing or invalid external model (including scale variant)"); return qfalse; }
    } else if (!PU_PointClass(name) && strcmp(name, "invisible_hurt") &&
            strcmp(name, "eternal_fire") && strcmp(name, "trigger_escape")) {
        PU_Error(p, "unsupported entity class"); return qfalse;
    }
    if (!!PU_Value("mins", NULL) != !!PU_Value("maxs", NULL)) {
        PU_Error(p, "mins and maxs must be supplied together"); return qfalse;
    }
    if (PU_Value("mins", NULL)) {
        sscanf(PU_Value("mins", ""), "%f %f %f", &mins[0], &mins[1], &mins[2]);
        sscanf(PU_Value("maxs", ""), "%f %f %f", &maxs[0], &maxs[1], &maxs[2]);
        for (i = 0; i < 3; i++) if (mins[i] > maxs[i]) {
            PU_Error(p, "inverted bounds"); return qfalse;
        }
    }
    if ((!strcmp(name, "func_breakable") || !strcmp(name, "trigger_escape")) &&
            !PU_Value("mins", NULL)) {
        PU_Error(p, "explicit bounds required"); return qfalse;
    }
    return qtrue;
}

static void PU_Bounds(gentity_t *ent, float radius) {
    vec3_t mins, maxs, forward, right, up, point;
    int i, j;
    VectorSet(mins, -radius, -radius, -radius);
    VectorSet(maxs, radius, radius, radius);
    if (PU_Value("mins", NULL)) {
        sscanf(PU_Value("mins", ""), "%f %f %f", &mins[0], &mins[1], &mins[2]);
        sscanf(PU_Value("maxs", ""), "%f %f %f", &maxs[0], &maxs[1], &maxs[2]);
    }
    /* File bounds are local to the model. Network collision uses a world AABB. */
    AngleVectors(ent->s.angles, forward, right, up);
    VectorNegate(right, right); /* Model local +Y is left, not AngleVectors right. */
    ClearBounds(ent->r.mins, ent->r.maxs);
    for (i = 0; i < 8; i++) {
        VectorClear(point);
        for (j = 0; j < 3; j++) {
            float value = (i & (1 << j)) ? maxs[j] : mins[j];
            VectorMA(point, value, j == 0 ? forward : j == 1 ? right : up, point);
        }
        AddPointToBounds(point, ent->r.mins, ent->r.maxs);
    }
}

static void PU_HurtTouch(gentity_t *self, gentity_t *other, trace_t *trace) {
    (void)trace;
    if (!other->takedamage || other->timestamp > level.time) return;
    other->timestamp = level.time + 1000;
    G_Damage(other, self, &g_entities[ENTITYNUM_WORLD], NULL, NULL,
        self->damage, DAMAGE_NO_KNOCKBACK, !strcmp(self->classname, "eternal_fire") ? MOD_MOLOTOV : MOD_TRIGGER_HURT);
}

static void PU_FireThink(gentity_t *self) {
    vec3_t normal = {0, 0, 1}, mins, maxs;
    int entities[MAX_GENTITIES], count, i;
    /* Existing clients render the existing fire event. Refresh for late joiners. */
    if (self->timestamp <= level.time) {
        G_AddEvent(self, EV_WHISKEY_BURNS, DirToByte(normal));
        self->timestamp = level.time + WHISKEY_BURNTIME / 2;
    }
    VectorAdd(self->r.currentOrigin, self->r.mins, mins);
    VectorAdd(self->r.currentOrigin, self->r.maxs, maxs);
    count = trap_EntitiesInBox(mins, maxs, entities, MAX_GENTITIES);
    for (i = 0; i < count; i++) {
        gentity_t *hit = &g_entities[entities[i]];
        if (!hit->takedamage) continue;
        if (hit->r.contents & (CONTENTS_EXPLOSIVE | CONTENTS_FLAMMABLE))
            G_Damage(hit, self, &g_entities[ENTITYNUM_WORLD], NULL, NULL,
                (hit->r.contents & CONTENTS_EXPLOSIVE) ? 100 : 10, 0, MOD_MOLOTOV);
    }
    self->nextthink = level.time + 250;
}

qboolean G_PowerupItemBlocked(gentity_t *ent) {
    gentity_t *safe = ent->powerupPeer;
    return ent->mapPowerup && ent->item && safe && safe->inuse &&
        safe->mapPowerup && safe->powerupHealth > 0 && !(safe->flags & EF_BROKEN);
}

static void PU_SafeState(gentity_t *safe, qboolean opened) {
    int i;
    for (i = MAX_CLIENTS; i < level.num_entities; i++) {
        gentity_t *ent = &g_entities[i];
        if (!ent->inuse || !ent->mapPowerup || ent->powerupPeer != safe) continue;
        if (opened) {
            if (ent->item) {
                if (ent->s.eType == ET_ITEM) RespawnItem(ent);
            } else {
                ent->s.eFlags &= ~EF_NODRAW;
                trap_LinkEntity(ent);
            }
        } else {
            ent->s.eFlags |= EF_NODRAW;
            ent->r.contents = 0;
            trap_UnlinkEntity(ent);
        }
    }
}

extern void func_breakable_die(gentity_t *, gentity_t *, gentity_t *, int, int);
static void PU_BreakableDie(gentity_t *self, gentity_t *inflictor,
        gentity_t *attacker, int damage, int mod) {
    func_breakable_die(self, inflictor, attacker, damage, mod);
    PU_SafeState(self, qtrue);
}

void G_SpawnPowerupBreakable(gentity_t *ent) {
    if (!ent->powerupHealth) {
        gitem_t *weapon;
        ent->powerupHealth = ent->health > 0 ? ent->health : 100;
        weapon = BG_FindItemForClassname(PU_Value("weapon", ""));
        if (weapon && weapon->giType == IT_WEAPON) ent->s.weapon = weapon->giTag;
        VectorCopy(ent->s.origin, ent->pos1);
        VectorCopy(ent->s.origin, ent->pos2);
    }
    ent->health = ent->powerupHealth;
    ent->s.angles2[0] = ent->health;
    ent->s.eType = ET_BREAKABLE;
    ent->s.powerups = FARCLIP_NONE;
    ent->flags &= ~EF_BROKEN;
    ent->s.eFlags &= ~EF_NODRAW;
    ent->wait = 0;
    ent->takedamage = qtrue;
    ent->die = PU_BreakableDie;
    ent->trio = (ent->spawnflags & 4) ? SURF_METAL : SURF_WOOD;
    ent->r.bmodel = qfalse;
    ent->r.contents = CONTENTS_BODY | CONTENTS_MOVER;
    G_SetOrigin(ent, ent->pos1);
    trap_LinkEntity(ent);
    PU_SafeState(ent, qfalse);
}

extern void SP_misc_externalmodel(gentity_t *);
extern void Touch_Escape(gentity_t *, gentity_t *, trace_t *);
qboolean G_SpawnPowerupEntity(gentity_t *ent) {
    const char *name;
    char model[MAX_QPATH];
    if (!powerupApplying) return qfalse;
    ent->mapPowerup = qtrue;
    name = PU_Class();
    ent->classname = G_NewString(name);
    if (!strcmp(name, "misc_model") || !strcmp(name, "misc_externalmodel") ||
            !strcmp(name, "func_breakable") || !strcmp(name, "func_flare")) {
        PU_Model(model); /* Already validated before any mutation. */
        ent->model = G_NewString(model);
        ent->s.modelindex = G_ModelIndex(model);
        PU_Bounds(ent, 0);
        G_SetOrigin(ent, ent->s.origin);
        VectorCopy(ent->s.angles, ent->s.apos.trBase);
        if (!strcmp(name, "func_breakable")) G_SpawnPowerupBreakable(ent);
        else if (!strcmp(name, "func_flare")) {
            ent->s.eType = ET_FLARE;
            ent->s.eFlags = ent->spawnflags;
            ent->r.contents = CONTENTS_DETAIL;
            G_SpawnInt("maxsize", "120", &ent->s.time);
            G_SpawnInt("minsize", "30", &ent->s.time2);
            G_SpawnInt("radius", "20", &ent->s.frame);
            G_SpawnFloat("trans", "0.6", &ent->s.angles[0]);
            G_SpawnVector("color", "1 1 1", ent->s.angles2);
            trap_LinkEntity(ent);
        } else {
            if (PU_Value("mins", NULL)) ent->r.contents = CONTENTS_SOLID;
            SP_misc_externalmodel(ent);
        }
        return qtrue;
    }
    if (!strcmp(name, "invisible_hurt") || !strcmp(name, "eternal_fire")) {
        PU_Bounds(ent, !strcmp(name, "eternal_fire") ? 24 : 8);
        ent->damage = ent->damage > 0 ? ent->damage : 10;
        ent->r.contents = CONTENTS_TRIGGER;
        ent->touch = PU_HurtTouch;
        ent->s.eType = ET_INVISIBLE;
        if (!strcmp(name, "eternal_fire")) {
            ent->s.eType = ET_GENERAL;
            ent->think = PU_FireThink;
            ent->nextthink = level.time + FRAMETIME;
        } else ent->r.svFlags |= SVF_NOCLIENT;
        trap_LinkEntity(ent);
        return qtrue;
    }
    if (!strcmp(name, "trigger_escape")) {
        PU_Bounds(ent, 0);
        ent->s.eType = ET_ESCAPE;
        ent->r.contents = CONTENTS_TRIGGER;
        ent->touch = Touch_Escape;
        VectorAdd(ent->r.mins, ent->r.maxs, ent->s.angles2);
        VectorMA(ent->r.currentOrigin, 0.5f, ent->s.angles2, ent->s.angles2);
        trap_LinkEntity(ent);
        return qtrue;
    }
    return qfalse; /* Existing point and item spawn functions, including aliases. */
}

static void PU_LinkSafes(void) {
    int i, j;
    for (i = MAX_CLIENTS; i < level.num_entities; i++) {
        gentity_t *safe = &g_entities[i];
        if (!safe->inuse || !safe->mapPowerup || !safe->powerupHealth ||
                !safe->model || !strstr(safe->model, "/safe.md3")) continue;
        for (j = MAX_CLIENTS; j < level.num_entities; j++) {
            gentity_t *ent = &g_entities[j];
            if (!ent->inuse || !ent->mapPowerup || ent == safe) continue;
            if (fabs(ent->s.origin[0] - safe->s.origin[0]) > 1 ||
                    fabs(ent->s.origin[1] - safe->s.origin[1]) > 1 ||
                    fabs(ent->s.origin[2] - safe->s.origin[2]) > 64) continue;
            if ((ent->item && ent->item->giType == IT_POWERUP && ent->item->giTag == PW_GOLD) ||
                    (ent->model && strstr(ent->model, "/safe_broken.md3")))
                ent->powerupPeer = safe;
        }
        PU_SafeState(safe, qfalse);
    }
}

/* Explicit opt-in: let a BR map with added DM spawns run as deathmatch,
 * or exercise a DM map's added bank robbery entities. -1 retains SG prefixes. */
int G_PowerupGametype(int prefixGametype) {
    vmCvar_t override;
    trap_Cvar_Register(&override, "g_mapPowerupsGametype", "-1", CVAR_ARCHIVE);
    if (override.integer == GT_FFA || override.integer == GT_TEAM ||
            override.integer == GT_RTP || override.integer == GT_BR)
        return override.integer;
    return prefixGametype;
}

void G_LoadPowerupEntities(void) {
    powerupParser_t parser;
    fileHandle_t f = 0;
    vmCvar_t enabled, variant;
    char map[MAX_QPATH], path[MAX_QPATH], defaultPath[MAX_QPATH];
    const char *remove;
    int len, token, blocks = 0, i, cutoff = level.num_entities, freeSlots;
    powerupLoaded = powerupRemoved = 0;
    powerupApplying = qfalse;
    Q_strncpyz(powerupStatus, "disabled", sizeof(powerupStatus));
    trap_Cvar_Register(&enabled, "g_mapPowerups", "1", CVAR_ARCHIVE);
    if (!enabled.integer) return;
    trap_Cvar_VariableStringBuffer("mapname", map, sizeof(map));
    for (i = 0; map[i]; i++) if (!((map[i] >= 'a' && map[i] <= 'z') ||
            (map[i] >= 'A' && map[i] <= 'Z') || (map[i] >= '0' && map[i] <= '9') ||
            map[i] == '_' || map[i] == '-')) {
        Q_strncpyz(powerupStatus, "rejected: invalid map name", sizeof(powerupStatus)); return;
    }
    if (!map[0] || strlen(map) + 14 >= sizeof(path)) {
        Q_strncpyz(powerupStatus, "rejected: map name too long", sizeof(powerupStatus)); return;
    }
    Com_sprintf(defaultPath, sizeof(defaultPath), "powerups/%s.txt", map);
    Q_strncpyz(path, defaultPath, sizeof(path));
    trap_Cvar_Register(&variant, "g_mapPowerupsVariant", "", CVAR_ARCHIVE);
    if (variant.string[0]) {
        for (i = 0; variant.string[i]; i++) if (!((variant.string[i] >= 'a' && variant.string[i] <= 'z') ||
                (variant.string[i] >= 'A' && variant.string[i] <= 'Z') ||
                (variant.string[i] >= '0' && variant.string[i] <= '9') ||
                variant.string[i] == '_' || variant.string[i] == '-')) {
            Q_strncpyz(powerupStatus, "rejected: invalid powerups variant", sizeof(powerupStatus)); return;
        }
        if (strlen(defaultPath) + strlen(variant.string) + 2 > sizeof(path)) {
            Q_strncpyz(powerupStatus, "rejected: powerups variant too long", sizeof(powerupStatus)); return;
        }
        Com_sprintf(path, sizeof(path), "%s.%s", defaultPath, variant.string);
    }
    len = trap_FS_FOpenFile(path, &f, FS_READ);
    if ((!f || len < 0) && variant.string[0]) {
        if (f) trap_FS_FCloseFile(f);
        f = 0; Q_strncpyz(path, defaultPath, sizeof(path));
        len = trap_FS_FOpenFile(path, &f, FS_READ);
    }
    if (len <= 0 || !f) {
        if (f) trap_FS_FCloseFile(f);
        Com_sprintf(powerupStatus, sizeof(powerupStatus), "%s: no file", path); return;
    }
    if (len >= (int)sizeof(powerupFile)) {
        trap_FS_FCloseFile(f);
        Com_sprintf(powerupStatus, sizeof(powerupStatus), "%s: file too large", path); return;
    }
    trap_FS_Read(powerupFile, len, f);
    trap_FS_FCloseFile(f);
    powerupFile[len] = 0;
    memset(&parser, 0, sizeof(parser)); parser.cursor = powerupFile; parser.line = 1;
    if (len >= 3 && (unsigned char)powerupFile[0] == 0xef && (unsigned char)powerupFile[1] == 0xbb &&
            (unsigned char)powerupFile[2] == 0xbf) parser.cursor += 3;
    for (i = 0; i < len; i++) if (!powerupFile[i]) {
        PU_Error(&parser, "embedded NUL byte"); goto rejected;
    }
    freeSlots = ENTITYNUM_MAX_NORMAL - level.num_entities;
    for (i = MAX_CLIENTS; i < level.num_entities; i++)
        if (!g_entities[i].inuse && !g_entities[i].neverFree) freeSlots++;
    while ((token = PU_Block(&parser)) > 0) {
        if (++blocks > PU_MAX_BLOCKS) { PU_Error(&parser, "too many blocks"); goto rejected; }
        if (!PU_Validate(&parser)) goto rejected;
    }
    if (token < 0) goto rejected;
    if (blocks + 32 > freeSlots) { PU_Error(&parser, "insufficient entity capacity"); goto rejected; }
    /* Validation is complete. Each pass starts with a new, advancing cursor. */
    parser.cursor = powerupFile + (len >= 3 && (unsigned char)powerupFile[0] == 0xef ? 3 : 0); parser.line = 1;
    while (PU_Block(&parser) > 0) {
        remove = PU_RemoveClass();
        if (!remove) continue;
        for (i = MAX_CLIENTS; i < cutoff; i++) {
            gentity_t *ent = &g_entities[i];
            if (ent->inuse && !ent->client && !ent->neverFree && ent->classname &&
                    !strcmp(ent->classname, remove)) {
                G_FreeEntity(ent); powerupRemoved++;
            }
        }
    }
    parser.cursor = powerupFile + (len >= 3 && (unsigned char)powerupFile[0] == 0xef ? 3 : 0); parser.line = 1;
    powerupApplying = qtrue;
    while (PU_Block(&parser) > 0) {
        if (PU_RemoveClass()) continue;
        if (!strcmp(PU_Class(), "message")) {
            trap_SetConfigstring(CS_MESSAGE, PU_Value("message", "")); continue;
        }
        G_SpawnGEntityFromSpawnVars();
    }
    powerupApplying = qfalse;
    PU_LinkSafes();
    for (i = MAX_CLIENTS; i < level.num_entities; i++)
        if (g_entities[i].inuse && g_entities[i].mapPowerup) powerupLoaded++;
    Com_sprintf(powerupStatus, sizeof(powerupStatus), "%s: applied %d blocks, spawned %d, removed %d",
        path, blocks, powerupLoaded, powerupRemoved);
    G_Printf("Map powerups: %s\n", powerupStatus);
    return;
rejected:
    Com_sprintf(powerupStatus, sizeof(powerupStatus), "%s: rejected at line %d: %s; original entities retained",
        path, parser.line, parser.error);
    G_Printf("Map powerups: %s\n", powerupStatus);
}

void Svcmd_Powerups_f(void) {
    int i, live = 0, linked = 0, safes = 0, fires = 0, spawns = 0, money = 0;
    int closed = 0, hiddenMoney = 0, openModels = 0;
    for (i = MAX_CLIENTS; i < level.num_entities; i++) {
        gentity_t *ent = &g_entities[i];
        if (!ent->inuse || !ent->mapPowerup) continue;
        live++; if (ent->r.linked) linked++;
        if (ent->powerupHealth) safes++;
        if (ent->powerupHealth && ent->model && strstr(ent->model, "/safe.md3") &&
                !(ent->flags & EF_BROKEN)) closed++;
        if (G_PowerupItemBlocked(ent) && !ent->r.linked && (ent->s.eFlags & EF_NODRAW)) hiddenMoney++;
        if (ent->powerupPeer && !ent->item && ent->r.linked && !(ent->s.eFlags & EF_NODRAW)) openModels++;
        if (!strcmp(ent->classname, "eternal_fire")) fires++;
        if (!strcmp(ent->classname, "info_player_deathmatch")) spawns++;
        if (ent->item && ent->item->giType == IT_POWERUP && ent->item->giTag == PW_GOLD) money++;
    }
    G_Printf("Map powerups: %s\n", powerupStatus);
    G_Printf("Map powerups: live=%d linked=%d breakables=%d fire=%d dm_spawns=%d money=%d\n",
        live, linked, safes, fires, spawns, money);
    G_Printf("Map powerups: closed_safes=%d hidden_safe_money=%d visible_open_safes=%d\n",
        closed, hiddenMoney, openModels);
}
#endif
