#pragma once

#include "game.h"

// bool simple_game_from_string(const char *str, struct GameState *game);
// bool load_game_file(const char *path, struct GameState *game);
// bool save_game_file(const char *path, struct GameState *game);

void print_game(const struct GameState *game);
void print_blocks(const struct GameState *game);
void print_cell_data(const struct GameState *game, const struct Cell *cell);
void print_cells(const struct GameState *game);

#define BOARD_DATA_STRUCTURE_PTR(T, name) T(*name)[BOARD_HEIGHT][BOARD_WIDTH]

#ifdef JNB_THREADING
    #if defined(__GNUC__) || defined(__clang__)
        #define JNB_THREADLOCAL __thread
    #elif defined(_MSC_VER)
        #define JNB_THREADLOCAL __declspec(thread)
    #else
        #error "Unknown compiler for threading"
    #endif
#else
    #define JNB_THREADLOCAL
#endif
