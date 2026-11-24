#include "RenegadeBridge.h"

#include "../Neural.h"
#include "../Search.h"
#include <array>
#include <atomic>
#include <charconv>
#include <cstring>
#include <memory>
#include <mutex>

void GenerateMagicTables();

namespace {
	std::mutex mutex;
	std::once_flag initialization;
	std::unique_ptr<Position> position;
	std::unique_ptr<Search> search;
	std::atomic<bool> stopRequested = false;
	std::atomic<bool> searching = false;

	bool Ready() {
		if (position && search) return true;
		std::cerr << "Renegade bridge: engine is not initialized" << std::endl;
		return false;
	}

	bool ParseNumber(const std::string& value, int min, int max) {
		int number = 0;
		const auto result = std::from_chars(value.data(), value.data() + value.size(), number);
		return result.ec == std::errc{} && result.ptr == value.data() + value.size() && number >= min && number <= max;
	}

	bool ValidateFen(const std::vector<std::string>& parts) {
		if (parts.size() != 4 && parts.size() != 6) return false;
		std::array<char, 64> board{};
		int rank = 7, file = 0, whiteKing = -1, blackKing = -1;
		for (char ch : parts[0]) {
			if (ch == '/') {
				if (file != 8 || rank == 0) return false;
				rank--;
				file = 0;
			}
			else if (ch >= '1' && ch <= '8') {
				file += ch - '0';
				if (file > 8) return false;
			}
			else {
				if (std::string_view("PNBRQKpnbrqk").find(ch) == std::string_view::npos || file >= 8) return false;
				if ((ch == 'P' || ch == 'p') && (rank == 0 || rank == 7)) return false;
				const int square = rank * 8 + file++;
				board[square] = ch;
				if (ch == 'K') {
					if (whiteKing != -1) return false;
					whiteKing = square;
				}
				if (ch == 'k') {
					if (blackKing != -1) return false;
					blackKing = square;
				}
			}
		}
		if (rank != 0 || file != 8 || whiteKing == -1 || blackKing == -1) return false;
		if (std::abs(whiteKing % 8 - blackKing % 8) <= 1 && std::abs(whiteKing / 8 - blackKing / 8) <= 1) return false;
		if (parts[1] != "w" && parts[1] != "b") return false;

		bool rights[4]{};
		if (parts[2] != "-") {
			for (char ch : parts[2]) {
				int side = -1, rook = -1, king = -1;
				if (!Settings::Chess960) {
					if (ch == 'K') { side = 0; rook = 7; king = 4; }
					else if (ch == 'Q') { side = 1; rook = 0; king = 4; }
					else if (ch == 'k') { side = 2; rook = 63; king = 60; }
					else if (ch == 'q') { side = 3; rook = 56; king = 60; }
				}
				else if (ch >= 'A' && ch <= 'H') {
					rook = ch - 'A';
					king = whiteKing;
					if (king / 8 == 0 && rook != king) side = rook > king ? 0 : 1;
				}
				else if (ch >= 'a' && ch <= 'h') {
					rook = 56 + ch - 'a';
					king = blackKing;
					if (king / 8 == 7 && rook != king) side = rook > king ? 2 : 3;
				}
				if (side < 0 || rights[side] || board[king] != (side < 2 ? 'K' : 'k')
					|| board[rook] != (side < 2 ? 'R' : 'r')) return false;
				rights[side] = true;
			}
		}

		if (parts[3] != "-") {
			const std::string& ep = parts[3];
			if (ep.size() != 2 || ep[0] < 'a' || ep[0] > 'h'
				|| ep[1] != (parts[1] == "w" ? '6' : '3')) return false;
			const int square = (ep[1] - '1') * 8 + ep[0] - 'a';
			const int pawnSquare = square + (parts[1] == "w" ? -8 : 8);
			const int originSquare = square + (parts[1] == "w" ? 8 : -8);
			if (board[square] || board[originSquare] || board[pawnSquare] != (parts[1] == "w" ? 'p' : 'P')) return false;
		}
		return parts.size() == 4 || (ParseNumber(parts[4], 0, 255) && ParseNumber(parts[5], 1, 65535));
	}

	bool ValidMoveString(const char* move) {
		if (!move) return false;
		const std::string_view value(move);
		if (value.size() != 4 && value.size() != 5) return false;
		if (value[0] < 'a' || value[0] > 'h' || value[1] < '1' || value[1] > '8'
			|| value[2] < 'a' || value[2] > 'h' || value[3] < '1' || value[3] > '8') return false;
		return value.size() == 4 || std::string_view("qrbn").find(value[4]) != std::string_view::npos;
	}

	bool CopyString(const std::string& value, char* buffer, int size) {
		if (!buffer || size <= 0 || value.size() >= static_cast<size_t>(size)) return false;
		std::memcpy(buffer, value.c_str(), value.size() + 1);
		return true;
	}
}

