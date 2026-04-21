#include "scoreboard_ui.h"
#include "display.h"
#include <stdio.h>

static void seg_a(uint16_t x, uint16_t y, uint16_t s, uint16_t c) { ILI9341_FillRect(x + s,   y,       3 * s, s, c); }
static void seg_b(uint16_t x, uint16_t y, uint16_t s, uint16_t c) { ILI9341_FillRect(x + 4*s, y + s,   s, 3 * s, c); }
static void seg_c(uint16_t x, uint16_t y, uint16_t s, uint16_t c) { ILI9341_FillRect(x + 4*s, y + 5*s, s, 3 * s, c); }
static void seg_d(uint16_t x, uint16_t y, uint16_t s, uint16_t c) { ILI9341_FillRect(x + s,   y + 8*s, 3 * s, s, c); }
static void seg_e(uint16_t x, uint16_t y, uint16_t s, uint16_t c) { ILI9341_FillRect(x,       y + 5*s, s, 3 * s, c); }
static void seg_f(uint16_t x, uint16_t y, uint16_t s, uint16_t c) { ILI9341_FillRect(x,       y + s,   s, 3 * s, c); }
static void seg_g(uint16_t x, uint16_t y, uint16_t s, uint16_t c) { ILI9341_FillRect(x + s,   y + 4*s, 3 * s, s, c); }

static uint16_t lastMainClock = 0xFFFF;
static uint16_t lastMainPenaltySeconds[2][2] = {
    {0xFFFF, 0xFFFF},
    {0xFFFF, 0xFFFF}
};

static uint8_t lastPenaltyPageValid = 0;
static TeamId lastPenaltyPageTeam = TEAM_HOME;
static uint8_t lastPenaltyPageUsed[MAX_TEAM_PENALTIES] = {0};
static PenaltyType lastPenaltyPageType[MAX_TEAM_PENALTIES] = {0};
static uint16_t lastPenaltyPageSeconds[MAX_TEAM_PENALTIES] = {0};

static uint16_t lastTimeoutPageSeconds = 0xFFFF;
static uint8_t lastTimeoutPageActive = 0xFF;
static TeamId lastTimeoutPageTeam = TEAM_HOME;


static void draw_digit7(uint16_t x, uint16_t y, uint8_t digit, uint16_t scale, uint16_t color, uint16_t bg) {
    ILI9341_FillRect(x, y, 5 * scale, 9 * scale, bg);

    switch (digit) {
        case 0: seg_a(x,y,scale,color); seg_b(x,y,scale,color); seg_c(x,y,scale,color); seg_d(x,y,scale,color); seg_e(x,y,scale,color); seg_f(x,y,scale,color); break;
        case 1: seg_b(x,y,scale,color); seg_c(x,y,scale,color); break;
        case 2: seg_a(x,y,scale,color); seg_b(x,y,scale,color); seg_g(x,y,scale,color); seg_e(x,y,scale,color); seg_d(x,y,scale,color); break;
        case 3: seg_a(x,y,scale,color); seg_b(x,y,scale,color); seg_g(x,y,scale,color); seg_c(x,y,scale,color); seg_d(x,y,scale,color); break;
        case 4: seg_f(x,y,scale,color); seg_g(x,y,scale,color); seg_b(x,y,scale,color); seg_c(x,y,scale,color); break;
        case 5: seg_a(x,y,scale,color); seg_f(x,y,scale,color); seg_g(x,y,scale,color); seg_c(x,y,scale,color); seg_d(x,y,scale,color); break;
        case 6: seg_a(x,y,scale,color); seg_f(x,y,scale,color); seg_g(x,y,scale,color); seg_e(x,y,scale,color); seg_c(x,y,scale,color); seg_d(x,y,scale,color); break;
        case 7: seg_a(x,y,scale,color); seg_b(x,y,scale,color); seg_c(x,y,scale,color); break;
        case 8: seg_a(x,y,scale,color); seg_b(x,y,scale,color); seg_c(x,y,scale,color); seg_d(x,y,scale,color); seg_e(x,y,scale,color); seg_f(x,y,scale,color); seg_g(x,y,scale,color); break;
        case 9: seg_a(x,y,scale,color); seg_b(x,y,scale,color); seg_c(x,y,scale,color); seg_d(x,y,scale,color); seg_f(x,y,scale,color); seg_g(x,y,scale,color); break;
        default: break;
    }
}

