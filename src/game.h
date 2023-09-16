#pragma once

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#define BOARD_WIDTH 14
#define BOARD_HEIGHT 10

typedef int8_t board_coord_t;

enum _CellType { CELL_EMPTY = 0, CELL_WALL, CELL_PIECE, CELL_EMERGE };
typedef int8_t CellType;

enum _MoveBlockDir {
    MOVE_BLOCK_LEFT = 0,
    MOVE_BLOCK_RIGHT,
    // up and down are meant primarily for internal gravity and spawning
    // handling
    MOVE_BLOCK_UP,
    MOVE_BLOCK_DOWN,
    MOVE_BLOCK_NONE
};
typedef int8_t MoveBlockDir;

enum _PieceConnect {
    PIECE_CON_LEFT = 1 << MOVE_BLOCK_LEFT,
    PIECE_CON_RIGHT = 1 << MOVE_BLOCK_RIGHT,
    PIECE_CON_UP = 1 << MOVE_BLOCK_UP,
    PIECE_CON_DOWN = 1 << MOVE_BLOCK_DOWN
};
typedef int8_t PieceConnect;

#define DIR_OPPOSITE(dir) ((dir) ^ 1)
#define DIR_IS_HORIZONTAL(dir) ((dir) < MOVE_BLOCK_UP)

// signed is not enough (in theory)
// index of a block in the game state
typedef uint8_t blockidx_t;

// reserve highest bit for holding fixed state until we process the blocks and
// discard it
typedef int8_t color_t;

/// @brief Create a color value for setting the initial state.
/// @param color Should be <= `INT8_MAX`
/// @param fixed It will be encoded inside the color value, for simplicity
/// @return
static inline color_t piece_make_color(int8_t color, bool fixed) {
    assert(color <= INT8_MAX);
    return (color & 0x7f) | (fixed << 7);
}

struct EmergeCell {
    color_t color;
    MoveBlockDir dir;
    bool fixed;
};

struct PieceCell {
    color_t color;
    PieceConnect no_connect;
    // block in the game state that this piece is part of
    blockidx_t block;
};

// built with 0-initialization in mind
struct Cell {
    CellType type;
    union {
        struct PieceCell piece;
        struct EmergeCell emerge;
    } data;
};

struct BoardPos {
    board_coord_t x;
    board_coord_t y;
};

#define MAKE_BOARD_POS(x, y) ((struct BoardPos){(x), (y)})

/// @brief A block is a set of connected piece-type cells. NOTE: the pieces are
/// not necessarily the same color, but any new pieces that connect have to be
/// the same color as the adjacent ones that are already part of the block.
struct Block {
    /// @brief Position of the block's top left corner.
    struct BoardPos pos;
    /// @brief Whether or not the block contains a fixed cell.
    bool fixed;
};

/// @brief Load something by writing to the board. Then use `game_finish_init`
/// to finish initialization. To release resources, use `game_free`.
struct GameState {
    /// @brief The game board; to be altered before calling
    /// `game_preprocess_alloc`.
    struct Cell board[BOARD_HEIGHT][BOARD_WIDTH];
    int block_count;
    // opted for flexible array member because this will make working with an
    // array of game states more efficient, although we will lose some memory,
    // as every other game state will be assumed to have the same block_count
    // from a memory perspective when operating with the game state array
    struct Block blocks[];
};

#define GAME_STATE_MAX_SIZE     \
    (sizeof(struct GameState) + \
     BOARD_WIDTH * BOARD_HEIGHT * sizeof(struct Block))

/// @brief Get the size of a game state.
static inline size_t game_get_size(const struct GameState *game) {
    return sizeof(struct GameState) + game->block_count * sizeof(struct Block);
}

static inline void game_copy_block_data(
    struct GameState *restrict dest, const struct GameState *restrict src) {
    dest->block_count = src->block_count;
    memcpy(dest->blocks, src->blocks, src->block_count * sizeof(struct Block));
}

/// @brief Get the cell at the given position.
static inline struct Cell *
    game_get_pos(struct GameState *game, struct BoardPos pos) {
    return &game->board[pos.y][pos.x];
}

/// @brief Get the cell at the given position with bounds checking.
static inline struct Cell *
    game_get_pos_safe(struct GameState *game, struct BoardPos pos) {
    if (pos.x < 0 || pos.x >= BOARD_WIDTH || pos.y < 0 || pos.y >= BOARD_HEIGHT)
        return NULL;
    return &game->board[pos.y][pos.x];
}

/// @brief Do preprocessing before the game state is ready to be used or after
/// it has been modified (doesn't reuse information). If `*dest` is `NULL`, then
/// it will also perform allocation.
/// @param game
/// @return Whether or not memory allocation succeeded
bool game_preprocess_alloc(struct GameState *game, struct GameState **dest);

/// @brief Tries to do a game move by moving a block either left or right.
/// Writes the updated game state to `dest` if it's not `NULL`. Caller is
/// responsible for properly allocating dest.
/// @param game
/// @param block
/// @param dir Direction of move
/// @param dest Destination game state; can't be the same as `game`
/// @return Whether or not the move can be made
bool game_do_move(
    struct GameState *restrict game,
    blockidx_t block,
    MoveBlockDir dir,
    struct GameState *restrict dest);

/// @brief Free and invalidate a game state. This only makes sense if `*dest`
/// was `NULL` for `game_preprocess_alloc`.
/// @param game
void game_free(struct GameState **game);