int renegade_init() {
	std::lock_guard lock(mutex);
	if (position && search) return 0;
	std::call_once(initialization, [] { GenerateMagicTables(); LoadDefaultNetwork(); });
	Settings::Chess960 = false;
	position = std::make_unique<Position>();
	search = std::make_unique<Search>();
	search->TranspositionTable.SetSize(Settings::Hash, 1);
	return 0;
}

void renegade_shutdown() {
	std::lock_guard lock(mutex);
	if (!Ready()) return;
	search->StopThreads();
	search.reset();
	position.reset();
}

void renegade_set_hash_mb(int megabytes) {
	std::lock_guard lock(mutex);
	if (!Ready()) return;
	if (megabytes < HashMin || megabytes > HashMax) {
		std::cerr << "Renegade bridge: invalid hash size" << std::endl;
		return;
	}
	Settings::Hash = megabytes;
	search->TranspositionTable.SetSize(megabytes, 1);
}

void renegade_set_chess960(int enabled) {
	std::lock_guard lock(mutex);
	if (!Ready()) return;
	if (enabled != 0 && enabled != 1) {
		std::cerr << "Renegade bridge: invalid Chess960 setting" << std::endl;
		return;
	}
	Settings::Chess960 = enabled != 0;
}

void renegade_new_game() {
	std::lock_guard lock(mutex);
	if (Ready()) search->ResetState(true);
}

int renegade_set_startpos() {
	std::lock_guard lock(mutex);
	if (!Ready()) return -1;
	*position = Settings::Chess960 ? Position(518, 518) : Position();
	return 0;
}

int renegade_set_fen(const char* fen) {
	if (!fen) return -1;
	std::lock_guard lock(mutex);
	if (!Ready()) return -1;
	const std::vector<std::string> parts = Split(fen);
	if (!ValidateFen(parts)) return -1;
	*position = Position(parts.size() == 4 ? std::string(fen) + " 0 1" : std::string(fen));
	return 0;
}

int renegade_push_move(const char* move) {
	if (!ValidMoveString(move)) return 0;
	std::lock_guard lock(mutex);
	if (!Ready()) return 0;
	MoveList moves{};
	position->GenerateAllLegalMoves(moves);
	for (const ScoredMove& legal : moves) {
		if (legal.move.ToString(Settings::Chess960) == move) {
			position->PushMove(legal.move);
			return 1;
		}
	}
	return 0;
}

int renegade_get_fen(char* buffer, int buffer_size) {
	std::lock_guard lock(mutex);
	if (!Ready()) return -1;
	return CopyString(position->GetFEN(), buffer, buffer_size) ? 0 : -1;
}

int renegade_get_legal_moves(char* buffer, int buffer_size) {
	std::lock_guard lock(mutex);
	if (!Ready()) return -1;
	MoveList moves{};
	position->GenerateAllLegalMoves(moves);
	std::string output;
	for (const ScoredMove& move : moves) {
		if (!output.empty()) output += ' ';
		output += move.move.ToString(Settings::Chess960);
	}
	return CopyString(output, buffer, buffer_size) ? static_cast<int>(moves.size()) : -1;
}

int renegade_get_game_state() {
	std::lock_guard lock(mutex);
	if (!Ready()) return -1;
	return static_cast<int>(position->GetGameState());
}

int renegade_search(int depth, int movetime_ms, char* best_move, int buffer_size, int* score_cp, int* depth_reached) {
	return renegade_search_with_nodes(depth, movetime_ms, 0, 0, best_move, buffer_size, score_cp, depth_reached);
}

int renegade_search_with_nodes(int depth, int movetime_ms, int64_t nodes, int64_t softnodes,
	char* best_move, int buffer_size, int* score_cp, int* depth_reached) {
	if (!best_move || buffer_size < 6 || !score_cp || !depth_reached || depth < 0 || depth > MaxDepth
		|| movetime_ms < 0 || nodes < 0 || softnodes < 0
		|| (depth == 0 && movetime_ms == 0 && nodes == 0 && softnodes == 0)) return -1;
	std::lock_guard lock(mutex);
	if (!Ready()) return -1;
	MoveList moves{};
	position->GenerateAllLegalMoves(moves);
	if (moves.size() == 0) return -1;
	SearchParams params{};
	params.depth = depth;
	params.movetime = movetime_ms;
	params.nodes = nodes;
	params.softnodes = softnodes;
	stopRequested.store(false);
	searching.store(true);
	const Results results = search->SearchSinglethreaded(*position, params, &stopRequested);
	searching.store(false);
	if (results.BestMove().IsNull() || !CopyString(results.BestMove().ToString(Settings::Chess960), best_move, buffer_size))
		return -1;
	*score_cp = ToCentipawns(results.score, results.ply);
	*depth_reached = results.depth;
	return 0;
}

int renegade_static_eval_cp() {
	std::lock_guard lock(mutex);
	if (!Ready()) return 0;
	return ToCentipawns(NeuralEvaluate(*position), position->GetPly());
}

void renegade_stop_search() {
	if (searching.load()) stopRequested.store(true);
}
