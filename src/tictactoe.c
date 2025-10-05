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

#ifndef VERIF_OUTPUT
#define VERIF_OUTPUT 0
#endif

// selective obfuscation 
#if defined(__clang__)
  #define OBFUS(x) __attribute__((annotate(x)))
#else
  #define OBFUS(x)
#endif


typedef struct {
    int board[9];     // 0=empty, 1=X, 2=O
    int who;          // 1=X, 2=O
    int turn;
    int is_admin;
    int weights[9];
    int sumX, sumO;
    int lastX, lastO;
    uint64_t h;
} state_t;

static const int WIN[8][3] = {
    {0,1,2},{3,4,5},{6,7,8},
    {0,3,6},{1,4,7},{2,5,8},
    {0,4,8},{2,4,6}
};

static inline void puts_NO(void){ puts("NO"); fflush(stdout); }
static inline void puts_OK(void){ puts("OK"); fflush(stdout); }

/* ===== utils ===== */
static void hold_console(void){
#if WAIT_ON_EXIT
  #ifdef _WIN32
    printf("\nPress any key to exit..."); fflush(stdout); _getch();
  #else
    printf("\nPress Enter to exit..."); fflush(stdout); int c; while ((c=getchar())!='\n' && c!=EOF) {}
  #endif
#endif
}

static int read_token(void){
    int ch;
    do { ch = getchar(); if (ch==EOF) return EOF; } while (isspace(ch));
    int temp_ch;
    while ((temp_ch = getchar()) != '\n' && temp_ch != EOF) {}
    return ch;
}

static int read_word(char *buf, size_t n){
    int ch;
    do { ch = getchar(); if (ch==EOF) return EOF; } while (isspace(ch));
    size_t i=0;
    while (ch!=EOF && !isspace(ch) && i+1<n){ buf[i++] = (char)ch; ch = getchar(); }
    buf[i]='\0'; return 0;
}

static void to_lower(char *s){ for(;*s;++s) if(*s>='A'&&*s<='Z') *s=(char)(*s + ('a'-'A')); }


// board/io 
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

static void print_weights_table(const state_t* S){
#if VERIF_OUTPUT
    puts("\n[ADMIN] Weights (cell# : weight)");
    for (int r=0;r<3;r++){
        for (int c=0;c<3;c++){
            int i=r*3+c;
            printf(" %d:%2d ", i+1, S->weights[i]);
            if (c<2) printf("|");
        }
        printf("\n");
        if (r<2) printf("---------+---------+---------\n");
    }
    puts("");
#endif
}

static void print_admin_step(const state_t* S, int idx_just_played){
#if VERIF_OUTPUT
    unsigned long long hv = (unsigned long long)S->h;
    printf("[ADMIN] move#%d %c -> cell %d (w=%d) | sumX=%d, sumO=%d | h=0x%016llx\n",
           S->turn, (S->who==1?'X':'O'), idx_just_played+1, S->weights[idx_just_played],
           S->sumX, S->sumO, hv);
#endif
}


//  minimal string/const hiding 
static inline void xor_dec_str(char *dst, const unsigned char *src, size_t n, unsigned char k){
    for (size_t i=0;i<n;i++) dst[i]=(char)(src[i]^k);
    dst[n]='\0';
}
static inline void xor_dec_raw(unsigned char *dst, const unsigned char *src, size_t n, unsigned char k){
    for (size_t i=0;i<n;i++) dst[i]=(unsigned char)(src[i]^k);
}
static inline uint64_t rd64_le(const unsigned char *p){
    uint64_t x=0; for(int i=0;i<8;i++) x |= (uint64_t)p[i]<<(i*8); return x;
}

// ("MSG{i-10ve-CtF-%016llx}", key=0xAB)
static const unsigned char ENC_FMT[] = {
  0xE6,0xF8,0xEC,0xD0,0xC2,0x86,0x9A,0x9B,0xDD,0xCE,0x86,
  0xE8,0xDF,0xED,0x86,0x8E,0x9B,0x9A,0x9D,0xC7,0xC7,0xD3,0xD6
};
// (0xBF455607A0F183CF, key=0xAB) little-endian 
static const unsigned char H1E[8] = { 0x64,0x28,0x5A,0x0B,0xAC,0xFD,0xEE,0x14 };
// (0xABCDEF0123456789, key=0xAB) little-endian
static const unsigned char H2E[8] = { 0x22,0xCC,0xEE,0x88,0xAA,0x44,0x66,0x00 };

/* expected = dec(H1E) ^ dec(H2E) = 0x1488b90683b4e446 */
OBFUS("sub")
static uint64_t runtime_expected_hash(void){
    unsigned char b1[8], b2[8];
    xor_dec_raw(b1, H1E, 8, 0xAB);
    xor_dec_raw(b2, H2E, 8, 0xAB);
    uint64_t k1 = rd64_le(b1);
    uint64_t k2 = rd64_le(b2);
    /* wipe stack buffers */
    for (int i=0;i<8;i++){ b1[i]=b2[i]=0; }
    return k1 ^ k2;
}

// admin helpers 
OBFUS("sub")
static bool is_admin_trigger(const char *tok){
    char t[32];
    strncpy(t, tok, sizeof(t)-1);
    t[sizeof(t)-1] = '\0';
    to_lower(t);
    /* "root1004" encoded with Caesar +4 → "vssx5448" */
    char real_trigger[] = "vssx5448";
    for (int i = 0; real_trigger[i] != '\0'; i++) real_trigger[i] -= 4;
    return strcmp(t, real_trigger) == 0;
}

