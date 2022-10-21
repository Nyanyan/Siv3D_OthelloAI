# include <Siv3D.hpp> // OpenSiv3D v0.6.5
#include <iostream>
#include <future>
#include <chrono>

#define SCORE_MAX 1000000

inline int pop_count_ull(uint64_t x) {
	x = x - ((x >> 1) & 0x5555555555555555ULL);
	x = (x & 0x3333333333333333ULL) + ((x >> 2) & 0x3333333333333333ULL);
	x = (x + (x >> 4)) & 0x0F0F0F0F0F0F0F0FULL;
	x = (x * 0x0101010101010101ULL) >> 56;
	return x;
}

struct AI_result {
	int pos;
	int val;
};

struct Flip {
	uint64_t flip;
	uint_fast8_t pos;
};

class Board {
public:
	uint64_t player;
	uint64_t opponent;

public:
	void reset() {
		player = 0x0000000810000000ULL;
		opponent = 0x0000001008000000ULL;
	}

	void move(Flip flip) {
		player ^= flip.flip;
		opponent ^= flip.flip;
		player ^= 1ULL << flip.pos;
		std::swap(player, opponent);
	}

	void undo(Flip flip) {
		std::swap(player, opponent);
		player ^= 1ULL << flip.pos;
		player ^= flip.flip;
		opponent ^= flip.flip;
	}

	uint64_t get_legal() {
		uint64_t res = 0ULL;
		constexpr int shifts[8] = { 1, -1, 8, -8, 7, -7, 9, -9 };
		constexpr uint64_t masks[4] = { 0x7E7E7E7E7E7E7E7EULL, 0x00FFFFFFFFFFFF00ULL, 0x007E7E7E7E7E7E00ULL, 0x007E7E7E7E7E7E00ULL };
		for (int i = 0; i < 8; ++i)
			res |= get_legal_part(shifts[i], masks[i / 2]);
		return res & ~(player | opponent);
	}

	Flip get_flip(uint_fast8_t pos) {
		Flip res;
		res.pos = pos;
		res.flip = 0ULL;
		uint64_t x = 1ULL << pos;
		constexpr int shifts[8] = { 1, -1, 8, -8, 7, -7, 9, -9 };
		constexpr uint64_t masks[4] = { 0x7E7E7E7E7E7E7E7EULL, 0x00FFFFFFFFFFFF00ULL, 0x007E7E7E7E7E7E00ULL, 0x007E7E7E7E7E7E00ULL };
		for (int i = 0; i < 8; ++i)
			res.flip |= get_flip_part(shifts[i], masks[i / 2], x);
		return res;
	}

	void pass() {
		std::swap(player, opponent);
	}

	int evaluate() {
		return evaluate_one_player(player) - evaluate_one_player(opponent);
	}

	int get_score() {
		int p = pop_count_ull(player);
		int o = pop_count_ull(opponent);
		int v = 64 - p - o;
		return p > o ? p - o + v : p - o - v;
	}

private:
	uint64_t enhanced_shift(uint64_t a, int b) {
		if (b >= 0)
			return a << b;
		return a >> (-b);
	}

	uint64_t get_legal_part(int shift, uint64_t mask) {
		uint64_t o = opponent & mask;
		uint64_t l = o & enhanced_shift(player, shift);
		for (int i = 0; i < 5; ++i)
			l |= o & enhanced_shift(l, shift);
		return enhanced_shift(l, shift);
	}

	uint64_t get_flip_part(int shift, uint64_t mask, uint64_t x) {
		uint64_t o = opponent & mask;
		uint64_t f = enhanced_shift(x, shift) & o;
		uint64_t nf;
		bool flipped = false;
		for (int i = 0; i < 8; ++i) {
			nf = enhanced_shift(f, shift);
			if (nf & player) {
				flipped = true;
				break;
			}
			f |= nf & o;
		}
		if (!flipped)
			f = 0ULL;
		return f;
	}

	int evaluate_one_player(uint64_t p) {
		// from https://uguisu.skr.jp/othello/5-1.html
		constexpr int cell_weight_score[6] = { 30, -12, 0, -1, -3, -15 };
		constexpr uint64_t cell_weight_mask[6] = {
			0b10000001'00000000'00000000'00000000'00000000'00000000'00000000'10000001ULL,
			0b01000010'10000001'00000000'00000000'00000000'00000000'10000001'01000010ULL,
			0b00100100'00000000'10100101'00000000'00000000'10100101'00000000'00100100ULL,
			0b00011000'00000000'00011000'10111101'10111101'00011000'00000000'00011000ULL,
			0b00000000'00111100'01000010'01000010'01000010'01000010'00111100'00000000ULL,
			0b00000000'01000010'00000000'00000000'00000000'00000000'01000010'00000000ULL
		};
		int res = 0;
		for (int i = 0; i < 6; ++i)
			res += cell_weight_score[i] * pop_count_ull(p & cell_weight_mask[i]);
		return res;
	}
};

struct Rich_board {
	Board board;
	int player;
	bool game_over;

	Rich_board() {
		reset();
	}

