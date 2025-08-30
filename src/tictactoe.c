// r누르면 STRICT(CTF) 모드, 아니면 일반 모드 (돌리기 구현이 어려워서 그냥 트리거로 사용)
// STRICT 모드에서는 첫 입력이 r이어야 하며,
// 이후 착수는 오름차순 제약을 만족해야 함.
// (오름차순 제약: X는 1,2,3,... / O는 1,2,3,... 순서로만 착수 가능)
// STRICT 모드에서 승리 시 FLAG 출력 (가짜 FLAG 가능)
// STRICT 모드에서 오류 발생 시 NO 출력
// 일반 모드에서는 자유롭게 착수 가능, r 입력 불가 
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

// -------------------- 공용 상태 --------------------
typedef struct {
    int board[9];     // 0=empty, 1=X, 2=O
    int who;          // 현재 턴 (1=X, 2=O)
    int turn;         // 진행된 수
    int used_r_first; // 첫 입력이 r인지 여부

    // --- 2번, 3번 담당에서 사용할 변수들 ---
    int weights[9];   // 각 칸의 가중치
    int sumX, sumO;   // X/O 가중치 합
    int nextX, nextO; // 오름차순 제약 검사용
} state_t;

// -------------------- 승리 조건 --------------------
static const int WIN[8][3] = {
  {0,1,2},{3,4,5},{6,7,8},
  {0,3,6},{1,4,7},{2,5,8},
  {0,4,8},{2,4,6}
};

// -------------------- 2,3번 담당 (아직 구현 X) --------------------
// TODO: 2번 - 첫턴 r 처리
static bool 첫턴_r처리(state_t* S){
    (void)S;
    return true;
}

// TODO: 3번 - 착수 전 검사 (오름차순 제약 등)
static bool 착수전_검사(state_t* S, int idx){
    (void)S; (void)idx;
    return true;
}

// TODO: 2,3번 - 착수 후 처리 (합 누적, next 갱신)
static void 착수후_처리(state_t* S, int idx){
    (void)S; (void)idx;
}

// TODO: 3번 - 최종 판정 (FLAG 출력, 가짜 FLAG, 오류 등)
static int 최종판정_STRICT(const state_t* S, char* out, size_t n){
    (void)S;
    if (n) snprintf(out, n, "ERR:NOT_IMPLEMENTED");
    return 0;
}

// -------------------- (1번 담당) --------------------
static inline void puts_NO(void){ puts("NO"); fflush(stdout); }
static inline void puts_OK(void){ puts("OK"); fflush(stdout); }

static void hold_console(void){ // 게임 종료 후 콘솔 대기
#if WAIT_ON_EXIT
  #ifdef _WIN32 // Windows
    printf("\nPress any key to exit...");
    fflush(stdout);
    _getch();
  #else // Linux, Mac
    printf("\nPress Enter to exit...");
    fflush(stdout);
    int c; while ((c=getchar())!='\n' && c!=EOF) {}
  #endif
#endif
}

static int read_token(void){ // 공백 무시하고 다음 문자 읽기
    int ch;
    do {
        ch = getchar();
        if (ch==EOF) return EOF;
    } while (isspace(ch));
    return ch;
}

static int check_win(const int b[9]){ // 승리 조건 검사
    for (int i=0;i<8;i++){
        int a=WIN[i][0], c=WIN[i][1], d=WIN[i][2];
        if (b[a] && b[a]==b[c] && b[c]==b[d]) return b[a];
    }
    return 0;
}

static char cell_char(int v, int pos){ // 칸 문자 
    if (v==1) return 'X';
    if (v==2) return 'O';
    return (char)('0'+pos+1);
}

static void draw_board(const state_t* S){ // 현재 보드 상태 출력
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

// -------------------- 일반 모드 (1번 담당) --------------------
static void play_normal(state_t* S, int first){
    int idx = first - '1';
    if (S->board[idx]!=0){ printf("Cell occupied. Game over.\n"); hold_console(); return; }
    if (!착수전_검사(S, idx)){ printf("Invalid move.\n"); hold_console(); return; }
    S->board[idx]=S->who; 착수후_처리(S, idx);
    S->turn++; S->who=2;
    draw_board(S);

    while (1){
        int ch = read_token(); 
        if (ch==EOF){ printf("Input ended.\n"); hold_console(); return; }
        if (ch=='r' || ch=='R'){ printf("Invalid input in normal mode.\n"); hold_console(); return; }
        if (ch<'1' || ch>'9'){ printf("Invalid input.\n"); hold_console(); return; }

        idx = ch - '1';
        if (S->board[idx]!=0){ printf("Cell occupied. Game over.\n"); hold_console(); return; }
        if (!착수전_검사(S, idx)){ printf("Invalid move.\n"); hold_console(); return; }
        S->board[idx]=S->who; 착수후_처리(S, idx);

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

// -------------------- STRICT 모드 (1번 담당 I/O + 2,3번 빈자리) --------------------
static void play_strict(state_t* S){
    S->used_r_first = 1;

    if (!첫턴_r처리(S)){ puts_NO(); hold_console(); return; }

    draw_board(S);

    while (1){
        int ch = read_token();
        if (ch==EOF){ printf("Input ended.\n"); hold_console(); return; }
        if (ch=='r' || ch=='R'){ printf("Already used r.\n"); hold_console(); return; }
        if (ch<'1' || ch>'9'){ printf("Invalid input.\n"); hold_console(); return; }

        int idx = ch - '1';
        if (S->board[idx]!=0){ printf("Cell occupied. Game over.\n"); hold_console(); return; }
        if (!착수전_검사(S, idx)){ printf("Invalid move.\n"); hold_console(); return; }

        S->board[idx] = S->who;
        착수후_처리(S, idx);

        int win = check_win(S->board);
        draw_board(S);

        if (win){
            if (win==1) printf("X wins!\n"); else printf("O wins!\n");
            char finale[256];
            int res = 최종판정_STRICT(S, finale, sizeof(finale));
            if (res==1){ puts_OK(); puts(finale); }
            else        puts(finale);
            hold_console();
            return;
        }

        S->turn++;
        if (S->turn==9){
            printf("Draw!\n");
            char finale[256];
            int res = 최종판정_STRICT(S, finale, sizeof(finale));
            if (res==1){ puts_OK(); puts(finale); }
            else        puts(finale);
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