static void draw_colon7(uint16_t x, uint16_t y, uint16_t scale, uint16_t color, uint16_t bg) {
    ILI9341_FillRect(x, y, scale, 9 * scale, bg);
    ILI9341_FillRect(x, y + 2 * scale, scale, scale, color);
    ILI9341_FillRect(x, y + 6 * scale, scale, scale, color);
}

static void draw_2digit(uint16_t x, uint16_t y, uint8_t value, uint16_t scale, uint16_t color, uint16_t bg) {
    draw_digit7(x,           y, (value / 10) % 10, scale, color, bg);
    draw_digit7(x + 6*scale, y, value % 10,        scale, color, bg);
}

static void draw_mmss(uint16_t x, uint16_t y, uint16_t totalSeconds, uint16_t scale, uint16_t color, uint16_t bg) {
    uint8_t mm = (uint8_t)(totalSeconds / 60);
    uint8_t ss = (uint8_t)(totalSeconds % 60);

    draw_digit7(x + 0 * 6 * scale, y, (mm / 10) % 10, scale, color, bg);
    draw_digit7(x + 1 * 6 * scale, y, mm % 10,        scale, color, bg);
    draw_colon7(x + 2 * 6 * scale - scale, y, scale, color, bg);
    draw_digit7(x + 2 * 6 * scale, y, (ss / 10) % 10, scale, color, bg);
    draw_digit7(x + 3 * 6 * scale, y, ss % 10,        scale, color, bg);
}

static void draw_mmss_changed(uint16_t x, uint16_t y,
                              uint16_t oldSeconds, uint16_t newSeconds,
                              uint16_t scale, uint16_t color, uint16_t bg)
{
    uint8_t old_mm = (uint8_t)(oldSeconds / 60);
    uint8_t old_ss = (uint8_t)(oldSeconds % 60);
    uint8_t new_mm = (uint8_t)(newSeconds / 60);
    uint8_t new_ss = (uint8_t)(newSeconds % 60);

    uint8_t old_digits[4] = {
        (uint8_t)((old_mm / 10) % 10),
        (uint8_t)(old_mm % 10),
        (uint8_t)((old_ss / 10) % 10),
        (uint8_t)(old_ss % 10)
    };

    uint8_t new_digits[4] = {
        (uint8_t)((new_mm / 10) % 10),
        (uint8_t)(new_mm % 10),
        (uint8_t)((new_ss / 10) % 10),
        (uint8_t)(new_ss % 10)
    };

    if (oldSeconds == 0xFFFF) {
        draw_colon7(x + 2 * 6 * scale - scale, y, scale, color, bg);
        draw_digit7(x + 0 * 6 * scale, y, new_digits[0], scale, color, bg);
        draw_digit7(x + 1 * 6 * scale, y, new_digits[1], scale, color, bg);
        draw_digit7(x + 2 * 6 * scale, y, new_digits[2], scale, color, bg);
        draw_digit7(x + 3 * 6 * scale, y, new_digits[3], scale, color, bg);
        return;
    }

    if (old_digits[0] != new_digits[0]) draw_digit7(x + 0 * 6 * scale, y, new_digits[0], scale, color, bg);
    if (old_digits[1] != new_digits[1]) draw_digit7(x + 1 * 6 * scale, y, new_digits[1], scale, color, bg);
    if (old_digits[2] != new_digits[2]) draw_digit7(x + 2 * 6 * scale, y, new_digits[2], scale, color, bg);
    if (old_digits[3] != new_digits[3]) draw_digit7(x + 3 * 6 * scale, y, new_digits[3], scale, color, bg);
}

static const char *penalty_type_text(PenaltyType t)
{
    switch (t) {
        case PENALTY_MINOR: return "MIN";
        case PENALTY_MAJOR: return "MAJ";
        default:            return "---";
    }
}

static uint16_t team_color(TeamId team)
{
    return (team == TEAM_HOME) ? TFT_GREEN : TFT_RED;
}

static const PenaltySlot *team_penalties_const(const ScoreboardState *s, TeamId team)
{
    return s->penalties[(int)team];
}

