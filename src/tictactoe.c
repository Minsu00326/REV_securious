#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>
#include <stdint.h>

#ifdef _WIN32
#include <conio.h>
#include <windows.h>
#else
#include <unistd.h>
#endif

#ifndef WAIT_ON_EXIT
#define WAIT_ON_EXIT 1
#endif

// ======= 설정값 =======
// 개발 단계에서 RH를 찍고 싶으면 빌드 옵션으로 -DDEV_HELP=1
#ifndef DEV_HELP
#define DEV_HELP 0
#endif

// 해시 검증은 항상 켬 (조건을 맞춰야만 복호화 키가 맞음)
#ifndef ENABLE_HASH_CHECK
#define ENABLE_HASH_CHECK 1
#endif

// ======= 배포용 상수 (교체 완료) =======
static const uint32_t SEED = 0x5EEDC0DE;
static const uint16_t TARGET16 = 0x88EB;
static const uint8_t  ENC_BYTES[] = {
    82,196,124,148,215,31,39,83,50,218,234,57,56,164,123,194,203,75,12,72,27,199
};

// ======= 회전/PRNG =======
static inline uint32_t rotl32(uint32_t x, int r) { return (x << r) | (x >> (32 - r)); }
static inline uint16_t rotl16(uint16_t x, int r) { r &= 15; return (uint16_t)((x << r) | (x >> (16 - r))); }

static uint32_t xs32(uint32_t x) {
    x ^= x << 13; x ^= x >> 17; x ^= x << 5;
    return x;
}
static uint32_t key_from_rh(uint32_t rh) {
    return (rh ^ (SEED * 0x9E3779B1u) ^ 0x85EBCA6Bu);
}

// ======= 상태 =======
typedef struct {
    int board[9];     // 0=empty, 1=X, 2=O
    int who;          // 1=X, 2=O
    int turn;         // 0..9
    int used_r_first; // 첫 입력이 r

    int weights[9];   // STRICT에서만 사용
    int sumX, sumO;

#if ENABLE_HASH_CHECK
    uint32_t rh;      // 롤링 해시
    int action_idx;   // 액션 번호
#endif
} state_t;

// ======= 승리 조건 =======
static const int WIN[8][3] = {
  {0,1,2},{3,4,5},{6,7,8},
  {0,3,6},{1,4,7},{2,5,8},
  {0,4,8},{2,4,6}
};

// ======= 유틸 선언 =======
static int  check_win(const int b[9]);
static int  read_token(void);
static void prompt_input(void);

// ======= 박스 UI =======
static void print_box(const char* lines[], int count) {
    int width = 0;
    for (int i = 0; i < count; i++) {
        int len = (int)strlen(lines[i]);
        if (len > width) width = len;
    }
    putchar('+'); for (int i = 0; i < width + 2; i++) putchar('-'); puts("+");
    for (int i = 0; i < count; i++) {
        int pad = width - (int)strlen(lines[i]);
        printf("| %s", lines[i]);
        for (int k = 0; k < pad; k++) putchar(' ');
        puts(" |");
    }
    putchar('+'); for (int i = 0; i < width + 2; i++) putchar('-'); puts("+");
}

static void show_end_box(const state_t* S, const char* status_line, int res, const char* finale) {
    char line_turn[64];
    snprintf(line_turn, sizeof(line_turn), "Turn: %d  Next: %c", S->turn, (S->who == 1 ? 'X' : 'O'));

    const char* lines[4];
    int n = 0;
    lines[n++] = line_turn;
    lines[n++] = status_line;

    if (res == 1) {
        lines[n++] = "OK";
        lines[n++] = finale;
    }
    else {
        lines[n++] = "NO";
    }
    print_box(lines, n);
}

// ======= STRICT 시각효과 =======
static void sleep_ms(int ms) {
#ifdef _WIN32
    Sleep(ms);
#else
    usleep(ms * 1000);
#endif
}
static void enable_ansi_colors(void) {
#ifdef _WIN32
    HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
    if (h == INVALID_HANDLE_VALUE) return;
    DWORD mode = 0;
    if (!GetConsoleMode(h, &mode)) return;
    mode |= 0x0004;
    SetConsoleMode(h, mode);
#endif
}
static void clear_screen(void) { printf("\x1b[2J\x1b[H"); fflush(stdout); }
static void strict_intro(void) {
    enable_ansi_colors(); clear_screen();
#ifdef _WIN32
    Beep(880, 90); sleep_ms(60); Beep(660, 90);
#endif
    printf("\x1b[1;31m"); puts("########################################");
    puts("#            STRICT  MODE              #");
    puts("########################################"); printf("\x1b[0m");
    printf("\n\x1b[1mArming checks\x1b[0m "); for (int i = 0; i < 14; i++) { printf("."); fflush(stdout); sleep_ms(60); }
    printf(" done\n"); printf("\x1b[32mREAL Tic-Tac_toe mode activated.\x1b[0m\n\n");
}

