#include <assert.h>
#include <stdio.h>
#include "../code/game/ai_dmnet.c"
level_locals_t level;
gentity_t g_entities[MAX_GENTITIES];
vmCvar_t bot_hearing;
float floattime;
static gclient_t clients[MAX_CLIENTS];
static int sameTeam, area=7, reachable=1, printed;
char *ClientName(int client,char *name,int size) { Q_strncpyz(name,"Tester",size);return name; }
void QDECL BotAI_Print(int type,char *fmt,...) { printed++; }
void QDECL Com_Printf(const char *fmt,...) { }
void QDECL Com_Error(int code,const char *fmt,...) { assert(!"unexpected engine error"); }
int BotSameTeam(bot_state_t *bs,int target) { return sameTeam; }
int BotPointAreaNum(vec3_t point) { return area; }
int trap_AAS_AreaReachability(int n) { return reachable; }
static void setup(bot_state_t *bs) {
 memset(bs,0,sizeof(*bs));memset(g_entities,0,sizeof(g_entities));
 bs->enemy=-1;bs->client=0;bs->heard_target=1;bs->heard_until=12;
 VectorSet(bs->heard_position,100,20,0);
 g_entities[1].inuse=qtrue;g_entities[1].health=100;g_entities[1].client=&clients[1];
 clients[1].pers.connected=CON_CONNECTED;
 sameTeam=0;area=7;reachable=1;bot_hearing.integer=1;floattime=10;
}
int main(void) {
 bot_state_t bs;bot_goal_t goal;int i,checks=0;
#define CHECK(x) do { assert(x);checks++; } while(0)
 setup(&bs);CHECK(BotHeardGoal(&bs,&goal));
 CHECK(goal.entitynum==1 && goal.areanum==7);
 CHECK(goal.origin[0]==100 && goal.origin[1]==20);
 CHECK(bs.enemy==-1); /* Hearing is not a visual enemy-search success. */
 bot_hearing.integer=0;CHECK(!BotHeardGoal(&bs,&goal));
 setup(&bs);floattime=13;CHECK(!BotHeardGoal(&bs,&goal));
 setup(&bs);sameTeam=1;CHECK(!BotHeardGoal(&bs,&goal));
 setup(&bs);g_entities[1].health=0;CHECK(!BotHeardGoal(&bs,&goal));
 setup(&bs);clients[1].pers.connected=CON_DISCONNECTED;CHECK(!BotHeardGoal(&bs,&goal));
 setup(&bs);bs.heard_target=bs.client;CHECK(!BotHeardGoal(&bs,&goal));
 setup(&bs);bs.heard_target=-1;CHECK(!BotHeardGoal(&bs,&goal));
 setup(&bs);bs.heard_target=MAX_CLIENTS;CHECK(!BotHeardGoal(&bs,&goal));
 setup(&bs);VectorClear(bs.heard_position);CHECK(!BotHeardGoal(&bs,&goal));CHECK(bs.heard_until==0);
 setup(&bs);area=0;CHECK(!BotHeardGoal(&bs,&goal));CHECK(bs.heard_until==0);
 setup(&bs);reachable=0;CHECK(!BotHeardGoal(&bs,&goal));CHECK(bs.heard_until==0);
 setup(&bs);BotResetNodeSwitches();strcpy(nodeswitch[MAX_NODESWITCHES],"sentinel");
 for(i=0;i<MAX_NODESWITCHES*4;i++) BotRecordNodeSwitch(&bs,"chase","sound","seek");
 CHECK(numnodeswitches==MAX_NODESWITCHES);
 CHECK(!strcmp(nodeswitch[MAX_NODESWITCHES],"sentinel"));
 numnodeswitches=MAX_NODESWITCHES*5;printed=0;BotDumpNodeSwitches(&bs);
 CHECK(printed<=MAX_NODESWITCHES+2);
 numnodeswitches=-2;BotRecordNodeSwitch(&bs,"chase","sound","seek");CHECK(numnodeswitches==-2);
 BotResetNodeSwitches();CHECK(numnodeswitches==0);
 printf("PASS: %d bot hearing/node-bound checks\n",checks);return 0;
}
