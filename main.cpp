#include <iostream>
#include <string>
#include "channel.h"
using namespace std;

mutex m;

const int P2 = 7;
const int SIZE = (1 << P2);

// randomly generate two matrixes of size SIZE
void Input(State* st) {
	for(size_t iMatrix = 0;iMatrix < 2;iMatrix++) {
		put<size_t>(SIZE, st->outputs[0]);
		
		cout << "Matrix " << iMatrix << ":" << endl;
		for(size_t iRow = 0;iRow < SIZE;iRow++) {
			for(size_t iCol = 0;iCol < SIZE;iCol++) {
				int value = (rand() % 3) - 1;
				put<int>(value, st->outputs[0]);
				cout << value << " ";
			}
			cout << endl;
		}
	}
	
	st->continuation = nullptr;
}

// Load matrix from channel 
void Load_loop(State* st) {
	size_t chan = get<size_t>("chan", st);
	
	if(get_ready<int>(st->inputs[chan])) {
		size_t lig = get<size_t>("lig", st);
		size_t col = get<size_t>("col", st);
		size_t size = get<size_t>("size", st);
		size_t id = get<size_t>("mat_id", st);
		
		push<int>("mat" + to_string(id), get<int>(st->inputs[chan]), st);
		
		col++;
		
		if(col == size) {
			col = 0;
			lig++;
		}
		
		if(lig == size) {
			pop<size_t>("mat_id", st);
			pop<size_t>("chan", st);
			st->continuation = pop<function<void(State*)>>("f_ptr", st);
		}
		
		put<size_t>("col", col, st);
		put<size_t>("lig", lig, st);
	}
}

void Load(State* st) {
	size_t chan = get<size_t>("chan", st);
	
	if(get_ready<size_t>(st->inputs[chan])) {
		size_t sz = get<size_t>(st->inputs[chan]);
		put<size_t>("size", sz, st);
		put<size_t>("col", 0, st);
		put<size_t>("lig", 0, st);

		st->continuation = Load_loop;
	}
}

// Put matrix to channel
void Store(State* st) {
	size_t id = get<size_t>("mat_id", st);
	
	size_t sz = get<size_t>("size", st);
	put<size_t>(sz, st->outputs[0]);
	
	for(size_t iRow = 0;iRow < sz;iRow++) {
		for(size_t iCol = 0;iCol < sz;iCol++) {
			put<int>(pop<int>("mat" + to_string(id), st), st->outputs[0]);
		}
	}
	
	st->continuation = nullptr;
}

// Put matrix to standard output
void Output(State* st) {
	size_t sz = get<size_t>("size", st);
	size_t id = pop<size_t>("mat_id", st);
	
	cout << "Matrix " << id << " : " << endl;
	for(size_t iLig = 0;iLig < sz;iLig++) {
		for(size_t iCol = 0;iCol < sz;iCol++) {
			cout << pop<int>("mat" + to_string(id), st) << " ";
		}
		cout << endl;
	}
	
	st->continuation = pop<function<void(State*)>>("f_ptr", st);
}

// Slow multiplication of two matrixes
void SlowMultiply(State* st) {
	size_t sz = get<size_t>("size", st);
	size_t idA = pop<size_t>("mat_id", st);
	size_t idB = pop<size_t>("mat_id", st);
	size_t idC = pop<size_t>("mat_id", st);

	vector<vector<int>> matA(sz, vector<int>(sz, 0)), matB(sz, vector<int>(sz, 0));
	for(size_t iLig = 0;iLig < sz;iLig++) {
		for(size_t iCol = 0;iCol < sz;iCol++) {
			matA[iLig][iCol] = pop<int>("mat" + to_string(idA), st);
			matB[iLig][iCol] = pop<int>("mat" + to_string(idB), st);
		}
	}
	
	vector<vector<int>> res(sz, vector<int>(sz, 0));
	
	for(size_t iLig = 0;iLig < sz;iLig++) {
		for(size_t iCol = 0;iCol < sz;iCol++) {
			int tot = 0;
			for(size_t iInter = 0;iInter < sz;iInter++) {
				tot += matA[iLig][iInter] * matB[iInter][iCol];
			}
			
			push<int>("mat" + to_string(idC), tot, st);
		}
	}
	
	st->continuation = pop<function<void(State*)>>("f_ptr", st);
}

void FastMultiply_merge(State* st);