// ======= RH 업데이트 =======
#if ENABLE_HASH_CHECK
static void rh_update(state_t* S, uint32_t op, uint32_t val) {
    // op: 1=place, 2=first 'r'
    uint32_t x = S->rh;
    x ^= (op * 0x9E3779B1u) + val + ((uint32_t)(S->action_idx & 0xFFFF) << 16) + 0x7F4A7C15u;
    x = rotl32(x, (S->action_idx % 7) + 3) + 0x85EBCA6Bu;
    S->rh = x;
    S->action_idx++;
}
#endif

// ======= 입력/공용 =======
static inline void puts_NO(void) { puts("NO"); fflush(stdout); }
static inline void puts_OK(void) { puts("OK"); fflush(stdout); }
static void hold_console(void) {
#if WAIT_ON_EXIT
#ifdef _WIN32
    printf("\nPress any key to exit..."); fflush(stdout); _getch();
#else
    printf("\nPress Enter to exit..."); fflush(stdout); int c; while ((c = getchar()) != '\n' && c != EOF) {}
#endif
#endif
}
static void prompt_input(void) { printf("input: "); fflush(stdout); }
static int read_token(void) {
    int ch; do { ch = getchar(); if (ch == EOF) return EOF; } while (isspace(ch));
    return ch;
}

// ======= 게임 유틸 =======
static int check_win(const int b[9]) {
    for (int i = 0; i < 8; i++) {
        int a = WIN[i][0], c = WIN[i][1], d = WIN[i][2];
        if (b[a] && b[a] == b[c] && b[c] == b[d]) return b[a];
    }
    return 0;
}
static char cell_char(int v, int pos) { if (v == 1) return 'X'; if (v == 2) return 'O'; return (char)('0' + pos + 1); }
static void draw_board(const state_t* S) {
    printf("\n");
    for (int r = 0; r < 3; r++) {
        for (int c = 0; c < 3; c++) {
            int i = r * 3 + c; printf(" %c ", cell_char(S->board[i], i));
            if (c < 2) printf("|");
        }
        printf("\n"); if (r < 2) printf("---+---+---\n");
    }
    printf("Turn: %d  Next: %c\n", S->turn, (S->who == 1 ? 'X' : 'O'));
}

// ======= STRICT/Normal 훅 =======
static bool handle_first_r(state_t* S) {
    static const int W[9] = { 41,8,2,48,18,15,9,16,57 };
    memcpy(S->weights, W, sizeof(W));
    S->sumX = S->sumO = 0;
#if ENABLE_HASH_CHECK
    S->rh = 0xC0FFEE42u; S->action_idx = 0;
    rh_update(S, 2u, 0u); // 첫 r
#endif
    return true;
}
static bool check_before_move(state_t* S, int idx) {
    (void)S; (void)idx; return true; // 필요 시 제약 추가
}
static void handle_after_move(state_t* S, int idx) {
    if (!S->used_r_first) return; // Normal 모드면 스킵
    int w = S->weights[idx];
    if (S->who == 1) S->sumX += w; else S->sumO += w;
#if ENABLE_HASH_CHECK
    uint32_t val = (uint32_t)((idx + 1) & 0xF) | ((uint32_t)(S->who & 3) << 4);
    rh_update(S, 1u, val);
#endif
}

// ======= 최종 판정 (STRICT) =======
static void decode_flag_with_key(uint32_t key, char* out, size_t n) {
    size_t L = sizeof(ENC_BYTES) / sizeof(ENC_BYTES[0]);
    if (n < L + 1) { if (n > 0) out[0] = '\0'; return; }
    uint32_t s = key | 1u;
    for (size_t i = 0; i < L; i++) {
        s = xs32(s);
        out[i] = (char)(ENC_BYTES[i] ^ (uint8_t)(s & 0xFF));
    }
    out[L] = '\0';
}
static int final_judgement_STRICT(const state_t* S, char* out, size_t n) {
    if (check_win(S->board) != 0) { if (n) snprintf(out, n, "NO"); return 0; }
    if (S->turn != 9) { if (n) snprintf(out, n, "NO"); return 0; }
    if (S->sumX != S->sumO) { if (n) snprintf(out, n, "NO"); return 0; }

#if ENABLE_HASH_CHECK
    uint16_t chk = (uint16_t)((S->rh ^ (SEED >> 8) ^ rotl16((uint16_t)SEED, 3)) & 0xFFFFu);

#if DEV_HELP
    // 개발용 출력: 의도한 수순으로 플레이한 뒤 RH/TARGET 확인
    printf("\n[DEV] RH_FINAL=0x%08X  TARGET16=0x%04X\n", S->rh, chk);
    if (n) snprintf(out, n, "NO");
    return 0; // DEV에서 멈춤
#else
    if (chk != TARGET16) { if (n) snprintf(out, n, "NO"); return 0; }
#endif
#endif

    if (n) {
        uint32_t key = key_from_rh(S->rh);
        decode_flag_with_key(key, out, n);
    }
    return 1;
}

