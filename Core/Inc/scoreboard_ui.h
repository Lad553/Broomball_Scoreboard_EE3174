#ifndef SCOREBOARD_UI_H
#define SCOREBOARD_UI_H

// Includes
#include "scoreboard_model.h"
#include "fonts.h"

// Function prototypes
void Scoreboard_DrawMainPage(const ScoreboardState *s);
void Scoreboard_UpdateHomeScore(const ScoreboardState *s);
void Scoreboard_UpdateAwayScore(const ScoreboardState *s);
void Scoreboard_UpdateMainClock(const ScoreboardState *s);
void Scoreboard_UpdatePenalties(const ScoreboardState *s);
void Scoreboard_DrawPenaltyPage(const ScoreboardState *s);
void Scoreboard_UpdatePenaltyPageTimers(const ScoreboardState *s);
void Scoreboard_DrawTimeoutPage(const ScoreboardState *s, uint16_t timeoutSeconds, uint8_t active, TeamId timeoutTeam);
void Scoreboard_UpdateTimeoutPage(const ScoreboardState *s, uint16_t timeoutSeconds, uint8_t active, TeamId timeoutTeam);
void Scoreboard_DrawOvertimeChoicePage(const ScoreboardState *s);

#endif