static void get_two_lowest_penalties(const ScoreboardState *s, TeamId team,
                                     uint16_t *first, uint16_t *second)
{
    const PenaltySlot *p = team_penalties_const(s, team);
    int firstIdx = -1;
    int secondIdx = -1;

    for (int i = 0; i < MAX_TEAM_PENALTIES; i++) {
        if (!p[i].used) continue;

        if (firstIdx < 0 || p[i].remainingSeconds < p[firstIdx].remainingSeconds) {
            secondIdx = firstIdx;
            firstIdx = i;
        } else if (secondIdx < 0 || p[i].remainingSeconds < p[secondIdx].remainingSeconds) {
            secondIdx = i;
        }
    }

    *first  = (firstIdx  >= 0) ? p[firstIdx].remainingSeconds  : 0;
    *second = (secondIdx >= 0) ? p[secondIdx].remainingSeconds : 0;
}

static void draw_penalty_row_counter(uint16_t y, uint16_t oldSeconds, uint16_t newSeconds)
{
    draw_mmss_changed(88, y + 3, oldSeconds, newSeconds, 1, TFT_CYAN, TFT_BLACK);
}

static void draw_penalty_row(uint8_t slotIndex, const PenaltySlot *p, TeamId team, uint16_t y)
{
    uint16_t accent = team_color(team);
    char buf[8];

    ILI9341_FillRect(8, y, 304, 16, TFT_BLACK);
    ILI9341_DrawRect(8, y, 304, 16, accent);

    snprintf(buf, sizeof(buf), "#%u", (unsigned)(slotIndex + 1));
    ILI9341_DrawText(12, y + 12, buf, &mono9x7, TFT_YELLOW);

    if (!p->used) {
        ILI9341_DrawText(42, y + 12, "EMPTY", &mono9x7, TFT_GRAY);
    } else {
        ILI9341_DrawText(42, y + 12, penalty_type_text(p->type), &mono9x7, TFT_WHITE);
        draw_mmss(88, y + 3, p->remainingSeconds, 1, TFT_CYAN, TFT_BLACK);

        ILI9341_DrawRect(240, y, 52, 16, TFT_RED);
        ILI9341_DrawText(254, y + 12, "DEL", &mono9x7, TFT_WHITE);
    }
}

static void draw_penalty_box_value(uint16_t x, uint16_t y,
                                   uint16_t oldSeconds, uint16_t newSeconds,
                                   uint16_t borderColor, uint16_t digitColor)
{
    if (oldSeconds == newSeconds) return;

    if (newSeconds == 0) {
        ILI9341_FillRect(x + 6, y + 4, 88, 18, TFT_BLACK);
        ILI9341_DrawFastHLine(x + 10, y + 12, 80, borderColor);
        return;
    }

    if (oldSeconds == 0 || oldSeconds == 0xFFFF) {
        ILI9341_FillRect(x + 6, y + 4, 88, 18, TFT_BLACK);
        draw_mmss(x + 6, y + 4, newSeconds, 2, digitColor, TFT_BLACK);
        return;
    }

    draw_mmss_changed(x + 6, y + 4, oldSeconds, newSeconds, 2, digitColor, TFT_BLACK);
}

static void draw_penalty_box(uint16_t x, uint16_t y, uint16_t seconds, uint16_t borderColor, uint16_t digitColor)
{
    ILI9341_DrawRect(x, y, 100, 24, borderColor);
    draw_penalty_box_value(x, y, 0xFFFF, seconds, borderColor, digitColor);
}

static void draw_timeout_indicator(uint16_t x, uint16_t y, uint8_t available, uint16_t onColor, uint16_t offColor) {
    uint16_t c = available ? onColor : offColor;
    ILI9341_FillRect(x, y, 18, 18, c);
    ILI9341_DrawRect(x, y, 18, 18, TFT_WHITE);
}

static void draw_timeout_page_button(uint16_t x, uint16_t y, const char *teamName,
                                     uint8_t available, uint16_t accent)
{
    uint16_t border = available ? accent : TFT_GRAY;

    ILI9341_FillRect(x, y, 126, 58, TFT_BLACK);
    ILI9341_DrawRect(x, y, 126, 58, border);

    ILI9341_DrawText(x + 28, y + 18, teamName, &mono12x7, border);

    if (available) {
        ILI9341_DrawText(x + 3, y + 42, "USE TIMEOUT", &mono9x7, TFT_WHITE);
    } else {
        ILI9341_DrawText(x + 10, y + 42, "NO TIMEOUT", &mono9x7, TFT_GRAY);
    }
}