static bool admin_init(state_t* S){
    if(!S) return false;
    const unsigned char encrypted_weights[9] = {0xa3,0xa2,0xbb,0x82,0xa9,0xb9,0xa4,0x9b,0x92};
    const unsigned char key = 0xAB;
    for (int i = 0; i < 9; i++) S->weights[i] = encrypted_weights[i] ^ key;
    S->sumX = S->sumO = 0;
    S->lastX = S->lastO = -1;
    return true;
}

static void init_secret_hash(state_t* S){
    S->h = 1469598103934665603ULL ^ 0xC0DEC0DEC0DEFACEULL;
}


// game rules
static bool pre_check(state_t* S, int idx){
    if (!S || !S->is_admin) return true;
    int w = S->weights[idx];
    if (S->who == 1){
        if (w <= S->lastX){ printf("Invalid order.\n"); return false; }
    } else {
        if (w <= S->lastO){ printf("Invalid order.\n"); return false; }
    }
    return true;
}

static void update_game_state(state_t* S, int idx){
    int w = S->weights[idx];
    if (S->who == 1){ S->sumX += w; S->lastX = w; }
    else            { S->sumO += w; S->lastO = w; }
}

OBFUS("fla")
static void update_secret_hash(state_t* S, int idx){
    int w = S->weights[idx];
    const uint64_t P = 1099511628211ULL;
    uint64_t mix = ( ((uint64_t)(idx & 0xFF))
                   ^ ((uint64_t)w       << 8)
                   ^ ((uint64_t)S->who  << 1) )
                   ^ 0x9E3779B97F4A7C15ULL;
    S->h ^= mix;
    S->h *= P;
}

static bool check_game_rules(const state_t* S){
    return (S->sumX == S->sumO);
}

OBFUS("fla")
static bool check_secret_hash(const state_t* S){
    return S->h == runtime_expected_hash();
}


// modes 
static void play_admin_mode(state_t* S){
    S->is_admin = 1;
    if (!admin_init(S)){ puts_NO(); hold_console(); return; }
    init_secret_hash(S);
    printf("Hello, admin\n");

    draw_board(S);
    print_weights_table(S);

    while (1){
        int ch = read_token();
        if (ch==EOF){ printf("Input ended.\n"); hold_console(); return; }
        if (ch<'1' || ch>'9'){ printf("Invalid input.\n"); hold_console(); return; }

        int idx = ch - '1';
        if (S->board[idx]!=0){ puts_NO(); hold_console(); return; }
        if (!pre_check(S, idx)){ puts_NO(); hold_console(); return; }

        S->board[idx] = S->who;
        update_game_state(S, idx);
        update_secret_hash(S, idx);
        print_admin_step(S, idx);

        int win = check_win(S->board);
        if (win){
            draw_board(S);
            if (win==1) printf("X wins!\n"); else printf("O wins!\n");
            puts_NO();
            hold_console(); return;
        }

        S->turn++;
        if (S->turn==9){
            draw_board(S);
            printf("Draw!\n");

            if (check_game_rules(S) && check_secret_hash(S)) {
                char fmt[24];
                xor_dec_str(fmt, ENC_FMT, 23, 0xAB);

                char finale[128];
                snprintf(finale, sizeof(finale), fmt, (unsigned long long)runtime_expected_hash());
                puts_OK();
                puts(finale);

                /* wipe */
                for (size_t i=0;i<sizeof(fmt);++i) fmt[i]=0;
            } else {
                puts_NO();
            }
            hold_console(); return;
        }

        S->who = (S->who==1)?2:1;
        draw_board(S);
    }
}

static void play_user_mode(state_t* S, int first){
    int idx = first - '1';
    if (S->board[idx]!=0){ printf("Cell already filled. Exit.\n"); hold_console(); return; }
    S->board[idx]=S->who;
    S->turn++; S->who=2;
    draw_board(S);

    while (1){
        int ch = read_token();
        if (ch==EOF){ printf("Input ended.\n"); hold_console(); return; }
        if (ch<'1' || ch>'9'){ printf("Invalid input.\n"); hold_console(); return; }
        idx = ch - '1';
        if (S->board[idx]!=0){ printf("Cell already filled. Exit.\n"); hold_console(); return; }
        S->board[idx]=S->who;
        int win = check_win(S->board);
        draw_board(S);
        if (win){
            if (win==1) printf("X wins!\n"); else printf("O wins!\n");
            hold_console(); return;
        }
        S->turn++;
        if (S->turn==9){
            printf("Draw!\n");
            hold_console(); return;
        }
        S->who = (S->who==1)?2:1;
    }
}
/* ============== */

int main(void){
    state_t S; memset(&S,0,sizeof(S));
    S.who=1;
    printf("Tic-Tac-Toe start!\n");
    draw_board(&S);

    char first_tok[64];
    if (read_word(first_tok, sizeof(first_tok))==EOF){ puts_NO(); hold_console(); return 0; }

    if (is_admin_trigger(first_tok)){
        play_admin_mode(&S);
        return 0;
    }

    if (strlen(first_tok)==1 && first_tok[0]>='1' && first_tok[0]<='9'){
        play_user_mode(&S, first_tok[0]);
        return 0;
    }

    printf("Invalid input.\n");
    hold_console();
    return 0;
}
