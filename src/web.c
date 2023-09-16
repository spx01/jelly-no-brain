#include "game.h"

#include "b64.h"
#include "util.h"

#include <stdio.h>

#ifdef __EMSCRIPTEN__
    #include <emscripten.h>
#else
    #define EMSCRIPTEN_KEEPALIVE
#endif

#define JNB_API EMSCRIPTEN_KEEPALIVE

#define MAX_UNDO 10

struct Game {
    _Alignas(struct GameState) uint8_t states[MAX_UNDO][GAME_STATE_MAX_SIZE];
    int current_state;
    int move_count;
    int undo_avail;
    int action_count;
    char *b64_buf;
};

void JNB_API GAME_test(struct Game *game);

static void gamestate_placeholder(struct GameState *state) {
    memset(state, 0, sizeof(struct GameState));

    BOARD_DATA_STRUCTURE_PTR(struct Cell, board) = &state->board;

    // fill the top and bottom rows with walls
    for (board_coord_t i = 0; i < BOARD_HEIGHT; ++i) {
        (*board)[i][0].type = CELL_WALL;
        (*board)[i][BOARD_WIDTH - 1].type = CELL_WALL;
    }

    // fill the left and right columns with walls
    for (board_coord_t j = 0; j < BOARD_WIDTH; ++j) {
        (*board)[0][j].type = CELL_WALL;
        (*board)[BOARD_HEIGHT - 1][j].type = CELL_WALL;
    }

    (*board)[6][6].type = CELL_WALL;

    (*board)[5][6].type = CELL_PIECE;
    (*board)[5][6].data.piece.color = piece_make_color(1, false);

    (*board)[5][5].type = CELL_PIECE;
    (*board)[5][5].data.piece.color = piece_make_color(1, false);

    (*board)[4][5].type = CELL_PIECE;
    (*board)[4][5].data.piece.color = piece_make_color(1, false);

    (*board)[8][5].type = CELL_PIECE;
    (*board)[8][5].data.piece.color = piece_make_color(1, false);

    (*board)[4][6].type = CELL_PIECE;
    (*board)[4][6].data.piece.color = piece_make_color(2, false);

    (*board)[3][6].type = CELL_PIECE;
    (*board)[3][6].data.piece.color = piece_make_color(2, false);
    (*board)[3][6].data.piece.no_connect = PIECE_CON_RIGHT | PIECE_CON_UP;

    (*board)[3][5].type = CELL_PIECE;
    (*board)[3][5].data.piece.color = piece_make_color(2, false);

    (*board)[2][6].type = CELL_PIECE;
    (*board)[2][6].data.piece.color = piece_make_color(3, false);

    (*board)[2][5].type = CELL_PIECE;
    (*board)[2][5].data.piece.color = piece_make_color(4, false);

    bool res = game_preprocess_alloc(state, &state);
    if (!res) {
        printf("gamestate_placeholder: failed to preprocess game state\n");
        return;
    }
}

static inline struct GameState *get_state(struct Game *game, int idx) {
    return (struct GameState *)game->states[idx];
}

static inline struct GameState *get_current_state(struct Game *game) {
    return get_state(game, game->current_state);
}

struct Game *JNB_API GAME_new(void) {
    struct Game *game = calloc(1, sizeof(struct Game));
    if (!game)
        return NULL;
    game->undo_avail = MAX_UNDO;
    game->b64_buf = NULL;
    gamestate_placeholder(get_current_state(game));
    return game;
}

void JNB_API GAME_free(struct Game *game) {
    free(game->b64_buf);
    free(game);
}

static inline struct GameState *get_next_state(struct Game *game) {
    return get_state(game, (game->current_state + 1) % MAX_UNDO);
}

static inline void advance_state(struct Game *game) {
    game->current_state = (game->current_state + 1) % MAX_UNDO;
    --game->move_count;
    game->undo_avail += game->undo_avail < MAX_UNDO;
}

bool JNB_API GAME_undo(struct Game *game) {
    bool res = false;
    if (game->undo_avail > 0) {
        game->current_state = (game->current_state - 1) % MAX_UNDO;
        --game->undo_avail;
        res = true;
    }
    --game->move_count;
    ++game->action_count;
    return res;
}

bool JNB_API
    GAME_move_piece(struct Game *game, int x, int y, MoveBlockDir dir) {
    if (!DIR_IS_HORIZONTAL(dir) || dir >= MOVE_BLOCK_NONE) {
        printf("move_piece: invalid direction\n");
        return false;
    }
    if (x >= BOARD_WIDTH || y >= BOARD_HEIGHT) {
        printf("move_piece: invalid coordinates\n");
        return false;
    }
    struct GameState *current = get_current_state(game);
    struct GameState *next = get_next_state(game);
    blockidx_t block = current->board[y][x].data.piece.block;
    if (!game_do_move(current, block, dir, next))
        return false;
    advance_state(game);
    ++game->action_count;
    return true;
}