	void reset() {
		board.reset();
		player = 0;
		game_over = false;
	}

	void move(uint_fast8_t pos) {
		Flip flip = board.get_flip(pos);
		board.move(flip);
		player ^= 1;
		if (board.get_legal() == 0ULL) {
			board.pass();
			player ^= 1;
			if (board.get_legal() == 0ULL)
				game_over = true;
		}
	}
};


inline uint_fast8_t ntz(uint64_t* x) {
	return pop_count_ull((~(*x)) & ((*x) - 1));
}

inline uint_fast8_t first_bit(uint64_t* x) {
	return ntz(x);
}

inline uint_fast8_t next_bit(uint64_t* x) {
	*x &= *x - 1;
	return ntz(x);
}

bool global_searching = true;

int nega_alpha(Board board, int depth, int alpha, int beta, bool passed) {
	if (!global_searching)
		return -SCORE_MAX;
	if (depth <= 0)
		return board.evaluate();
	uint64_t legal = board.get_legal();
	if (legal == 0ULL) {
		if (passed)
			return board.get_score();
		board.pass();
		return -nega_alpha(board, depth, -beta, -alpha, true);
	}
	Flip flip;
	for (uint_fast8_t cell = first_bit(&legal); legal; cell = next_bit(&legal)) {
		flip = board.get_flip(cell);
		board.move(flip);
		alpha = std::max(alpha, -nega_alpha(board, depth - 1, -beta, -alpha, false));
		board.undo(flip);
		if (beta <= alpha)
			break;
	}
	return alpha;
}

AI_result ai(Board board, int depth) {
	AI_result res = { -1, -SCORE_MAX };
	uint64_t legal = board.get_legal();
	if (legal == 0ULL) {
		std::cerr << "[ERROR] no legal move" << std::endl;
		return res;
	}
	int v;
	Flip flip;
	for (uint_fast8_t cell = first_bit(&legal); legal; cell = next_bit(&legal)) {
		flip = board.get_flip(cell);
		board.move(flip);
		v = -nega_alpha(board, depth - 1, -SCORE_MAX, -res.val, false);
		board.undo(flip);
		if (res.val < v) {
			res.pos = cell;
			res.val = v;
		}
	}
	return res;
}

#define BOARD_SIZE 400
#define BOARD_COORD_SIZE 20
#define DISC_SIZE 20
#define LEGAL_SIZE 7
#define STABLE_SIZE 4
#define BOARD_CELL_FRAME_WIDTH 2
#define BOARD_DOT_SIZE 5
#define BOARD_ROUND_FRAME_WIDTH 10
#define BOARD_ROUND_DIAMETER 20
#define BOARD_SY 60
constexpr int BOARD_SX = 20 + BOARD_COORD_SIZE;
constexpr int BOARD_CELL_SIZE = BOARD_SIZE / 8;

void draw_board(Font font, Font font_bold, Rich_board board) {
	String coord_x = U"abcdefgh";
	for (int i = 0; i < 8; ++i) {
		font_bold(i + 1).draw(15, Arg::center(BOARD_SX - BOARD_COORD_SIZE, BOARD_SY + BOARD_CELL_SIZE * i + BOARD_CELL_SIZE / 2), Color(51, 51, 51));
		font_bold(coord_x[i]).draw(15, Arg::center(BOARD_SX + BOARD_CELL_SIZE * i + BOARD_CELL_SIZE / 2, BOARD_SY - BOARD_COORD_SIZE - 2), Color(51, 51, 51));
	}
	for (int i = 0; i < 7; ++i) {
		Line(BOARD_SX + BOARD_CELL_SIZE * (i + 1), BOARD_SY, BOARD_SX + BOARD_CELL_SIZE * (i + 1), BOARD_SY + BOARD_SIZE).draw(BOARD_CELL_FRAME_WIDTH, Color(51, 51, 51));
		Line(BOARD_SX, BOARD_SY + BOARD_CELL_SIZE * (i + 1), BOARD_SX + BOARD_SIZE, BOARD_SY + BOARD_CELL_SIZE * (i + 1)).draw(BOARD_CELL_FRAME_WIDTH, Color(51, 51, 51));
	}
	Circle(BOARD_SX + 2 * BOARD_CELL_SIZE, BOARD_SY + 2 * BOARD_CELL_SIZE, BOARD_DOT_SIZE).draw(Color(51, 51, 51));
	Circle(BOARD_SX + 2 * BOARD_CELL_SIZE, BOARD_SY + 6 * BOARD_CELL_SIZE, BOARD_DOT_SIZE).draw(Color(51, 51, 51));
	Circle(BOARD_SX + 6 * BOARD_CELL_SIZE, BOARD_SY + 2 * BOARD_CELL_SIZE, BOARD_DOT_SIZE).draw(Color(51, 51, 51));
	Circle(BOARD_SX + 6 * BOARD_CELL_SIZE, BOARD_SY + 6 * BOARD_CELL_SIZE, BOARD_DOT_SIZE).draw(Color(51, 51, 51));
	RoundRect(BOARD_SX, BOARD_SY, BOARD_SIZE, BOARD_SIZE, 20).drawFrame(0, 10, Palette::White);
	const Color colors[2] = { Palette::Black, Palette::White };
	uint64_t legal = board.board.get_legal();
	for (int cell = 0; cell < 64; ++cell) {
		int x = BOARD_SX + (cell % 8) * BOARD_CELL_SIZE + BOARD_CELL_SIZE / 2;
		int y = BOARD_SY + (cell / 8) * BOARD_CELL_SIZE + BOARD_CELL_SIZE / 2;
		if (1 & (board.board.player >> (63 - cell))) {
			Circle(x, y, DISC_SIZE).draw(colors[board.player]);
		}
		else if (1 & (board.board.opponent >> (63 - cell))) {
			Circle(x, y, DISC_SIZE).draw(colors[board.player ^ 1]);
		}
		else if (1 & (legal >> (63 - cell))) {
			Circle(x, y, LEGAL_SIZE).draw(Palette::Cyan);
		}
	}
}