static void draw_timeout_page_status(uint16_t timeoutSeconds, uint8_t active, TeamId timeoutTeam)
{
    ILI9341_FillRect(55, 150, 210, 14, TFT_BLACK);

    if (active) {
        if (timeoutTeam == TEAM_HOME) {
            ILI9341_DrawText(90, 162, "HOME TIMEOUT", &mono9x7, TFT_GREEN);
        } else {
            ILI9341_DrawText(90, 162, "AWAY TIMEOUT", &mono9x7, TFT_RED);
        }
    } else if (timeoutSeconds == 0) {
        ILI9341_DrawText(103, 162, "TIMEOUT OVER", &mono9x7, TFT_YELLOW);
    } else {
        ILI9341_DrawText(83, 162, "SELECT TIMEOUT", &mono9x7, TFT_YELLOW);
    }
}

void Scoreboard_UpdateHomeScore(const ScoreboardState *s) {
    ILI9341_FillRect(19, 79, 82, 70, TFT_BLACK);
    ILI9341_DrawRect(18, 78, 84, 72, TFT_GREEN);
    draw_2digit(28, 90, s->homeScore, 5, TFT_GREEN, TFT_BLACK);
}

void Scoreboard_UpdateAwayScore(const ScoreboardState *s) {
    ILI9341_FillRect(219, 79, 82, 70, TFT_BLACK);
    ILI9341_DrawRect(218, 78, 84, 72, TFT_RED);
    draw_2digit(228, 90, s->awayScore, 5, TFT_RED, TFT_BLACK);
}

void Scoreboard_UpdateMainClock(const ScoreboardState *s) {
    draw_mmss_changed(101, 17, lastMainClock, s->gameSeconds, 5, TFT_YELLOW, TFT_BLACK);
    lastMainClock = s->gameSeconds;
}

void Scoreboard_UpdatePenalties(const ScoreboardState *s)
{
    uint16_t homeTop, homeBottom, awayTop, awayBottom;

    get_two_lowest_penalties(s, TEAM_HOME, &homeTop, &homeBottom);
    get_two_lowest_penalties(s, TEAM_AWAY, &awayTop, &awayBottom);

    draw_penalty_box_value(26, 178, lastMainPenaltySeconds[(int)TEAM_HOME][0], homeTop,    TFT_GREEN, TFT_WHITE);
    draw_penalty_box_value(26, 204, lastMainPenaltySeconds[(int)TEAM_HOME][1], homeBottom, TFT_GREEN, TFT_WHITE);
    draw_penalty_box_value(192, 178, lastMainPenaltySeconds[(int)TEAM_AWAY][0], awayTop,    TFT_RED,   TFT_WHITE);
    draw_penalty_box_value(192, 204, lastMainPenaltySeconds[(int)TEAM_AWAY][1], awayBottom, TFT_RED,   TFT_WHITE);

    lastMainPenaltySeconds[(int)TEAM_HOME][0] = homeTop;
    lastMainPenaltySeconds[(int)TEAM_HOME][1] = homeBottom;
    lastMainPenaltySeconds[(int)TEAM_AWAY][0] = awayTop;
    lastMainPenaltySeconds[(int)TEAM_AWAY][1] = awayBottom;
}

void Scoreboard_DrawTimeoutPage(const ScoreboardState *s,
                                uint16_t timeoutSeconds,
                                uint8_t active,
                                TeamId timeoutTeam)
{
    uint16_t timerColor = active ? TFT_YELLOW : TFT_GRAY;

    ILI9341_FillScreen(TFT_BLACK);
    ILI9341_DrawRect(4, 4, 312, 232, TFT_CYAN);

    /* Same clock position as the main page. This value is intentionally not updated
       while the timeout page is open, so it stays paused. */
    ILI9341_DrawRect(98, 10, 126, 60, TFT_CYAN);
    draw_mmss(101, 17, s->gameSeconds, 5, TFT_YELLOW, TFT_BLACK);

    ILI9341_DrawText(55, 84, "TOUCH CLOCK TO RETURN", &mono9x7, TFT_WHITE);

    draw_timeout_page_button(18, 92, "HOME", s->homeTimeoutAvailable ? 1U : 0U, TFT_GREEN);
    draw_timeout_page_button(176, 92, "AWAY", s->awayTimeoutAvailable ? 1U : 0U, TFT_RED);

    draw_timeout_page_status(timeoutSeconds, active, timeoutTeam);
    draw_mmss(114, 170, timeoutSeconds, 4, timerColor, TFT_BLACK);

    lastTimeoutPageSeconds = timeoutSeconds;
    lastTimeoutPageActive = active;
    lastTimeoutPageTeam = timeoutTeam;
}

