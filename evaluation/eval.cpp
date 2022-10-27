#include <iostream>
#include <random>
#include <chrono>
#include <vector>
#include <string>
#include <fstream>
#include <sstream>
#include <iomanip>

using namespace std;

#define N_EVAL 10

#define STEP 256
#define STEP2 128
#define SCORE_MAX 64

struct Board{
    uint64_t player;
    uint64_t opponent;
};

struct Datum{
    Board board;
    int value;
};

int eval[N_EVAL];
const uint64_t eval_masks[N_EVAL] = {
    0x8100000000000081ULL, 
    0x4281000000008142ULL, 
    0x2400810000810024ULL, 
    0x1800008181000018ULL, 
    0x0042000000004200ULL, 
    0x0024420000422400ULL, 
    0x0018004242001800ULL, 
    0x0000240000240000ULL, 
    0x0000182424180000ULL, 
    0x0000001818000000ULL
};

vector<Datum> data;

inline uint64_t tim(){
    return chrono::duration_cast<chrono::milliseconds>(chrono::high_resolution_clock::now().time_since_epoch()).count();
}

mt19937 raw_myrandom(tim());

inline double myrandom(){
    return (double)raw_myrandom() / mt19937::max();
}

inline int32_t myrandrange(int32_t s, int32_t e){
    return s +(int)((e - s) * myrandom());
}

inline int pop_count_ull(uint64_t x) {
	x = x - ((x >> 1) & 0x5555555555555555ULL);
	x = (x & 0x3333333333333333ULL) + ((x >> 2) & 0x3333333333333333ULL);
	x = (x + (x >> 4)) & 0x0F0F0F0F0F0F0F0FULL;
	x = (x * 0x0101010101010101ULL) >> 56;
	return x;
}

int evaluate(Board board){
    int res = 0;
    for (int i =0 ; i < N_EVAL; ++i){
        res += eval[i] * pop_count_ull(board.player & eval_masks[i]);
        res -= eval[i] * pop_count_ull(board.opponent & eval_masks[i]);
    }
    res += res > 0 ? STEP2 : (res < 0 ? -STEP2 : 0);
    res /= STEP;
    return max(-SCORE_MAX, min(SCORE_MAX, res));
}

void init(){
    for (int i = 0; i < N_EVAL; ++i)
        eval[i] = myrandrange(-STEP * SCORE_MAX, STEP * SCORE_MAX + 1);
}

Datum create_datum(string line){
    Datum datum;
    datum.board.player = 0ULL;
    datum.board.opponent = 0ULL;
    for (int i = 0; i < 64; ++i){
        if (line[i] == '0')
            datum.board.player |= 1ULL << (63 - i);
        else if (line[i] == '1')
            datum.board.opponent |= 1ULL << (63 - i);
    }
    datum.value = stoi(line.substr(67));
    if (line[65] == '1'){
        swap(datum.board.player, datum.board.opponent);
        datum.value *= -1;
    }
    return datum;
}

void input_data(int use_num){
    for (int num = 0; num < use_num; ++num){
        ostringstream ss;
        ss << setw(7) << setfill('0') << num;
        string file(ss.str());
        file = "data/" + file + ".txt";
        cerr << file << endl;
        ifstream ifs(file);
        if (!ifs){
            cerr << "error opening " << file << endl;
            continue;
        }
        string line;
        while (true){
            getline(ifs, line);
            if (line.size() < 67)
                break;
            data.emplace_back(create_datum(line));
        }
    }
}

double scoring(double *mae){
    int score;
    double error;
    double res = 0.0;
    *mae = 0.0;
    for (Datum &datum: data){
        score = evaluate(datum.board);
        error = (double)abs(score - datum.value) / (SCORE_MAX * 2);
        res += error * error;
        *mae += (double)abs(score - datum.value);
    }
    *mae /= data.size();
    return res / data.size();
}

void hillclimb(uint64_t tl){
    double mae;
    double f_score = scoring(&mae), n_score;
    cerr << f_score << " " << mae << endl;
    uint64_t strt = tim();
    int f_val, idx;
    while (tim() - strt < tl){
        idx = myrandrange(0, N_EVAL);
        f_val = eval[idx];
        while (eval[idx] == f_val)
            eval[idx] += myrandrange(-STEP * 5, STEP * 5 + 1);
        n_score = scoring(&mae);
        if (f_score >= n_score){
            f_score = n_score;
            cerr << "\r" << (tim() - strt) * 100 / tl << " " << f_score << " " << mae << "                          ";
        } else
            eval[idx] = f_val;
    }
    cerr << "\r" << f_score << " " << mae << "                          " << endl;
}

int main(int argc, char* argv[]){
    init();
    input_data(atoi(argv[1]));
    cerr << "data size " << data.size() << endl;
    hillclimb((uint64_t)atoi(argv[2]) * 60 * 1000);
    for (int i = 0; i < N_EVAL; ++i)
        cerr << eval[i] << ", ";
    cerr << endl;
    return 0;
}