#include "game.h"
#include "util.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include <stdio.h>

// relies on _MoveBlockDir order
static const struct BoardPos DIR_DELTAS[5] = {
    {-1, 0}, {1, 0}, {0, -1}, {0, 1}, {0, 0}};

// we can have this globals due to the fact that we only use them during one
// exposed function call; otherwise, they would have to tied to the game state

// we use this a lot
static JNB_THREADLOCAL bool g_visited[BOARD_HEIGHT][BOARD_WIDTH];

static JNB_THREADLOCAL blockidx_t
    g_blocks_need_move[BOARD_HEIGHT * BOARD_WIDTH];
static JNB_THREADLOCAL int g_blocks_need_move_top = -1;

static JNB_THREADLOCAL struct BoardPos g_pos_stack[BOARD_HEIGHT * BOARD_WIDTH];

static JNB_THREADLOCAL blockidx_t
    g_blocks_need_gravity[BOARD_HEIGHT * BOARD_WIDTH];
static JNB_THREADLOCAL int g_blocks_need_gravity_top = -1;

static JNB_THREADLOCAL _Alignas(
    struct GameState) uint8_t _buf[2][GAME_STATE_MAX_SIZE];
static JNB_THREADLOCAL struct GameState *g_tmp_state1 = (void *)_buf[0];
static JNB_THREADLOCAL struct GameState *g_tmp_state2 = (void *)_buf[1];

// safety measure to avoid incorrect global usage
// this should maximize the chance of catching bugs
static inline void public_safe_globals(void) {
#ifndef NDEBUG
    assert(g_blocks_need_move_top == -1);
    assert(g_blocks_need_gravity_top == -1);
    memset(&g_visited, 0xff, sizeof(g_visited));
    memset(&g_blocks_need_move, 0xff, sizeof(g_blocks_need_move));
    memset(&g_blocks_need_move_top, 0xff, sizeof(g_blocks_need_move_top));
    memset(&g_pos_stack, 0xff, sizeof(g_pos_stack));
    memset(&g_blocks_need_gravity, 0xff, sizeof(g_blocks_need_gravity));
    memset(&g_blocks_need_gravity_top, 0xff, sizeof(g_blocks_need_gravity_top));
    memset(g_tmp_state1, 0xff, GAME_STATE_MAX_SIZE);
    memset(g_tmp_state2, 0xff, GAME_STATE_MAX_SIZE);
#endif
}

static inline struct BoardPos add_dir(struct BoardPos pos, MoveBlockDir dir) {
    return (struct BoardPos){
        .x = pos.x + DIR_DELTAS[dir].x,
        .y = pos.y + DIR_DELTAS[dir].y,
    };
}

static inline bool is_fixed_initial(const struct PieceCell *piece) {
    return piece->color & 0x80;
}

static inline void remove_fixed_initial(struct PieceCell *piece) {
    piece->color &= 0x7f;
}

static inline bool
    colors_equal_initial(const struct PieceCell *a, const struct PieceCell *b) {
    return ((a->color ^ b->color) & 0x7f) == 0;
}

static bool pieces_can_connect(
    const struct PieceCell *from,
    const struct PieceCell *to,
    MoveBlockDir dir) {
    if (!colors_equal_initial(from, to))
        return false;
    if (from->no_connect & (1 << dir))
        return false;
    if (to->no_connect & (1 << DIR_OPPOSITE(dir)))
        return false;
    return true;
}