void Scoreboard_UpdateTimeoutPage(const ScoreboardState *s,
                                  uint16_t timeoutSeconds,
                                  uint8_t active,
                                  TeamId timeoutTeam)
{
    uint16_t timerColor = active ? TFT_YELLOW : TFT_GRAY;
    (void)s;

    if (active != lastTimeoutPageActive || timeoutTeam != lastTimeoutPageTeam) {
        draw_timeout_page_status(timeoutSeconds, active, timeoutTeam);
        ILI9341_FillRect(110, 166, 102, 42, TFT_BLACK);
        draw_mmss(114, 170, timeoutSeconds, 4, timerColor, TFT_BLACK);
    } else if (timeoutSeconds != lastTimeoutPageSeconds) {
        draw_mmss_changed(114, 170, lastTimeoutPageSeconds, timeoutSeconds, 4, timerColor, TFT_BLACK);
    }

    lastTimeoutPageSeconds = timeoutSeconds;
    lastTimeoutPageActive = active;
    lastTimeoutPageTeam = timeoutTeam;
}


void Scoreboard_DrawOvertimeChoicePage(const ScoreboardState *s)
{
    char buf[20];

    ILI9341_FillScreen(TFT_BLACK);
    ILI9341_DrawRect(4, 4, 312, 232, TFT_CYAN);

    ILI9341_DrawText(83, 30, "GAME TIED", &mono12x7, TFT_YELLOW);
    ILI9341_DrawText(42, 55, "SELECT OVERTIME TYPE", &mono9x7, TFT_WHITE);

    ILI9341_DrawRect(28, 84, 264, 50, TFT_GREEN);
    ILI9341_DrawText(65, 105, "REGULAR SEASON", &mono12x7, TFT_GREEN);
    ILI9341_DrawText(92, 127, "5:00 SUDDEN DEATH", &mono9x7, TFT_WHITE);

    ILI9341_DrawRect(28, 152, 264, 50, TFT_RED);
    ILI9341_DrawText(105, 173, "PLAYOFFS", &mono12x7, TFT_RED);
    ILI9341_DrawText(83, 195, "15:00 REPEATING OT", &mono9x7, TFT_WHITE);

    snprintf(buf, sizeof(buf), "PERIOD %u", (unsigned)s->period);
    ILI9341_DrawText(124, 222, buf, &mono9x7, TFT_CYAN);
}

void Scoreboard_DrawMainPage(const ScoreboardState *s) {
    uint16_t homeTop, homeBottom, awayTop, awayBottom;

    ILI9341_FillScreen(TFT_BLACK);

    ILI9341_DrawRect(4, 4, 312, 232, TFT_CYAN);

    ILI9341_DrawRect(98, 10, 126, 60, TFT_CYAN);
    draw_mmss(101, 17, s->gameSeconds, 5, TFT_YELLOW, TFT_BLACK);

    ILI9341_DrawText(30, 65, "HOME", &mono12x7, TFT_YELLOW);
    ILI9341_DrawText(240, 65, "AWAY", &mono12x7, TFT_YELLOW);

    ILI9341_DrawRect(18, 78, 84, 72, TFT_GREEN);
    ILI9341_DrawRect(218, 78, 84, 72, TFT_RED);
    draw_2digit(28, 90, s->homeScore, 5, TFT_GREEN, TFT_BLACK);
    draw_2digit(228, 90, s->awayScore, 5, TFT_RED, TFT_BLACK);

    ILI9341_DrawText(108, 108, "PERIOD", &mono9x7, TFT_YELLOW);
    draw_digit7(184, 94, s->period % 10, 2, TFT_CYAN, TFT_BLACK);

    draw_timeout_indicator(110, 128, s->homeTimeoutAvailable ? 1U : 0U, TFT_GREEN, TFT_GRAY);
    draw_timeout_indicator(192, 128, s->awayTimeoutAvailable ? 1U : 0U, TFT_RED, TFT_GRAY);

    ILI9341_DrawText(32, 166, "PENALTY", &mono9x7, TFT_YELLOW);
    ILI9341_DrawText(198, 166, "PENALTY", &mono9x7, TFT_YELLOW);

    /* touch these to enter penalty page */
    ILI9341_DrawRect(14, 172, 126, 58, TFT_GREEN);
    ILI9341_DrawRect(180, 172, 126, 58, TFT_RED);

    get_two_lowest_penalties(s, TEAM_HOME, &homeTop, &homeBottom);
    get_two_lowest_penalties(s, TEAM_AWAY, &awayTop, &awayBottom);

    draw_penalty_box(26, 178, homeTop,    TFT_GREEN, TFT_WHITE);
    draw_penalty_box(26, 204, homeBottom, TFT_GREEN, TFT_WHITE);

    draw_penalty_box(192, 178, awayTop,    TFT_RED, TFT_WHITE);
    draw_penalty_box(192, 204, awayBottom, TFT_RED, TFT_WHITE);

    lastMainClock = s->gameSeconds;
    lastMainPenaltySeconds[(int)TEAM_HOME][0] = homeTop;
    lastMainPenaltySeconds[(int)TEAM_HOME][1] = homeBottom;
    lastMainPenaltySeconds[(int)TEAM_AWAY][0] = awayTop;
    lastMainPenaltySeconds[(int)TEAM_AWAY][1] = awayBottom;
}

