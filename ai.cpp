#include <iostream>

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
};

inline void bit_print_board(uint64_t x){
    for (uint32_t i = 0; i < 64; ++i){
        std::cerr << (1 & (x >> (63 - i)));
        if (i % 8 == 7)
            std::cerr << std::endl;
    }
    std::cerr << std::endl;
}

int main() {
    Board board;
    board.player = 0x0000000800000000ULL;
    board.opponent = 0x002A9C141C480004ULL;
    uint64_t legal = board.get_legal();
    bit_print_board(legal);
    return 0;
}