void interact_move(Rich_board *board) {
	uint64_t legal = board->board.get_legal();
	for (int_fast8_t cell = 0; cell < 64; ++cell) {
		if (1 & (legal >> (63 - cell))) {
			int x = cell % 8;
			int y = cell / 8;
			Rect cell_rect(BOARD_SX + x * BOARD_CELL_SIZE, BOARD_SY + y * BOARD_CELL_SIZE, BOARD_CELL_SIZE, BOARD_CELL_SIZE);
			if (cell_rect.leftClicked()) {
				board->move(63 - cell);
			}
		}
	}
}

void ai_move(Rich_board* board, int depth, int *value, std::future<AI_result> *ai_future) {
	if (ai_future->valid()) {
		if (ai_future->wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
			AI_result result = ai_future->get();
			*value = result.val;
			board->move(result.pos);
		}
	}
	else {
		*ai_future = std::async(std::launch::async, ai, board->board, depth);
	}
}

double CalculateScale(const Vec2& baseSize, const Vec2& currentSize) {
	return Min((currentSize.x / baseSize.x), (currentSize.y / baseSize.y));
}

void stop_calculating(std::future<AI_result> *ai_future) {
	if (ai_future->valid()) {
		global_searching = false;
		ai_future->get();
		global_searching = true;
	}
}

void Main() {
	Size window_size = Size(800, 500);
	Window::Resize(window_size);
	Window::SetStyle(WindowStyle::Sizable);
	Scene::SetResizeMode(ResizeMode::Virtual);
	Window::SetTitle(U"Siv3D サンプル オセロAI");
	Scene::SetBackground(Color(36, 153, 114));
	const Font font{ FontMethod::MSDF, 50 };
	const Font font_bold{ FontMethod::MSDF, 50, Typeface::Bold };
	Rich_board board;
	double depth = 5;
	int value = 0;
	int ai_player = -1;
	std::future<AI_result> ai_future;

	while (System::Update()) {
		const double scale = CalculateScale(window_size, Scene::Size());
		const Transformer2D screenScaling{ Mat3x2::Scale(scale), TransformCursor::Yes };
		draw_board(font, font_bold, board);
		if (!board.game_over) {
			if (board.player == ai_player) {
				ai_move(&board, round(depth), &value, &ai_future);
			}
			else {
				interact_move(&board);
			}
		}
		SimpleGUI::Slider(U"先読み{}手"_fmt(round(depth)), depth, 1, 13, Vec2{ 470, 10 }, 150, 150);
		if (SimpleGUI::Button(U"AI先手(黒)で対局", Vec2(470, 60))) {
			stop_calculating(&ai_future);
			board.reset();
			ai_player = 0;
		}
		else if (SimpleGUI::Button(U"AI後手(白)で対局", Vec2(470, 100))) {
			stop_calculating(&ai_future);
			board.reset();
			ai_player = 1;
		}
		if (!board.game_over) {
			if (board.player == 0)
				font(U"黒番").draw(20, Vec2{ 470, 140 });
			else
				font(U"白番").draw(20, Vec2{ 470, 140 });
			if (board.player == ai_player) {
				font(U"AIの手番").draw(20, Vec2{ 470, 180 });
			}
			else if (board.player == 1 - ai_player) {
				font(U"あなたの手番").draw(20, Vec2{ 470, 180 });
			}
		}
		else
			font(U"終局").draw(20, Vec2{ 500, 140 });
		int black_score = pop_count_ull(board.board.player), white_score = pop_count_ull(board.board.opponent);
		if (board.player == 1)
			std::swap(black_score, white_score);
		Circle(480, 230, 12).draw(Palette::Black);
		Circle(600, 230, 12).draw(Palette::White);
		font(black_score).draw(20, Arg::leftCenter(500, 230));
		font(white_score).draw(20, Arg::rightCenter(580, 230));
		Line(540, 218, 540, 242).draw(2, Color(51, 51, 51));
		font(U"評価値: {}"_fmt(value)).draw(20, Vec2{470, 260});
	}
}