void Scoreboard_DrawPenaltyPage(const ScoreboardState *s)
{
    TeamId team = s->penaltyPageTeam;
    const PenaltySlot *p = team_penalties_const(s, team);
    uint16_t accent = team_color(team);
    uint16_t y = 40;

    ILI9341_FillScreen(TFT_BLACK);
    ILI9341_DrawRect(4, 4, 312, 232, accent);

    ILI9341_DrawRect(8, 8, 52, 16, TFT_CYAN);
    ILI9341_DrawText(15, 20, "BACK", &mono9x7, TFT_WHITE);

    if (team == TEAM_HOME) {
        ILI9341_DrawText(92, 20, "HOME PENALTIES", &mono12x7, TFT_GREEN);
    } else {
        ILI9341_DrawText(92, 20, "AWAY PENALTIES", &mono12x7, TFT_RED);
    }

    for (int i = 0; i < MAX_TEAM_PENALTIES; i++) {
        draw_penalty_row((uint8_t)i, &p[i], team, y);
        lastPenaltyPageUsed[i] = p[i].used ? 1U : 0U;
        lastPenaltyPageType[i] = p[i].type;
        lastPenaltyPageSeconds[i] = p[i].remainingSeconds;
        y += 24;
    }
    lastPenaltyPageTeam = team;
    lastPenaltyPageValid = 1;

    ILI9341_DrawRect(32, 200, 115, 20, TFT_GREEN);
    ILI9341_DrawRect(178, 200, 115, 20, TFT_YELLOW);

    ILI9341_DrawText(35, 214, "+ MIN 2:00", &mono9x7, TFT_WHITE);
    ILI9341_DrawText(181, 214, "+ MAJ 5:00", &mono9x7, TFT_WHITE);
}

void Scoreboard_UpdatePenaltyPageTimers(const ScoreboardState *s)
{
    TeamId team = s->penaltyPageTeam;
    const PenaltySlot *p = team_penalties_const(s, team);
    uint16_t y = 40;

    if (!lastPenaltyPageValid || lastPenaltyPageTeam != team) {
        Scoreboard_DrawPenaltyPage(s);
        return;
    }

    for (int i = 0; i < MAX_TEAM_PENALTIES; i++) {
        uint8_t used = p[i].used ? 1U : 0U;

        if (used != lastPenaltyPageUsed[i] || p[i].type != lastPenaltyPageType[i]) {
            draw_penalty_row((uint8_t)i, &p[i], team, y);
        } else if (used && p[i].remainingSeconds != lastPenaltyPageSeconds[i]) {
            draw_penalty_row_counter(y, lastPenaltyPageSeconds[i], p[i].remainingSeconds);
        }

        lastPenaltyPageUsed[i] = used;
        lastPenaltyPageType[i] = p[i].type;
        lastPenaltyPageSeconds[i] = p[i].remainingSeconds;
        y += 24;
    }
}
