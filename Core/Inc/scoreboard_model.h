#ifndef SCOREBOARD_MODEL_H
#define SCOREBOARD_MODEL_H

// Includes
#include <stdint.h>

// Defines
#define MAX_TEAM_PENALTIES 5

// Enums
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

// Structs
typedef struct {
	uint8_t used;           // 1 if this slot is occupied, 0 if empty
	uint8_t running;        // 1 if the penalty clock is counting down
	PenaltyType type;       // minor or major
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