struct Cell *get_cell_internal(struct Game *game, int x, int y) {
    if (x >= BOARD_WIDTH || y >= BOARD_HEIGHT || x < 0 || y < 0) {
        return NULL;
    }
    return &get_current_state(game)->board[y][x];
}

struct Cell *JNB_API GAME_get_cell(struct Game *game, int x, int y) {
    struct Cell *cell = get_cell_internal(game, x, y);
    if (!cell) {
        printf("get_cell: invalid coordinates\n");
    }
    return cell;
}

int JNB_API GAME_get_move_count(struct Game *game) {
    return game->move_count;
}

int JNB_API GAME_get_action_count(struct Game *game) {
    return game->action_count;
}

int JNB_API GAME_get_undo_avail(struct Game *game) {
    return game->undo_avail;
}

CellType JNB_API GAME_get_cell_type(struct Game *game, struct Cell *cell) {
    return cell->type;
}

int8_t JNB_API GAME_get_color(struct Game *game, struct Cell *cell) {
    if (cell->type == CELL_PIECE)
        return cell->data.piece.color;
    else if (cell->type == CELL_EMERGE)
        return cell->data.emerge.color;
    else
        return -1;
}

PieceConnect JNB_API
    GAME_piece_where_can_connect(struct Game *game, struct Cell *cell) {
    if (cell->type != CELL_PIECE)
        return -1;
    return 0xf ^ cell->data.piece.no_connect;
}

blockidx_t JNB_API GAME_get_block(struct Game *game, struct Cell *cell) {
    if (cell->type != CELL_PIECE)
        return -1;
    return cell->data.piece.block;
}

bool JNB_API GAME_block_is_fixed(struct Game *game, blockidx_t block) {
    return get_current_state(game)->blocks[block].fixed;
}

static const struct BoardPos DIR_DELTAS[4] = {{-1, 0}, {1, 0}, {0, -1}, {0, 1}};

int32_t JNB_API GAME_get_cell_coords(struct Game *game, struct Cell *cell) {
    if ((void *)cell < (void *)get_current_state(game)->board) {
        printf("get_cell_coords: cell is not in the current state\n");
        return -1;
    }
    uintptr_t diff =
        (struct Cell *)cell - (struct Cell *)get_current_state(game)->board;
    int32_t y = diff / BOARD_WIDTH;
    int32_t x = diff % BOARD_WIDTH;
    return (x << 16) | y;
}

int JNB_API game_get_board_width(void) {
    return BOARD_WIDTH;
}

int JNB_API game_get_board_height(void) {
    return BOARD_HEIGHT;
}

void JNB_API GAME_print_current_state(struct Game *game) {
    print_game(get_current_state(game));
}

int JNB_API GAME_get_current_state_block_count(struct Game *game) {
    return get_current_state(game)->block_count;
}

static bool wall_emerge_adj(struct Cell *c1, struct Cell *c2) {
    return (c1->type == CELL_EMERGE && c2->type == CELL_WALL) ||
        (c1->type == CELL_WALL && c2->type == CELL_EMERGE);
}

// retunrs whether two cells should be represented as connected in some way by
// the renderer
static bool should_connect(struct Cell *c1, struct Cell *c2) {
    // cells being conneted could mean different things
    // for instance, if a piece is adjacent to a "connective" piece in the same
    // block, then the connection would be represented differently on the
    // screen, as the connective block would use a different sprite
    // the renderer should handle this, this API doesn't
    if (c1->type == CELL_PIECE && c2->type == CELL_PIECE) {
        return c1->data.piece.block == c2->data.piece.block;
    } else if (c1->type == c2->type) {
        return true;
    } else {
        return wall_emerge_adj(c1, c2);
    }
}

int8_t JNB_API GAME_cell_where_connected(struct Game *game, struct Cell *cell) {
    // FIXME
    int32_t pos = GAME_get_cell_coords(game, cell);
    PieceConnect res = 0;
    for (int8_t i = 0; i < 4; ++i) {
        struct BoardPos delta = DIR_DELTAS[i];
        struct Cell *adj = get_cell_internal(
            game, (pos >> 16) + delta.x, (pos & 0xffff) + delta.y);
        if (adj != NULL && should_connect(cell, adj)) {
            res |= 1 << i;
        }
    }
    return res;
}

const char *JNB_API GAME_get_current_state_b64(struct Game *game) {
    free(game->b64_buf);
    game->b64_buf = b64_encode_alloc(
        get_current_state(game), game_get_size(get_current_state(game)));
    return game->b64_buf;
}

void GAME_test(struct Game *game) {
    printf("GAME_test\n");
    const char *test_strings[4] = {
        "hello world!",
        "mod 3 is 0<>",
        "mod 3 is 1<><",
        "mod 3 is 2<><>",
    };
    for (int i = 0; i < 4; ++i) {
        char *str = b64_encode_alloc(test_strings[i], strlen(test_strings[i]));
        printf("%s\n", str);
        char *dec = b64_decode_alloc(str, NULL);
        printf("%s\n", dec);
        free(dec);
        free(str);
    }
}