static void fill_block_initial(
    struct GameState *game, struct BoardPos pos, struct Block *dest) {
    // simple DFS on the same color pieces starting at pos, block data inherits
    // the fixed property from ORing the pieces'

    // we don't care if we reuse the same memory, we don't need the 0s
    // memset(g_pos_stack, 0, sizeof(g_pos_stack));

    dest->pos = pos;

    int stack_top = 0;
    g_pos_stack[stack_top] = pos;
    while (stack_top >= 0) {
        struct BoardPos pos = g_pos_stack[stack_top];
        g_visited[pos.y][pos.x] = true;
        --stack_top;

        struct Cell *cell = game_get_pos(game, pos);
        assert(cell->type == CELL_PIECE);
        // block_count is updating until we finish processing blocks, so this
        // works
        cell->data.piece.block = game->block_count;

        // if a piece of the block is fixed, then the whole block is fixed
        // clean up the initial representation of the fixed property, moving it
        // to the block
        if (is_fixed_initial(&cell->data.piece)) {
            dest->fixed = true;
            // even though we clean this for the current cell, an adjacent cell
            // might not have this cleared yet, so we still need
            // colors_equal_initial
            remove_fixed_initial(&cell->data.piece);
        }

        for (MoveBlockDir dir = 0; dir < MOVE_BLOCK_NONE; ++dir) {
            struct BoardPos next_pos = add_dir(pos, dir);

            const struct Cell *new_cell = game_get_pos_safe(game, next_pos);
            if (new_cell == NULL || new_cell->type != CELL_PIECE)
                continue;

            if (g_visited[next_pos.y][next_pos.x])
                continue;

            const struct PieceCell *from = &cell->data.piece;
            const struct PieceCell *to = &new_cell->data.piece;

            if (pieces_can_connect(from, to, dir)) {
                ++stack_top;
                g_pos_stack[stack_top] = next_pos;
            }
        }
    }
}

// this is only be meant to be called in the initial state, the blocks can be
// updated after that
static struct Block *find_blocks(struct GameState *game) {
    memset(g_visited, 0, sizeof(g_visited));

    // we could use a static array instead of allocating this dynamically, but
    // calling game_preprocess_alloc is not a common operation
    // 0-initialization is important for the fixed field
    struct Block *blocks =
        calloc(1, BOARD_HEIGHT * BOARD_WIDTH * sizeof(struct Block));
    if (blocks == NULL)
        return NULL;

    for (board_coord_t i = 0; i < BOARD_HEIGHT; ++i) {
        for (board_coord_t j = 0; j < BOARD_WIDTH; ++j) {
            if (g_visited[i][j] || game->board[i][j].type != CELL_PIECE)
                continue;

            fill_block_initial(
                game, MAKE_BOARD_POS(j, i), &blocks[game->block_count]);
            ++game->block_count;
        }
    }

    return realloc(blocks, game->block_count * sizeof(struct Block));
}

bool game_preprocess_alloc(struct GameState *initial, struct GameState **dest) {
    // finds all blocks in the initial state first, to determine the size of the
    // flexible array member, then copies to the destination

    struct Block *blocks = find_blocks(initial);
    if (blocks == NULL)
        return false;

    if (*dest == NULL) {
        *dest = calloc(1, game_get_size(initial));
    }

    if (*dest != initial) {
        memcpy(*dest, initial, sizeof(struct GameState));
    }

    memcpy(
        (*dest)->blocks, blocks, initial->block_count * sizeof(struct Block));
    free(blocks);

    // not exactly needed
    // if (*dest != initial) {
    //     initial->block_count = 0;
    // }

    public_safe_globals();
    return true;
}

void game_free(struct GameState **game) {
    free(*game);
    *game = NULL;
}

static inline bool
    is_stop_cell(const struct GameState *game, const struct Cell *cell) {
    if (cell->type == CELL_EMPTY)
        return false;

    if (cell->type == CELL_WALL || cell->type == CELL_EMERGE)
        return true;

    // in case more types are added and I forget
    assert(cell->type == CELL_PIECE);
    const struct PieceCell *piece = &cell->data.piece;
    return game->blocks[piece->block].fixed;
}

