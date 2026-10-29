# SmokinGuns v20 – ausführlich kommentierter Quellcode

Diese Fassung enthält den vollständigen `code`-Baum. Sie entspricht funktional
der kompilierten v20-QVM; hinzugefügt wurden ausschließlich Kommentare und
diese Dokumentation.

## Einstiegspunkte

| Thema | Datei | Wichtige Funktionen/Strukturen |
|---|---|---|
| Bank- und Wegpunktnavigation | `code/game/ai_dmnet.c` | `BotAI_FindNode`, `BotGetItemLongTermGoal` |
| Dynamit- und Feuerflucht | `code/game/ai_dmnet.c` | `BotClearPath`, `BotAI_FindNearestNode` |
| Verteidiger an der Bank | `code/game/ai_dmnet.c` | `AINode_Seek_NBG`, `brDefendWait` |
| Escape-Verfolgung | `code/game/ai_dmnet.c` | Geldträger-Block in `BotGetItemLongTermGoal` |
| Kauf, Kampf, Gegnerbindung | `code/game/ai_dmq3.c` | Waffenwahl, Enemy-Lock, BR-Kaufcode |
| Gatling-Taktik | `code/game/ai_dmq3.c` | `BotGatling...`-Funktionen |
| BR-Debugausgabe | `code/game/ai_main.c` | `BotBRDebugThink` |
| Bot-Zustandsfelder | `code/game/ai_main.h` | `bot_orient_t`, `bot_state_t` |
| Cvar-Definitionen | `code/game/g_main.c` | `vmCvar_t`-Deklarationen und Tabelle |

## Ablauf eines Angreifers

1. `BotGetItemLongTermGoal` ermittelt sichtbare Geldentitäten.
2. Der Teamplan weist Bank und Routenrolle zu.
3. `BotAI_SetMoneyRoute` setzt Bank, Start und Endpunkt.
4. `BotAI_FindNode` wählt den ersten lokalen Einstieg und danach taktische
   Teilwegpunkte.
5. Vor einer intakten Dynamittür erzeugt der Aktivierungsstack den Wurf.
6. `BotClearPath` erkennt den eigenen Sprengsatz und setzt `BFL_AVOID`.
7. Nach der Explosion wird die geöffnete Bank erneut als Geldziel gewählt.
8. Mit `PW_GOLD` wird `BFL_ESCAPE` aktiv; die Escape-Teilroute wird abgearbeitet.

## Ablauf eines Verteidigers

1. Ab Teamplan-Stufe 2 werden Lawmen auf Bankpositionen verteilt.
2. Am letzten Wartepunkt verhindert `brDefendWait` Bewegung und Waffenwechsel.
3. Die Blickrichtung zeigt vom Bank-Endpunkt weg in den Raum.
4. `BotFindEnemy` bleibt aktiv und darf jederzeit in den Kampf wechseln.
5. Hat ein Outlaw Gold, übernimmt der Lawman bei einem Bot-Träger dessen
   nächsten Escape-Wegpunkt. Bei Menschen wird die aktuelle Position verfolgt.

## Feuer

Ein explodierter Molotow besteht nicht mehr nur aus einem Missile. Die Karte
enthält danach `sprotz`- und `whiskey_pool`-Entitäten. Nur ein brennender Pool
hat `s.apos.trDelta[0] != 0`. Knoten im Radius von 450 Einheiten werden für die
laufende Route markiert; anschließend wird das alte direkte Ziel verworfen.

## Debugfelder

- `role`: `OUTLAW` oder `LAWMAN`
- `area`: aktuelle AAS-Area
- `goalArea`: Zielarea
- `gold`: Zustand von `PW_GOLD`
- `route`: 0 direkt, 1 links, 2 rechts, 3 Unterstützung
- `actEnt`/`actWpn`: Aktivierungsziel und dafür vorgesehene Waffe
- `MONEY`, `SEEK`, `ACTIVATE`, `AVOID`, `ESCAPE`: aktive Zustandsflags

## Build-Hinweis

Kommentare verändern die QVM nicht semantisch. Für einen reproduzierbaren
QVM-Build müssen dennoch dieselben Q3LCC/Q3ASM-Werkzeuge und Build-Defines wie
bei v20 verwendet werden.
