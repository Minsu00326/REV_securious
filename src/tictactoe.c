#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>

#ifdef _WIN32
  #include <conio.h>
  #include <windows.h>
#else
  #include <unistd.h>
#endif

#ifndef WAIT_ON_EXIT
#define WAIT_ON_EXIT 1
#endif

// -------------------- State --------------------
typedef struct {
    int board[9];     // 0=empty, 1=X, 2=O
    int who;          // current turn (1=X, 2=O)
    int turn;         // number of moves
    int used_r_first; // whether first input was r

    int weights[9];   // weight of each cell
    int sumX, sumO;   // X/O weight sums
    int nextX, nextO; // ascending order check (unused for now)
} state_t;

// -------------------- Win condition --------------------
static const int WIN[8][3] = {
  {0,1,2},{3,4,5},{6,7,8},
  {0,3,6},{1,4,7},{2,5,8},
  {0,4,8},{2,4,6}
};

// -------------------- forward declarations --------------------
static int check_win(const int b[9]);

// -------------------- simple boxed UI helpers --------------------
static void print_box(const char* lines[], int count){
    int width = 0;
    for (int i=0;i<count;i++){
        int len = (int)strlen(lines[i]);
        if (len > width) width = len;
    }
    putchar('+'); for (int i=0;i<width+2;i++) putchar('-'); puts("+");
    for (int i=0;i<count;i++){
        int pad = width - (int)strlen(lines[i]);
        printf("| %s", lines[i]);
        for (int k=0;k<pad;k++) putchar(' ');
        puts(" |");
    }
    putchar('+'); for (int i=0;i<width+2;i++) putchar('-'); puts("+");
}

static void show_end_box(const state_t* S, const char* status_line, int res, const char* finale){
    char line_turn[64];
    snprintf(line_turn, sizeof(line_turn), "Turn: %d  Next: %c", S->turn, (S->who==1?'X':'O'));

    const char* lines[4];
    int n = 0;
    lines[n++] = line_turn;           // Turn/Next
    lines[n++] = status_line;         // Result

    if (res==1){
        lines[n++] = "OK";
        lines[n++] = finale;          // FLAG
    } else {
        lines[n++] = "NO";
    }
    print_box(lines, n);
}

// -------------------- Visual helpers for STRICT mode --------------------
static void sleep_ms(int ms){
#ifdef _WIN32
    Sleep(ms);
#else
    usleep(ms*1000);
#endif
}

// Enable ANSI colors for Windows 10+ terminals (best-effort)
static void enable_ansi_colors(void){
#ifdef _WIN32
    HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
    if (h == INVALID_HANDLE_VALUE) return;
    DWORD mode = 0;
    if (!GetConsoleMode(h, &mode)) return;
    mode |= 0x0004; // ENABLE_VIRTUAL_TERMINAL_PROCESSING
    SetConsoleMode(h, mode);
#endif
}

static void clear_screen(void){
    printf("\x1b[2J\x1b[H");
    fflush(stdout);
}

static void strict_intro(void){
    enable_ansi_colors();
    clear_screen();
#ifdef _WIN32
    Beep(880, 90); sleep_ms(60);
    Beep(660, 90); sleep_ms(60);
#endif
    printf("\x1b[1;31m");
    puts("########################################");
    puts("#            STRICT  MODE              #");
    puts("########################################");
    printf("\x1b[0m");

    printf("\n\x1b[1mArming checks\x1b[0m ");
    for (int i=0;i<14;i++){ printf("."); fflush(stdout); sleep_ms(60); }
    printf(" done\n");

    printf("\x1b[32mREAL Tic-Tac_toe mode activated.\x1b[0m\n\n");
}

// -------------------- TODO functions --------------------
static bool handle_first_r(state_t* S) {
    // (추후 STRICT 전용 초기화가 필요하면 여기에 구현)
    (void)S;
    return true;
}

static bool check_before_move(state_t* S, int idx) {
    // (추후 오름차순 제약 등 검사가 필요하면 여기에 구현)
    (void)S; (void)idx;
    return true;
}

// 합 누적 (가중치 lazy init 포함)
static void handle_after_move(state_t* S, int idx) {
    if (S->weights[0] == 0 && S->weights[8] == 0) {
        static const int W[9] = {41, 8, 2, 48, 18, 15, 9, 16, 57};
        memcpy(S->weights, W, sizeof(W));
    }
    int w = S->weights[idx];
    if (S->who == 1) S->sumX += w; else S->sumO += w;
}

// STRICT: 무승부 && sumX==sumO -> FLAG
static int final_judgement_STRICT(const state_t* S, char* out, size_t n) {
    if (check_win(S->board) != 0) { if (n) snprintf(out, n, "NO"); return 0; }
    if (S->turn != 9)             { if (n) snprintf(out, n, "NO"); return 0; }
    if (S->sumX == S->sumO) {
        if (n) snprintf(out, n, "FLAG{draw_equal_sum}");
        return 1;
    }
    if (n) snprintf(out, n, "NO");
    return 0;
}

