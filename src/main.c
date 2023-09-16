#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "game.h"
#include "util.h"

struct GameState *make_simple_game(void) {
    struct GameState tmp;
    memset(&tmp, 0, sizeof(struct GameState));

    // fill the top and bottom rows with walls
    for (board_coord_t i = 0; i < BOARD_HEIGHT; ++i) {
        tmp.board[i][0].type = CELL_WALL;
        tmp.board[i][BOARD_WIDTH - 1].type = CELL_WALL;
    }

    // fill the left and right columns with walls
    for (board_coord_t j = 0; j < BOARD_WIDTH; ++j) {
        tmp.board[0][j].type = CELL_WALL;
        tmp.board[BOARD_HEIGHT - 1][j].type = CELL_WALL;
    }

    tmp.board[6][6].type = CELL_WALL;

    tmp.board[5][6].type = CELL_PIECE;
    tmp.board[5][6].data.piece.color = piece_make_color(1, false);

    tmp.board[5][5].type = CELL_PIECE;
    tmp.board[5][5].data.piece.color = piece_make_color(1, false);

    tmp.board[4][5].type = CELL_PIECE;
    tmp.board[4][5].data.piece.color = piece_make_color(1, false);

    tmp.board[8][5].type = CELL_PIECE;
    tmp.board[8][5].data.piece.color = piece_make_color(1, false);

    tmp.board[4][6].type = CELL_PIECE;
    tmp.board[4][6].data.piece.color = piece_make_color(2, false);

    tmp.board[3][6].type = CELL_PIECE;
    tmp.board[3][6].data.piece.color = piece_make_color(2, false);

    tmp.board[3][5].type = CELL_PIECE;
    tmp.board[3][5].data.piece.color = piece_make_color(2, false);

    tmp.board[2][6].type = CELL_PIECE;
    tmp.board[2][6].data.piece.color = piece_make_color(3, false);

    tmp.board[2][5].type = CELL_PIECE;
    tmp.board[2][5].data.piece.color = piece_make_color(4, false);

    struct GameState *dest = NULL;
    bool res = game_preprocess_alloc(&tmp, &dest);
    if (!res)
        return NULL;
    return dest;
}

int main(void) {
    // TODO: first, test the current collision implementation thoroughly

    // TODO: add logging (simple)

    // TODO: advanced state builder (not very important right now)
    // ideas:
    // - string representation for main board similar to the one printed by the
    // `print_game` function
    // - multiple input layers: board, block mask, connect direction mask
    // - special representation for connective pieces
    // - connective pieces can implicitly inherit the color of one of the pieces
    // they connect to
    // - connective pieces will have a default connect direction applied, one
    // where they can only connect to the piece they are connected to (dictated
    // by block mask)

    struct GameState *game = make_simple_game();
    print_game(game);
    print_blocks(game);
    // printf(
    //     "trying to move block 0: %d\n",
    //     game_do_move(game, 0, MOVE_BLOCK_LEFT, NULL));

    // TODO: better simple interface for this
    // won't be useful for the solver though
    struct GameState *game2 = calloc(1, game_get_size(game));
    game_do_move(game, 3, MOVE_BLOCK_LEFT, game2);

    print_game(game2);
    print_blocks(game2);
    game_free(&game);
    game_free(&game2);
    return 0;
}
