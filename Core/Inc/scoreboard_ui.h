#ifndef SCOREBOARD_UI_H
#define SCOREBOARD_UI_H

#include "scoreboard_model.h"
#include "fonts.h"

void Scoreboard_DrawMainPage(const ScoreboardState *s);
void Scoreboard_UpdateHomeScore(const ScoreboardState *s);
void Scoreboard_UpdateAwayScore(const ScoreboardState *s);
void Scoreboard_UpdateMainClock(const ScoreboardState *s);
void Scoreboard_UpdatePenalties(const ScoreboardState *s);
void Scoreboard_DrawPenaltyPage(const ScoreboardState *s);
void Scoreboard_UpdatePenaltyPageTimers(const ScoreboardState *s);

#endif
