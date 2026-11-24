#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

int renegade_init(void);
void renegade_shutdown(void);
void renegade_set_hash_mb(int megabytes);
void renegade_set_chess960(int enabled);
void renegade_new_game(void);
int renegade_set_startpos(void);
int renegade_set_fen(const char* fen);
int renegade_push_move(const char* move);
int renegade_get_fen(char* buffer, int buffer_size);
int renegade_get_legal_moves(char* buffer, int buffer_size);
int renegade_get_game_state(void);
int renegade_search(int depth, int movetime_ms, char* best_move, int buffer_size, int* score_cp, int* depth_reached);
// Zero disables a limit; at least one must be positive. The first depth always completes.
int renegade_search_with_nodes(int depth, int movetime_ms, int64_t nodes, int64_t softnodes,
	char* best_move, int buffer_size, int* score_cp, int* depth_reached);
int renegade_static_eval_cp(void);
void renegade_stop_search(void);

#ifdef __cplusplus
}
#endif