// adds adjacent blocks to g_blocks_need_move
// marks blocks on top in g_blocks_need_gravity
// returns whether or not it found something that can't be moved
// relies on g_visited being cleared before the call loop
static bool block_add_adjacent_blocks(
    struct GameState *game, blockidx_t block, MoveBlockDir dir) {
    // iterate through every cell of the block, checking for adjacency in the
    // dir direction, return early if we find an unmovable obstacle

    // we don't care if we reuse the same memory, we don't need the 0s
    // memset(g_pos_stack, 0, sizeof(g_pos_stack));

    int stack_top = 0;
    struct BoardPos pos = game->blocks[block].pos;
    g_pos_stack[stack_top] = pos;

    while (stack_top >= 0) {
        struct BoardPos pos = g_pos_stack[stack_top];
        g_visited[pos.y][pos.x] = true;
        --stack_top;

        // gravity
        if (dir != MOVE_BLOCK_UP) {
            struct BoardPos above = add_dir(pos, MOVE_BLOCK_UP);
            const struct Cell *above_cell = game_get_pos_safe(game, above);
            if (above_cell != NULL && above_cell->type == CELL_PIECE) {
                const struct PieceCell *above_piece = &above_cell->data.piece;
                if (above_piece->block != block) {
                    ++g_blocks_need_gravity_top;
                    g_blocks_need_gravity[g_blocks_need_gravity_top] =
                        above_piece->block;
                }
            }
        }

        // add block neighbors
        for (MoveBlockDir dir = 0; dir < MOVE_BLOCK_NONE; ++dir) {
            struct BoardPos next_pos = add_dir(pos, dir);

            const struct Cell *cell = game_get_pos_safe(game, next_pos);
            if (cell == NULL || cell->type != CELL_PIECE)
                continue;

            if (g_visited[next_pos.y][next_pos.x])
                continue;

            if (cell->data.piece.block == block) {
                ++stack_top;
                g_pos_stack[stack_top] = next_pos;
            }
        }

        // we don't need to iterate directions for this, only look to one side

        struct BoardPos next_pos = add_dir(pos, dir);
        const struct Cell *cell = game_get_pos_safe(game, next_pos);

        // end of board
        // TODO: maybe make this configurable for wraparound?
        if (cell == NULL)
            return true;

        if (cell->type == CELL_EMPTY)
            continue;

        if (is_stop_cell(game, cell))
            return true;

        // if it's visited and it's not a stop cell, it's a piece of a block
        // processed previously from move_block
        if (g_visited[next_pos.y][next_pos.x])
            continue;

        assert(cell->type == CELL_PIECE);
        // all non-stop cells are pieces, so we can safely do this
        const struct PieceCell *new_piece = &cell->data.piece;

        if (new_piece->block == block) {
            // we don't need to add the block to the stack, it's already been
            // added
            continue;
        }

        // case where we found an adjacent block

        if (game->blocks[new_piece->block].fixed)
            return true;

        // if it's not part of our block and is a piece from a movable block,
        // add its block
        ++g_blocks_need_move_top;
        g_blocks_need_move[g_blocks_need_move_top] = new_piece->block;
    }

    // haven't found anything around us at all, so this block can be moved fine
    return false;
}

