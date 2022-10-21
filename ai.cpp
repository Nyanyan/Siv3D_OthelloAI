#include <iostream>

#define SCORE_MAX 1000000

inline int pop_count_ull(uint64_t x){
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
            constexpr int shifts[8] = {1, -1, 8, -8, 7, -7, 9, -9};
            constexpr uint64_t masks[4] = {0x7E7E7E7E7E7E7E7EULL, 0x00FFFFFFFFFFFF00ULL, 0x007E7E7E7E7E7E00ULL, 0x007E7E7E7E7E7E00ULL};
            for (int i = 0; i < 8; ++i)
                res |= get_legal_part(shifts[i], masks[i / 2]);
            return res & ~(player | opponent);
        }

        Flip get_flip(uint_fast8_t pos) {
            Flip res;
            res.pos = pos;
            res.flip = 0ULL;
            uint64_t x = 1ULL << pos;
            constexpr int shifts[8] = {1, -1, 8, -8, 7, -7, 9, -9};
            constexpr uint64_t masks[4] = {0x7E7E7E7E7E7E7E7EULL, 0x00FFFFFFFFFFFF00ULL, 0x007E7E7E7E7E7E00ULL, 0x007E7E7E7E7E7E00ULL};
            for (int i = 0; i < 8; ++i)
                res.flip |= get_flip_part(shifts[i], masks[i / 2], x);
            return res;
        }

        void pass(){
            std::swap(player, opponent);
        }

        int evaluate(){
            return evaluate_one_player(player) - evaluate_one_player(opponent);
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

        int evaluate_one_player(uint64_t p){
            // from https://uguisu.skr.jp/othello/5-1.html
            #define N_WEIGHT 6
            constexpr int cell_weight_score[N_WEIGHT] = {30, -12, 0, -1, -3, -15};
            constexpr uint64_t cell_weight_mask[N_WEIGHT] = {
                0b10000001'00000000'00000000'00000000'00000000'00000000'00000000'10000001ULL,
                0b01000010'10000001'00000000'00000000'00000000'00000000'10000001'01000010ULL,
                0b00100100'00000000'10100101'00000000'00000000'10100101'00000000'00100100ULL,
                0b00011000'00000000'00011000'10111101'10111101'00011000'00000000'00011000ULL,
                0b00000000'00111100'01000010'01000010'01000010'01000010'00111100'00000000ULL,
                0b00000000'01000010'00000000'00000000'00000000'00000000'01000010'00000000ULL
            };
            int res = 0;
            for (int i = 0; i < N_WEIGHT; ++i)
                res += cell_weight_score[i] * pop_count_ull(p & cell_weight_mask[i]);
            return res;
        }
};

inline uint_fast8_t ntz(uint64_t *x) {
    return pop_count_ull((~(*x)) & ((*x) - 1));
}

inline uint_fast8_t first_bit(uint64_t *x) {
    return ntz(x);
}

inline uint_fast8_t next_bit(uint64_t *x) {
    *x &= *x - 1;
    return ntz(x);
}

int nega_alpha(Board board, int depth, int alpha, int beta, bool passed) {
    if (depth == 0)
        return board.evaluate();
    uint64_t legal = board.get_legal();
    if (legal == 0ULL) {
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

AI_result ai(Board board, int depth){
    AI_result res = {-1, -SCORE_MAX};
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
        if (res.val < v){
            res.pos = cell;
            res.val = v;
        }
    }
    return res;
}





inline void bit_print_board(uint64_t x) {
    for (uint32_t i = 0; i < 64; ++i) {
        std::cerr << (1 & (x >> (63 - i)));
        if (i % 8 == 7)
            std::cerr << std::endl;
    }
    std::cerr << std::endl;
}

Board input_board(){
    Board res;
    char elem;
    int player;
    std::cin >> player;
    res.player = 0;
    res.opponent = 0;
    for (int i = 0; i < 64; ++i){
        std::cin >> elem;
        if (elem == '0'){
            if (player == 0)
                res.player |= 1ULL << (63 - i);
            else
                res.opponent |= 1ULL << (63 - i);
        } else if (elem == '1'){
            if (player == 1)
                res.player |= 1ULL << (63 - i);
            else
                res.opponent |= 1ULL << (63 - i);
        }
    }
    return res;
}


std::string idx_to_coord(int idx){
    int y = 7 - idx / 8;
    int x = 7 - idx % 8;
    const std::string x_coord = "abcdefgh";
    return x_coord[x] + std::to_string(y + 1);
}

int main() {
    Board board = input_board();
    AI_result result = ai(board, 10);
    std::cerr << idx_to_coord(result.pos) << " " << result.val << std::endl;
    return 0;
}
