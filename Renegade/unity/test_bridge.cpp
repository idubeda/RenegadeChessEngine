#include "RenegadeBridge.h"

#include <atomic>
#include <cassert>
#include <chrono>
#include <cstring>
#include <thread>

int main() {
	assert(renegade_init() == 0);
	assert(renegade_init() == 0);
	renegade_set_hash_mb(1);
	renegade_new_game();
	assert(renegade_set_startpos() == 0);
	char fen[128];
	char moves[2048];
	assert(renegade_get_fen(fen, sizeof(fen)) == 0);
	assert(std::strcmp(fen, "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1") == 0);
	assert(renegade_get_legal_moves(moves, sizeof(moves)) == 20);
	assert(std::strstr(moves, "e2e4") != nullptr);
	assert(renegade_get_legal_moves(moves, 1) == -1);
	assert(renegade_push_move("e2e5") == 0);
	assert(renegade_push_move("i2e4") == 0);
	assert(renegade_push_move("e2e4q") == 0);
	assert(renegade_push_move("e2e4") == 1);
	assert(renegade_get_fen(fen, sizeof(fen)) == 0);
	assert(std::strstr(fen, " b ") != nullptr);

	assert(renegade_set_fen("not a fen") == -1);
	assert(renegade_set_fen("8/8/8/8/8/8/8/8 w - - 0 1") == -1);
	assert(renegade_set_fen("7k/8/8/8/8/8/8/K7 w K - 0 1") == -1);
	assert(renegade_set_fen("7k/8/8/8/8/8/8/K7 w - e2 0 1") == -1);
	assert(renegade_set_fen("7k/8/8/8/8/8/8/K7 w - - 999999999999 1") == -1);
	assert(renegade_get_fen(fen, sizeof(fen)) == 0);
	assert(std::strstr(fen, " b ") != nullptr);
	assert(renegade_set_fen("7k/6Q1/6K1/8/8/8/8/8 b - -") == 0);
	assert(renegade_get_game_state() == 1);
	assert(renegade_get_legal_moves(moves, sizeof(moves)) == 0);
	assert(std::strlen(moves) == 0);

	assert(renegade_set_startpos() == 0);
	char best[16];
	int score = 0, depth = 0;
	assert(renegade_search(0, 0, best, sizeof(best), &score, &depth) == -1);
	assert(renegade_search(2, 0, best, sizeof(best), &score, &depth) == 0);
	assert(depth == 2);
	assert(renegade_push_move(best) == 1);
	assert(renegade_set_startpos() == 0);
	assert(renegade_search_with_nodes(0, 0, 0, 0, best, sizeof(best), &score, &depth) == -1);
	assert(renegade_search_with_nodes(2, 0, -1, 0, best, sizeof(best), &score, &depth) == -1);
	assert(renegade_search_with_nodes(2, 0, 0, -1, best, sizeof(best), &score, &depth) == -1);
	renegade_new_game();
	assert(renegade_search_with_nodes(6, 0, 1, 0, best, sizeof(best), &score, &depth) == 0);
	assert(depth == 1 && renegade_push_move(best) == 1);
	assert(renegade_set_startpos() == 0);
	renegade_new_game();
	assert(renegade_search_with_nodes(0, 0, 1, 0, best, sizeof(best), &score, &depth) == 0);
	assert(depth == 1 && renegade_push_move(best) == 1);
	assert(renegade_set_startpos() == 0);
	renegade_new_game();
	assert(renegade_search_with_nodes(0, 0, 0, 1, best, sizeof(best), &score, &depth) == 0);
	assert(depth == 1 && renegade_push_move(best) == 1);
	assert(renegade_set_startpos() == 0);
	renegade_new_game();
	assert(renegade_search_with_nodes(4, 0, INT64_C(4294967297), INT64_C(4294967297), best, sizeof(best), &score, &depth) == 0);
	assert(depth == 4);
	std::atomic<bool> done = false;
	int searchStatus = -1;
	std::thread worker([&] {
		searchStatus = renegade_search(0, 30000, best, sizeof(best), &score, &depth);
		done.store(true);
	});
	const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
	while (!done.load() && std::chrono::steady_clock::now() < deadline) {
		renegade_stop_search();
		std::this_thread::yield();
	}
	assert(done.load());
	worker.join();
	assert(searchStatus == 0 && depth >= 1);
	assert(renegade_push_move(best) == 1);

	renegade_set_chess960(1);
	assert(renegade_set_fen("r3k2r/8/8/8/8/8/8/R3K2R w HAha - 0 1") == 0);
	assert(renegade_get_legal_moves(moves, sizeof(moves)) > 0);
	assert(std::strstr(moves, "e1h1") != nullptr);
	assert(renegade_push_move("e1h1") == 1);
	assert(renegade_get_fen(fen, sizeof(fen)) == 0);
	assert(std::strstr(fen, "R4RK1") != nullptr);
	renegade_shutdown();
	assert(renegade_init() == 0);
	renegade_shutdown();
}