// just moves a block, which can result in a temporarily unresolved state
// returns whether or not the move was successful
// marks moved blocks in g_blocks_need_gravity as well
static bool move_block(
    struct GameState *restrict game,
    blockidx_t block,
    MoveBlockDir dir,
    struct GameState *restrict dest) {
    // finds all blocks that need to be moved in order to move the given block
    // by doing a DFS starting at the block using block_add_adjacent_blocks,
    // then performs the move if the destination is not NULL

    assert(dest != game);

    if (game->blocks[block].fixed == true)
        return false;

    memset(g_visited, 0, sizeof(g_visited));
    memset(g_blocks_need_move, 0, sizeof(g_blocks_need_move));
    g_blocks_need_move_top = 0;
    g_blocks_need_move[0] = block;

    // save this in case we don't end up moving anything
    int gravity_top_before = g_blocks_need_gravity_top;

    while (g_blocks_need_move_top >= 0) {
        blockidx_t block = g_blocks_need_move[g_blocks_need_move_top];
        --g_blocks_need_move_top;

        // gravity
        ++g_blocks_need_gravity_top;
        g_blocks_need_gravity[g_blocks_need_gravity_top] = block;

        // we don't have to check if a block is already visited, as the
        // block_add_adjacent_blocks function does that for us

        bool stop = block_add_adjacent_blocks(game, block, dir);
        if (stop) {
            // this is needed becuase we don't want to mark blocks for gravity
            // if we don't end up moving anything
            g_blocks_need_gravity_top = gravity_top_before;

// early return is fine
#ifndef NDEBUG
            g_blocks_need_move_top = -1;
#endif
            return false;
        }
    }

    if (dest == NULL) {
        // theoretically unneeded since this use case doesn't happen within the
        // internal game logic
        g_blocks_need_gravity_top = gravity_top_before;
        return true;
    }

    // important! clear the board
    memset(dest, 0, game_get_size(game));

    // at this point, the state of g_visited is useful to us as a mask for
    // the cells that need to be moved, so all we have to do is iterate over
    // all the cells

    // copy the block data
    game_copy_block_data(dest, game);

#if 1
    for (board_coord_t i = 0; i < BOARD_HEIGHT; ++i) {
        for (board_coord_t j = 0; j < BOARD_WIDTH; ++j) {
            MoveBlockDir needed_dir = g_visited[i][j] ? dir : MOVE_BLOCK_NONE;
            struct BoardPos target = add_dir(MAKE_BOARD_POS(j, i), needed_dir);
            struct Cell *dest_cell = game_get_pos(dest, target);

            if (dest_cell->type != CELL_EMPTY)
                // this means something was moved here at a previous step
                continue;

            *dest_cell = *game_get_pos(game, MAKE_BOARD_POS(j, i));

            struct Block *block_of = &dest->blocks[dest_cell->data.piece.block];
            if (block_of->pos.x == j && block_of->pos.y == i) {
                // we can update the block pos in the dest now
                block_of->pos = target;
            }
        }
    }

#else
    // in this branch, we are going to do 2 passes to make sure overlaps don't
    // occur

    // the first pass will be to copy the blocks that don't need to be moved
    for (board_coord_t i = 0; i < BOARD_HEIGHT; ++i) {
        for (board_coord_t j = 0; j < BOARD_WIDTH; ++j) {
            // if we need to move it, leave it to the second pass
            if (g_visited[i][j])
                continue;

            struct Cell *dest_cell = game_get_pos(dest, MAKE_BOARD_POS(j, i));
            *dest_cell = *game_get_pos(game, MAKE_BOARD_POS(j, i));
        }
    }

    // the second pass will be to move the blocks that need to be moved
    for (board_coord_t i = 0; i < BOARD_HEIGHT; ++i) {
        for (board_coord_t j = 0; j < BOARD_WIDTH; ++j) {
            if (!g_visited[i][j])
                continue;

            struct BoardPos target = add_dir(MAKE_BOARD_POS(j, i), dir);
            struct Cell *dest_cell = game_get_pos(dest, target);
            assert(dest_cell->type == CELL_EMPTY);
            *dest_cell = *game_get_pos(game, MAKE_BOARD_POS(j, i));

            struct Block *block_of = &dest->blocks[dest_cell->data.piece.block];
            if (block_of->pos.x == j && block_of->pos.y == i) {
                // we can update the block pos in the dest now
                block_of->pos = target;
            }
        }
    }
#endif

    return true;
}

static inline struct BoardPos min_pos(struct BoardPos a, struct BoardPos b) {
    if (a.y == b.y) {
        return a.x < b.x ? a : b;
    }
    return a.y < b.y ? a : b;
}

