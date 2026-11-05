#include <assert.h>
#include <stdio.h>
#include <stdarg.h>
#include "../code/game/g_powerups.c"

level_locals_t level;
gentity_t g_entities[MAX_GENTITIES];
static char arena[200000], input[PU_MAX_FILE + 20];
static int arenaUsed, spawned, freed, fsEnabled = 1, modelsExist = 1;
static gitem_t item;
static int gametypeOverride=-1, touchIndex=-1;
static char variantName[64];
static char lastConfig[1024], parseError[256];
char *G_NewString(const char *s) { char *p = arena + arenaUsed; arenaUsed += strlen(s)+1; assert(arenaUsed < sizeof(arena)); strcpy(p,s);return p; }
void QDECL G_Printf(const char *fmt, ...) { }
void QDECL Com_Printf(const char *fmt, ...) { }
void QDECL Com_Error(int code, const char *fmt, ...) { assert(!"unexpected Com_Error"); }
void trap_Cvar_Register(vmCvar_t *v,const char *n,const char *d,int f) { memset(v,0,sizeof(*v));v->integer=!strcmp(n,"g_mapPowerupsGametype")?gametypeOverride:fsEnabled;
 if (!strcmp(n,"g_mapPowerupsVariant")) Q_strncpyz(v->string,variantName,sizeof(v->string)); }
void trap_Cvar_VariableStringBuffer(const char *n,char *v,int size) { Q_strncpyz(v,"testmap",size); }
int trap_FS_FOpenFile(const char *name,fileHandle_t *f,fsMode_t mode) {
 if (!Q_strncmp(name,"models/",7)) { *f=modelsExist?2:0; return modelsExist?100:-1; }
 *f=1;return strlen(input);
}
void trap_FS_Read(void *b,int n,fileHandle_t f) { memcpy(b,input,n); }
void trap_FS_FCloseFile(fileHandle_t f) { }
void trap_SetConfigstring(int i,const char *s) { Q_strncpyz(lastConfig,s,sizeof(lastConfig)); }
void trap_LinkEntity(gentity_t *e) { e->r.linked=qtrue; }
int trap_EntitiesInBox(const vec3_t mins,const vec3_t maxs,int *list,int maxcount) { if(touchIndex<0)return 0;list[0]=touchIndex;return 1; }
void trap_UnlinkEntity(gentity_t *e) { e->r.linked=qfalse; }
void trap_Argv(int i,char *s,int n) { *s=0; }
void G_FreeEntity(gentity_t *e) { e->inuse=qfalse;freed++; }
void G_SpawnGEntityFromSpawnVars(void) {
 int i;
 for (i=MAX_CLIENTS;i<ENTITYNUM_MAX_NORMAL;i++) if (!g_entities[i].inuse) break;
 assert(i<ENTITYNUM_MAX_NORMAL);
 if (i>=level.num_entities) level.num_entities=i+1;
 g_entities[i].inuse=qtrue;g_entities[i].mapPowerup=qtrue;
 g_entities[i].classname=G_NewString(PU_Class());spawned++;
}
gitem_t *BG_FindItemForClassname(const char *s) {
 if (!strcmp(s,"pickup_money") || !strcmp(s,"item_money") || !strcmp(s,"weapon_sawedoff") || !strcmp(s,"weapon_dynamite") || !strcmp(s,"item_belt") || !strcmp(s,"ammo_dynamite")) return &item;
 return NULL;
}
void RespawnItem(gentity_t *e) { e->s.eFlags &= ~EF_NODRAW;e->r.contents=CONTENTS_TRIGGER;trap_LinkEntity(e); }
void G_SetOrigin(gentity_t *e,vec3_t o) { VectorCopy(o,e->r.currentOrigin);VectorCopy(o,e->s.pos.trBase); }
int G_ModelIndex(char *s) { return 1; }
qboolean G_SpawnInt(const char *key,const char *def,int *out) { *out=atoi(PU_Value(key,def)); return qtrue; }
qboolean G_SpawnFloat(const char *key,const char *def,float *out) { *out=atof(PU_Value(key,def));return qtrue; }
qboolean G_SpawnVector(const char *key,const char *def,float *out) { sscanf(PU_Value(key,def),"%f %f %f",out,out+1,out+2);return qtrue; }
void G_AddEvent(gentity_t *e,int ev,int parm) { e->s.event=ev; }
void G_Damage(gentity_t *t,gentity_t *i,gentity_t *a,vec3_t d,vec3_t p,float damage,int flags,int mod) { t->health-=damage; }
void SP_misc_externalmodel(gentity_t *e) { trap_LinkEntity(e); }
void Touch_Escape(gentity_t *s,gentity_t *o,trace_t *t) { }
void func_breakable_die(gentity_t *s,gentity_t *i,gentity_t *a,int damage,int mod) { s->flags|=EF_BROKEN;s->takedamage=qfalse;trap_UnlinkEntity(s); }