// -------------------- Common helpers --------------------
static inline void puts_NO(void){ puts("NO"); fflush(stdout); }
static inline void puts_OK(void){ puts("OK"); fflush(stdout); }

static void hold_console(void){
#if WAIT_ON_EXIT
  #ifdef _WIN32
    printf("\nPress any key to exit...");
    fflush(stdout);
    _getch();
  #else
    printf("\nPress Enter to exit...");
    fflush(stdout);
    int c; while ((c=getchar())!='\n' && c!=EOF) {}
  #endif
#endif
}

static int read_token(void){
    int ch;
    do {
        ch = getchar();
        if (ch==EOF) return EOF;
    } while (isspace(ch));
    return ch;
}

static int check_win(const int b[9]){
    for (int i=0;i<8;i++){
        int a=WIN[i][0], c=WIN[i][1], d=WIN[i][2];
        if (b[a] && b[a]==b[c] && b[c]==b[d]) return b[a];
    }
    return 0;
}

static char cell_char(int v, int pos){
    if (v==1) return 'X';
    if (v==2) return 'O';
    return (char)('0'+pos+1);
}

static void draw_board(const state_t* S){
    printf("\n");
    for (int r=0;r<3;r++){
        for (int c=0;c<3;c++){
            int i=r*3+c;
            printf(" %c ", cell_char(S->board[i], i));
            if (c<2) printf("|");
        }
        printf("\n");
        if (r<2) printf("---+---+---\n");
    }
    printf("Turn: %d  Next: %c\n", S->turn, (S->who==1?'X':'O'));
}

// -------------------- Normal mode --------------------
static void play_normal(state_t* S, int first){
    int idx = first - '1';
    if (S->board[idx]!=0){ printf("Cell occupied. Game over.\n"); hold_console(); return; }
    if (!check_before_move(S, idx)){ printf("Invalid move.\n"); hold_console(); return; }
    S->board[idx]=S->who; handle_after_move(S, idx);
    S->turn++; S->who=2;
    draw_board(S);

    while (1){
        int ch = read_token(); 
        if (ch==EOF){ printf("Input ended.\n"); hold_console(); return; }
        if (ch=='r' || ch=='R'){ printf("Invalid input in normal mode.\n"); hold_console(); return; }
        if (ch<'1' || ch>'9'){ printf("Invalid input.\n"); hold_console(); return; }

        idx = ch - '1';
        if (S->board[idx]!=0){ printf("Cell occupied. Game over.\n"); hold_console(); return; }
        if (!check_before_move(S, idx)){ printf("Invalid move.\n"); hold_console(); return; }
        S->board[idx]=S->who; handle_after_move(S, idx);

        int win = check_win(S->board);
        draw_board(S);
        if (win){
            if (win==1) printf("X wins!\n");
            else        printf("O wins!\n");
            hold_console();
            return;
        }
        S->turn++;
        if (S->turn==9){
            printf("Draw!\n");
            hold_console();
            return;
        }
        S->who = (S->who==1)?2:1;
    }
}

// -------------------- STRICT mode --------------------
static void play_strict(state_t* S){
    S->used_r_first = 1;

    if (!handle_first_r(S)){ puts_NO(); hold_console(); return; }
    strict_intro();           // 시각 효과
    draw_board(S);

    while (1){
        int ch = read_token();
        if (ch==EOF){ printf("Input ended.\n"); hold_console(); return; }
        if (ch=='r' || ch=='R'){ printf("Already used r.\n"); hold_console(); return; }
        if (ch<'1' || ch>'9'){ printf("Invalid input.\n"); hold_console(); return; }

        int idx = ch - '1';
        if (S->board[idx]!=0){ printf("Cell occupied. Game over.\n"); hold_console(); return; }
        if (!check_before_move(S, idx)){ printf("Invalid move.\n"); hold_console(); return; }

        S->board[idx] = S->who;
        handle_after_move(S, idx);

        int win = check_win(S->board);
        draw_board(S);

        if (win){
            const char* status = (win==1) ? "X wins!" : "O wins!";
            char finale[256];
            int res = final_judgement_STRICT(S, finale, sizeof(finale));
            show_end_box(S, status, res, finale);
            hold_console();
            return;
        }

        S->turn++;
        if (S->turn==9){
            const char* status = "Draw!";
            char finale[256];
            int res = final_judgement_STRICT(S, finale, sizeof(finale));
            show_end_box(S, status, res, finale);
            hold_console();
            return;
        }

        S->who = (S->who==1)?2:1;
    }
}

// -------------------- main --------------------
int main(void){
    state_t S; memset(&S,0,sizeof(S));
    S.who=1; S.nextX=1; S.nextO=1;

    printf("Tic-Tac-Toe start!\n");
    draw_board(&S);

    int first = read_token();
    if (first==EOF){ puts_NO(); hold_console(); return 0; }

    if (first=='r' || first=='R'){
        play_strict(&S);
    } else if (first>='1' && first<='9'){
        play_normal(&S, first);
    } else {
        printf("Invalid input.\n");
        hold_console();
    }
    return 0;
}
