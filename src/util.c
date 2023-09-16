#include "util.h"

#include <inttypes.h>
#include <stdio.h>

void print_cell_data(const struct GameState *game, const struct Cell *cell) {
    switch (cell->type) {
    case CELL_EMPTY:
        printf("Empty");
        break;
    case CELL_PIECE: {
        const struct PieceCell *piece = &cell->data.piece;
        printf(
            "Piece: color=%" PRIi8
            ", fixed=%d (inferred from block), no_connect=%d, block=%d",
            piece->color, game->blocks[piece->block].fixed, piece->no_connect,
            piece->block);
        break;
    }
    case CELL_WALL:
        printf("Wall");
        break;
    }
}

static inline void render_cell(const struct Cell *cell) {
    switch (cell->type) {
    case CELL_EMPTY:
        printf(" ");
        break;
    case CELL_PIECE:
        printf("%1" PRIi8, cell->data.piece.color);
        break;
    case CELL_WALL:
        printf("#");
        break;
    }
}

void print_game(const struct GameState *game) {
    for (board_coord_t i = 0; i < BOARD_HEIGHT; ++i) {
        for (board_coord_t j = 0; j < BOARD_WIDTH; ++j) {
            render_cell(&game->board[i][j]);
        }
        printf("\n");
    }
}

void print_blocks(const struct GameState *game) {
    for (int i = 0; i < game->block_count; ++i) {
        printf(
            "Block %d: (%d, %d): fixed %d\n", i, game->blocks[i].pos.x,
            game->blocks[i].pos.y, (int)game->blocks[i].fixed);
    }
}

void print_cells(const struct GameState *game) {
    for (board_coord_t i = 0; i < BOARD_HEIGHT; ++i) {
        for (board_coord_t j = 0; j < BOARD_WIDTH; ++j) {
            printf("(%d, %d): ", j, i);
            print_cell_data(game, &game->board[i][j]);
            printf("\n");
        }
    }
}