static void connect_adjacent_blocks(struct GameState *game, blockidx_t block) {
    int stack_top = 0;
    struct BoardPos pos = game->blocks[block].pos;
    g_pos_stack[stack_top] = pos;

    struct Block *block_ptr = &game->blocks[block];

    while (stack_top >= 0) {
        struct BoardPos pos = g_pos_stack[stack_top];
        g_visited[pos.y][pos.x] = true;
        --stack_top;

        struct Cell *cell = game_get_pos(game, pos);
        assert(cell->type == CELL_PIECE);

        // update the lexically smallest position representing the block
        // this is only going to do anything when the previous block assigned to
        // the cell at pos was different from our block, but doing it
        // unconditionally is probably faster
        // if (cell->data.piece.block != block)
        block_ptr->pos = min_pos(block_ptr->pos, pos);
        // change the block index on the board
        cell->data.piece.block = block;

        for (MoveBlockDir dir = 0; dir < MOVE_BLOCK_NONE; ++dir) {
            struct BoardPos next_pos = add_dir(pos, dir);

            const struct Cell *new_cell = game_get_pos_safe(game, next_pos);
            if (new_cell == NULL || new_cell->type != CELL_PIECE)
                continue;

            if (g_visited[next_pos.y][next_pos.x])
                continue;

            const struct PieceCell *from = &cell->data.piece;
            const struct PieceCell *to = &new_cell->data.piece;

            if (to->block == block || pieces_can_connect(from, to, dir)) {
                ++stack_top;
                g_pos_stack[stack_top] = next_pos;
            }
        }
    }
}

static void
    update_block_connections(struct GameState *game, struct Block *tmp_buf) {
    // iterate over blocks, updating the connections for each
    // blocks will also get reindexed and it's important that we iterate over
    // them in order, so that the resulting block array is contiguous
    // first, we are going to be modifying them in-place, which will cause some
    // of the blocks to become invalid, then we will reindex them using tmp

    memset(g_visited, 0, sizeof(g_visited));

    int dest_idx = 0;
    for (blockidx_t src_idx = 0; src_idx < game->block_count; ++src_idx) {
        struct Block *src = &game->blocks[src_idx];

        if (g_visited[src->pos.y][src->pos.x])
            continue;

        connect_adjacent_blocks(game, src_idx);

        tmp_buf[dest_idx] = *src;
        ++dest_idx;
    }

    memcpy(game->blocks, tmp_buf, game->block_count * sizeof(struct Block));
    game->block_count = dest_idx;
}

// TODO: add a higher level version of this function that returns some sort
// of state that can be advanced and that can be used to generate a delta
// for each intermediate state
bool game_do_move(
    struct GameState *restrict game,
    blockidx_t block,
    MoveBlockDir dir,
    struct GameState *restrict dest) {

    assert(DIR_IS_HORIZONTAL(dir));

    // other functions use this to mark all moved blocks and also the blocks
    // above them
    memset(g_blocks_need_gravity, 0, sizeof(g_blocks_need_gravity));
    g_blocks_need_gravity_top = -1;

    struct GameState *front = g_tmp_state1;
    struct GameState *back = g_tmp_state2;

    bool could_move = move_block(game, block, dir, front);
    if (!could_move)
        return false;

    // if the initial is successful, none of the post-move operations can fail,
    // therefore we can return if there is no destination
    if (dest == NULL)
        return true;

    // apply gravity by moving all the blocks down until they can't be moved any
    // more
    // first we move the newly moved blocks
    // after that, we can move the blocks marked as above the moved blocks in
    // their intial position

    while (g_blocks_need_gravity_top >= 0) {
        blockidx_t block = g_blocks_need_gravity[g_blocks_need_gravity_top];
        --g_blocks_need_gravity_top;

        while (move_block(front, block, MOVE_BLOCK_DOWN, back)) {
            struct GameState *tmp = front;
            front = back;
            back = tmp;
        }
    }

    // resulting state is now in front
    memcpy(dest, front, game_get_size(front));

    update_block_connections(dest, back->blocks);

    public_safe_globals();
    return true;
}