// ======= 노말 모드 보조 =======
static int confirm_move_prompt(int num) {
    while (1) {
        printf("Place at %d ? (Y/N): ", num); fflush(stdout);
        int k = read_token();
        if (k == EOF) return -1;
        if (k == 'Y' || k == 'y') return 1;
        if (k == 'N' || k == 'n') return 0;
        printf("Please answer Y or N.\n");
    }
}

// ======= 노말 모드 =======
static void play_normal(state_t* S, int first) {
    int ch = first;
    while (1) {
        if (ch == EOF) { printf("Input ended.\n"); hold_console(); return; }
        if (ch == 'r' || ch == 'R') { printf("Invalid input in normal mode.\n"); hold_console(); return; }
        if (ch < '1' || ch>'9') { printf("Invalid input.\n"); hold_console(); return; }

        int idx = ch - '1';
        if (S->board[idx] != 0) { printf("Cell occupied. Game over.\n"); hold_console(); return; }

        int ok = confirm_move_prompt(idx + 1);
        if (ok == -1) { printf("Input ended.\n"); hold_console(); return; }
        if (ok == 0) { prompt_input(); ch = read_token(); continue; }

        if (!check_before_move(S, idx)) { printf("Invalid move.\n"); hold_console(); return; }
        S->board[idx] = S->who; // normal: sum/hash 미사용

        int win = check_win(S->board); draw_board(S);
        if (win) { printf(win == 1 ? "X wins!\n" : "O wins!\n"); hold_console(); return; }

        S->turn++;
        if (S->turn == 9) { printf("Draw!\n"); hold_console(); return; }
        S->who = (S->who == 1) ? 2 : 1;

        prompt_input(); ch = read_token();
    }
}

// ======= STRICT 모드 =======
static void play_strict(state_t* S) {
    S->used_r_first = 1;
    if (!handle_first_r(S)) { puts_NO(); hold_console(); return; }
    strict_intro(); draw_board(S);

    while (1) {
        prompt_input();
        int ch = read_token();
        if (ch == EOF) { printf("Input ended.\n"); hold_console(); return; }
        if (ch == 'r' || ch == 'R') { printf("Already used r.\n"); hold_console(); return; }
        if (ch < '1' || ch>'9') { printf("Invalid input.\n"); hold_console(); return; }

        int idx = ch - '1';
        if (S->board[idx] != 0) { printf("Cell occupied. Game over.\n"); hold_console(); return; }
        if (!check_before_move(S, idx)) { printf("Invalid move.\n"); hold_console(); return; }

        S->board[idx] = S->who; handle_after_move(S, idx);

        int win = check_win(S->board); draw_board(S);
        if (win) {
            const char* status = (win == 1) ? "X wins!" : "O wins!";
            char finale[256]; int res = final_judgement_STRICT(S, finale, sizeof(finale));
            show_end_box(S, status, res, finale); hold_console(); return;
        }

        S->turn++;
        if (S->turn == 9) {
            const char* status = "Draw!";
            char finale[256]; int res = final_judgement_STRICT(S, finale, sizeof(finale));
            show_end_box(S, status, res, finale); hold_console(); return;
        }

        S->who = (S->who == 1) ? 2 : 1;
    }
}

// ======= main =======
int main(void) {
    state_t S; memset(&S, 0, sizeof(S)); S.who = 1;

    printf("Tic-Tac-Toe start!\n");
    draw_board(&S);

    prompt_input();
    int first = read_token();
    if (first == EOF) { puts_NO(); hold_console(); return 0; }

    if (first == 'r' || first == 'R') play_strict(&S);
    else if (first >= '1' && first <= '9') play_normal(&S, first);
    else { printf("Invalid input.\n"); hold_console(); }

    return 0;
}