// Multiplication of two matrices using a divide and conquer approach
void FastMultiply(State* st) {
	size_t sz = get<size_t>("size", st);
	size_t idA = pop<size_t>("mat_id", st);
	size_t idB = pop<size_t>("mat_id", st);
	
	put<size_t>("tsize", sz, st);
	
	vector<vector<int>> matA(sz, vector<int>(sz, 0)), matB(sz, vector<int>(sz, 0));
	
	for(size_t iLig = 0;iLig < sz;iLig++) {
		for(size_t iCol = 0;iCol < sz;iCol++) {
			matA[iLig][iCol] = pop<int>("mat" + to_string(idA), st);
			matB[iLig][iCol] = pop<int>("mat" + to_string(idB), st);
		}
	}
	
	vector<vector<int>> mats(8);
	
	size_t hlf = sz / 2;
	for(size_t iL = 0;iL < 2;iL++) {
		for(size_t iC = 0;iC < 2;iC++) {
			size_t idMat = 4 * iL + 2 * iC;
			
			for(size_t iLig = iL * hlf;iLig < (iL + 1) * hlf;iLig++) {
				for(size_t iCol = iC * hlf;iCol < (iC + 1) * hlf;iCol++) {
					mats[idMat].push_back(matA[iLig][iCol]);
					mats[idMat + 1].push_back(matB[iLig][iCol]);
				}
			}
		}
	}
	
	push_front<function<void(State*)>>("f_ptr", FastMultiply_merge, st);
	
	for(size_t iL = 0;iL < 2;iL++) {
		for(size_t iC = 0;iC < 2;iC++) {
			for(size_t iI = 0;iI < 2;iI++) {
				size_t idA = 4 * iL + 2 * iI;
				size_t idB = 4 * iI + 2 * iC + 1;

				Channel *in = new Channel(), *out = new Channel();
				st->inputs.push_back(out);
				st->outputs.push_back(in);
				
				put<size_t>(hlf, in);
				for(int val : mats[idA]) {
					put<int>(val, in);
				}
				
				put<size_t>(hlf, in);
				for(int val : mats[idB]) {
					put<int>(val, in);
				}
				
				push_front<function<void(State*)>>("f_ptr", Load, st);
				push_front<size_t>("mat_id", 16 + 4 * iL + 2 * iC + iI, st);
				push_front<size_t>("chan", st->inputs.size() - 1, st);
				
				State* nst = new State({in}, {out}, Load);
				push<function<void(State*)>>("f_ptr", Load, nst);
				if(sz <= 64)
					push<function<void(State*)>>("f_ptr", SlowMultiply, nst);
				else
					push<function<void(State*)>>("f_ptr", FastMultiply, nst);
				
				push<function<void(State*)>>("f_ptr", Store, nst);
				
				push<size_t>("chan", 0, nst);
				push<size_t>("chan", 0, nst);
				
				push<size_t>("mat_id", 0, nst);
				push<size_t>("mat_id", 1, nst);
				push<size_t>("mat_id", 0, nst);
				push<size_t>("mat_id", 1, nst);
				push<size_t>("mat_id", 2, nst);
				push<size_t>("mat_id", 2, nst);
				doco(*nst);
			}
		}
	}
	
	st->continuation = pop<function<void(State*)>>("f_ptr", st);
}

void FastMultiply_merge(State* st) {
	size_t sz = get<size_t>("tsize", st);
	size_t idC = pop<size_t>("mat_id", st);
	
	put<size_t>("size", sz, st);
	
	size_t hlf = sz / 2;
	 
	vector<vector<int>> res(sz, vector<int>(sz, 0));
	
	for(size_t iL = 0;iL < 2;iL++) {
		for(size_t iC = 0;iC < 2;iC++) {
			for(size_t iI = 0;iI < 2;iI++) {
				size_t id = 16 + 4 * iL + 2 * iC + iI;	
			
				for(size_t iLig = iL * hlf;iLig < (iL + 1) * hlf;iLig++) {
					for(size_t iCol = iC * hlf;iCol < (iC + 1) * hlf;iCol++) {
						res[iLig][iCol] += pop<int>("mat" + to_string(id), st);
					}
				}
			}
		}
	}
	
	for(size_t iLig = 0;iLig < sz;iLig++) {
		for(size_t iCol = 0;iCol < sz;iCol++) {
			push<int>("mat" + to_string(idC), res[iLig][iCol], st);
		}
	}
	
	st->continuation = pop<function<void(State*)>>("f_ptr", st);
}

void End(State* st) {
	st->continuation = nullptr;
}

State st_multiply(Channel* in, Channel* out) {
	State* st = new State({in}, {out}, Load);
	push<function<void(State*)>>("f_ptr", Load, st);
	push<function<void(State*)>>("f_ptr", FastMultiply, st);
	push<function<void(State*)>>("f_ptr", Output, st);
	push<function<void(State*)>>("f_ptr", End, st);
	
	push<size_t>("chan", 0, st);
	push<size_t>("chan", 0, st);
	
	push<size_t>("mat_id", 0, st);
	push<size_t>("mat_id", 1, st);
	push<size_t>("mat_id", 0, st);
	push<size_t>("mat_id", 1, st);
	push<size_t>("mat_id", 2, st);
	push<size_t>("mat_id", 2, st);
	return *st;
}

int main() {
	ios_base::sync_with_stdio(false);
	cin.tie(nullptr);
	cout.tie(nullptr);
	
	Channel in, out;
	
	State st = st_multiply(&in, &out);
	
	doco(
		State({}, {&in}, Input),
		st
	);
	
	run(1);
	return 0;
}
