#include <assert.h>
#include <stdio.h>
#include "../code/game/ai_dmq3.c"
float floattime;
vmCvar_t bot_throw_interval;
static bot_state_t state;
bot_state_t *BotAI_GetBotState(int client) { return client == 0 ? &state : NULL; }
void QDECL Com_Printf(const char *fmt,...) {}
void QDECL Com_Error(int code,const char *fmt,...) { assert(0); }
static void setup(void) {
 memset(&state,0,sizeof(state));state.inuse=1;state.enemy=1;
 state.combat_pause_enemy=1;state.cur_ps.groundEntityNum=ENTITYNUM_WORLD;
 state.cur_ps.weapon=WP_REM58;state.weaponnum=WP_REM58;
 state.cur_ps.stats[STAT_HEALTH]=100;state.lastframe_health=100;
 floattime=10;bot_throw_interval.value=12;
}
int main(void) {
 int checks=0;
#define CHECK(x) do {assert(x);checks++;}while(0)
 setup();state.cur_ps.stats[STAT_WEAPONS]=(1<<WP_REM58)|(1<<WP_DYNAMITE)|(1<<WP_MOLOTOV);
 state.cur_ps.ammo[WP_REM58]=6;state.cur_ps.ammo[WP_DYNAMITE]=2;state.cur_ps.ammo[WP_MOLOTOV]=2;
 CHECK(BotChooseBestWeapon(&state.cur_ps,WP_KNIFE)==WP_MOLOTOV);
 CHECK(BotCombatGun(&state.cur_ps)==WP_REM58);
 state.cur_ps.ammo[WP_REM58]=0;CHECK(BotCombatGun(&state.cur_ps)==WP_KNIFE);
 setup();CHECK(BotCombatThrowConditions(&state,400,1,100,1,0,GT_FFA));
 CHECK(!BotCombatThrowConditions(&state,200,1,100,1,0,GT_FFA));
 CHECK(!BotCombatThrowConditions(&state,900,1,100,1,0,GT_FFA));
 CHECK(!BotCombatThrowConditions(&state,400,0,100,1,0,GT_FFA));
 CHECK(!BotCombatThrowConditions(&state,400,1,100,1,1,GT_FFA));
 CHECK(!BotCombatThrowConditions(&state,400,1,250,1,0,GT_FFA));
 CHECK(BotCombatThrowConditions(&state,400,1,250,2,0,GT_FFA));
 CHECK(!BotCombatThrowConditions(&state,400,1,100,1,0,GT_DUEL));
 state.cur_ps.powerups[PW_GOLD]=1;CHECK(!BotCombatThrowConditions(&state,400,1,100,1,0,GT_BR));
 setup();state.lastframe_health=110;CHECK(!BotCombatPause(&state,400,1,10,.25,1.5));
 setup();CHECK(BotCombatPause(&state,400,1,10,.25,1.5));
 CHECK(state.combat_pause_until==10.25f);CHECK(state.combat_pause_ready>10);
 CHECK(BotCombatPause(&state,400,1,10.1,.25,1.5));
 CHECK(!BotCombatPause(&state,400,1,10.3,.25,1.5));
 CHECK(BotCombatPause(&state,400,1,12,.25,1.5));
 CHECK(!BotCombatPause(&state,400,0,12.1,.25,1.5));CHECK(state.combat_pause_until==0);
 setup();state.cur_ps.groundEntityNum=ENTITYNUM_NONE;CHECK(!BotCombatPause(&state,400,1,10,.25,1.5));
 setup();state.flags=BFL_RELOAD;CHECK(!BotCombatPause(&state,400,1,10,.25,1.5));
 setup();state.cur_ps.torsoAnim=BOTH_LADDER;CHECK(!BotCombatPause(&state,400,1,10,.25,1.5));
 setup();state.weaponnum=WP_AKIMBO;CHECK(BotCombatPause(&state,400,1,10,.25,1.5));
 setup();state.cur_ps.weapon=state.weaponnum=WP_AKIMBO;CHECK(BotCombatPause(&state,400,1,10,.25,1.5));
 setup();CHECK(BotCombatPause(&state,400,1,10,5,0));CHECK(state.combat_pause_until==10.5f);
 CHECK(state.combat_pause_next>11.29f && state.combat_pause_next<11.31f);
 BotCombatWeaponFired(0,WP_REM58);CHECK(state.combat_shots==1);CHECK(state.combat_pause_until==0);
 BotCombatWeaponFired(0,WP_DYNAMITE);CHECK(state.combat_throws==1);CHECK(state.combat_throw_next==22);
 BotCombatWeaponFired(1,WP_DYNAMITE);CHECK(state.combat_throws==1);
 printf("PASS: %d bot combat policy checks\n",checks);return 0;
}
