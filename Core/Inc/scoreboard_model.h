#ifndef SCOREBOARD_MODEL_H
#define SCOREBOARD_MODEL_H

#include <stdint.h>

#define MAX_TEAM_PENALTIES 5

typedef enum {
    TEAM_HOME = 0,
    TEAM_AWAY = 1
} TeamId;

typedef enum {
    PAGE_MAIN = 0,
    PAGE_PENALTY_INPUT = 1
} ScoreboardPage;

typedef enum {
    PENALTY_MINOR = 0,
    PENALTY_MAJOR = 1
} PenaltyType;

typedef struct {
    uint8_t used;
    uint8_t running;
    PenaltyType type;
    uint16_t remainingSeconds;
} PenaltySlot;

typedef struct {
    uint8_t period;
    uint8_t homeScore;
    uint8_t awayScore;
    uint8_t homeTimeoutAvailable;
    uint8_t awayTimeoutAvailable;
    uint16_t gameSeconds;

    PenaltySlot penalties[2][MAX_TEAM_PENALTIES];

    ScoreboardPage page;
    TeamId penaltyPageTeam;
} ScoreboardState;

#endif