static void reset(const char *text) {
 memset(&level,0,sizeof(level));memset(g_entities,0,sizeof(g_entities));
 level.num_entities=MAX_CLIENTS+1;level.time=1000;
 g_entities[MAX_CLIENTS].inuse=qtrue;g_entities[MAX_CLIENTS].classname="info_player_deathmatch";
 arenaUsed=spawned=freed=0;modelsExist=fsEnabled=1;lastConfig[0]=variantName[0]=0;gametypeOverride=-1;touchIndex=-1;
 Q_strncpyz(input,text,sizeof(input));
}
static int parse(const char *text) {
 powerupParser_t p;int token,n=0;
 memset(&p,0,sizeof(p));p.cursor=text;p.line=1;
 parseError[0]=0;
 while ((token=PU_Block(&p))>0) { if (!PU_Validate(&p)) { token=-1;break; } n++; }
 if(token<0) Com_sprintf(parseError,sizeof(parseError),"line %d: %s (%s)",p.line,p.error,PU_Class());
 return token<0?-1:n;
}
int main(int argc,char **argv) {
 int checks=0,i;gentity_t *safe,*money,*open,*victim;
#define CHECK(x) do { assert(x);checks++; } while(0)
 reset("");
 CHECK(PU_Number("-12.5 +23 0",3)); CHECK(!PU_Number("1 2",3));
 CHECK(!PU_Number("1 2 3 junk",3)); CHECK(!PU_Number("nan",1));
 CHECK(!PU_Number("inf",1)); CHECK(!PU_Number("--5",1));
 CHECK(!PU_Number("1000001",1)); CHECK(PU_Number(".5",1));
 CHECK(parse("//comment\n{ classname message message \"hello // world\" } /*ok*/") == 1);
 CHECK(parse("{ classname trigger_escape origin \"0 0 0\" model \"\" mins \"-1 -1 -1\" maxs \"1 1 1\" }")==1);
 CHECK(parse("{ classname message message hello ")==-1);
 CHECK(parse("{ { classname message message hello } ")==-1);
 CHECK(parse("{ classname message message \"unterminated }")==-1);
 CHECK(parse("{ classname worldspawn origin \"0 0 0\" }")==-1);
 CHECK(parse("{ classname remove_target_entities target worldspawn }")==-1);
 CHECK(parse("{ classname remove_target_entities target info_player_deathmatch }")==1);
 CHECK(parse("{ classname invisible_hurt origin \"0 0 0\" mins \"2 0 0\" maxs \"1 1 1\" }")==-1);
 CHECK(parse("{ classname target_speaker origin \"0 0 0\" }")==-1);
 CHECK(parse("{ classname info_weapon_sawedoff origin \"0 0 0\" }")==1);
 CHECK(parse("{ classname bot_roam origin \"0 0 0\" }")==1);
 CHECK(parse("{ classname func_breakable model models/misc/safe origin \"0 0 0\" }")==-1);
 modelsExist=0;
 CHECK(parse("{ classname misc_model model models/misc/tree origin \"0 0 0\" }")==-1);
 reset("{ classname remove_info_player_deathmatch } { classname info_player_deathmatch origin \"1 2 3\" }");
 G_LoadPowerupEntities(); CHECK(freed==1 && spawned==1 && powerupLoaded==1);
 CHECK(g_entities[MAX_CLIENTS].inuse); /* new spawn survives removal pass */
 reset("{ classname remove_info_player_deathmatch } { classname no_such_class origin \"0 0 0\" }");
 G_LoadPowerupEntities(); CHECK(freed==0 && spawned==0);CHECK(g_entities[MAX_CLIENTS].inuse);
 reset("{ classname remove_info_player_deathmatch } { classname info_player_deathmatch origin \"0 0 0\" ");
 G_LoadPowerupEntities();CHECK(freed==0 && spawned==0);
 reset("{ classname remove_info_player_deathmatch }");g_entities[MAX_CLIENTS].neverFree=qtrue;
 G_LoadPowerupEntities();CHECK(freed==0);
 reset("{ classname remove_info_player_deathmatch }");fsEnabled=0;
 G_LoadPowerupEntities();CHECK(freed==0 && spawned==0);
 reset("{ classname message message \"New map title\" }");
 G_LoadPowerupEntities();CHECK(!strcmp(lastConfig,"New map title"));

 reset("/* unfinished");G_LoadPowerupEntities();CHECK(freed==0 && spawned==0);
 reset("{ classname message message title }");strcpy(variantName,"../bad");
 G_LoadPowerupEntities();CHECK(freed==0 && spawned==0 && strstr(powerupStatus,"invalid powerups variant"));
 CHECK(G_PowerupGametype(GT_BR)==GT_BR);gametypeOverride=GT_FFA;
 CHECK(G_PowerupGametype(GT_BR)==GT_FFA);gametypeOverride=GT_SINGLE_PLAYER;
 CHECK(G_PowerupGametype(GT_BR)==GT_BR);
 reset("\xef\xbb\xbf{ classname message message BOM }");
 G_LoadPowerupEntities();CHECK(!strcmp(lastConfig,"BOM"));
 reset("{ classname message message title }");
 memset(input,'x',sizeof(input)-1);input[sizeof(input)-1]=0;
 G_LoadPowerupEntities();CHECK(freed==0 && spawned==0 && strstr(powerupStatus,"too large"));
 reset("");CHECK(parse("{ classname message message hi } /* missing")==-1);

 reset(""); { powerupParser_t boundsParser = {0};
 boundsParser.cursor="{ classname invisible_hurt origin \"0 0 0\" mins \"-2 -5 -1\" maxs \"3 7 4\" }";boundsParser.line=1;
 CHECK(PU_Block(&boundsParser)==1); }
 safe=&g_entities[MAX_CLIENTS];PU_Bounds(safe,0);
 CHECK(fabs(safe->r.mins[1]+5)<0.01f && fabs(safe->r.maxs[1]-7)<0.01f);
 safe->s.angles[YAW]=90;PU_Bounds(safe,0);
 CHECK(fabs(safe->r.mins[0]+7)<0.01f && fabs(safe->r.maxs[0]-5)<0.01f);

 reset(""); { powerupParser_t blocker = {0};
 blocker.cursor="{ classname invisible_hurt origin \"10 20 30\" }";blocker.line=1;
 CHECK(PU_Block(&blocker)==1);
 safe=&g_entities[MAX_CLIENTS];VectorSet(safe->s.origin,10,20,30);
 powerupApplying=qtrue;CHECK(G_SpawnPowerupEntity(safe));powerupApplying=qfalse;
 CHECK(safe->r.linked && safe->r.contents==CONTENTS_SOLID && safe->s.eType==ET_INVISIBLE);
 CHECK(!(safe->r.svFlags & SVF_NOCLIENT) && !safe->touch);
 CHECK(safe->r.currentOrigin[2]==30 && safe->r.maxs[2]==8);
 }

 /* Actual safe state callbacks: money and open model follow break/respawn. */
 reset("");level.num_entities=MAX_CLIENTS+4;
 safe=&g_entities[MAX_CLIENTS];money=safe+1;open=safe+2;victim=safe+3;
 safe->mapPowerup=qtrue;safe->health=100;safe->model="models/misc/safe.md3";
 money->inuse=open->inuse=qtrue;money->mapPowerup=open->mapPowerup=qtrue;
 money->item=&item;money->powerupPeer=open->powerupPeer=safe;money->s.eType=ET_ITEM;
 G_SpawnPowerupBreakable(safe);
 CHECK(G_PowerupItemBlocked(money));CHECK(!open->r.linked && !money->r.linked);
 PU_BreakableDie(safe,victim,victim,100,MOD_DYNAMITE);
 CHECK(!G_PowerupItemBlocked(money));CHECK(open->r.linked && money->r.linked);
 G_SpawnPowerupBreakable(safe);
 CHECK(safe->health==100 && safe->takedamage);CHECK(!money->r.linked && !open->r.linked);
 victim->health=100;victim->takedamage=qtrue;safe->damage=10;safe->classname="eternal_fire";
 PU_HurtTouch(safe,victim,NULL);PU_HurtTouch(safe,victim,NULL);
 CHECK(victim->health==90);level.time+=1000;PU_HurtTouch(safe,victim,NULL);CHECK(victim->health==80);
 safe->classname="eternal_fire";PU_FireThink(safe);
 CHECK(safe->s.event==EV_WHISKEY_BURNS && safe->nextthink>level.time);
 victim->health=200;victim->r.contents=CONTENTS_EXPLOSIVE;touchIndex=MAX_CLIENTS+3;
 PU_FireThink(safe);CHECK(victim->health==100);
 victim->r.contents=CONTENTS_FLAMMABLE;PU_FireThink(safe);CHECK(victim->health==90);touchIndex=-1;
 printf("PASS: %d powerups checks\n",checks);
 for (i=1;i<argc;i++) {
  FILE *f=fopen(argv[i],"rb");long len;
  assert(f);fseek(f,0,SEEK_END);len=ftell(f);rewind(f);assert(len<sizeof(input));
  fread(input,1,len,f);input[len]=0;fclose(f);modelsExist=1;
  { int result=parse(input); printf("FILE %s blocks=%d %s\n",argv[i],result,parseError); }
 }
 return 0;
}
