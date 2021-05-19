#include <string>
#include "channel.h"
using namespace std;

mutex m;

/***************************************/
/*        MATRIX MULTIPLICATION        */
/***************************************/

namespace matrices {
	const int P2 = 8;
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
				st->continuation = pop<void (*)(State*)>("f_ptr", st);
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
		
		st->continuation = pop<void (*)(State*)>("f_ptr", st);
	}

	// Slow multiplication of two matrixes
	void SlowMultiply(State* st) {
		cerr << "SLOW" << endl;
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
		
		st->continuation = pop<void (*)(State*)>("f_ptr", st);
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
		
		push_front<void (*)(State*)>("f_ptr", FastMultiply_merge, st);
		
		for(size_t iL = 0;iL < 2;iL++) {
			for(size_t iC = 0;iC < 2;iC++) {
				for(size_t iI = 0;iI < 2;iI++) {
					size_t idA = 4 * iL + 2 * iI;
					size_t idB = 4 * iI + 2 * iC + 1;

					size_t in = new_channel(), out = new_channel();
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
					
					push_front<void (*)(State*)>("f_ptr", Load, st);
					push_front<size_t>("mat_id", 16 + 4 * iL + 2 * iC + iI, st);
					push_front<size_t>("chan", st->inputs.size() - 1, st);
					
					State* nst = new State({in}, {out}, Load);
					push<void (*)(State*)>("f_ptr", Load, nst);
					if(sz <= 64)
						push<void (*)(State*)>("f_ptr", SlowMultiply, nst);
					else
						push<void (*)(State*)>("f_ptr", FastMultiply, nst);
					
					push<void (*)(State*)>("f_ptr", Store, nst);
					
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
		
		st->continuation = pop<void (*)(State*)>("f_ptr", st);
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
		
		st->continuation = pop<void (*)(State*)>("f_ptr", st);
	}

	void End(State* st) {
		st->continuation = nullptr;
	}

	State st_multiply(size_t in, size_t out) {
		State* st = new State({in}, {out}, Load);
		push<void (*)(State*)>("f_ptr", Load, st);
		push<void (*)(State*)>("f_ptr", FastMultiply, st);
		push<void (*)(State*)>("f_ptr", Output, st);
		push<void (*)(State*)>("f_ptr", End, st);
		
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

	void multiply() {
		size_t in = new_channel(), out = new_channel();
		
		State st = st_multiply(in, out);
		
		doco(
			State({}, {in}, Input),
			st
		);
	}
};

/***************************************/
/*               PRIMES                */
/***************************************/

namespace primes {
	const int MAX_VALUE = 100000;
	
	void Output(State* state) {
		if(get_ready<int>(state->inputs[0]))
			cout << get<int>(state->inputs[0]) << endl;
	}

	void Integers(State* state) {
		int value = get<int>("value", state);
		
		if(value <= MAX_VALUE) {
			put<int>(value, state->outputs[0]);
			put<int>("value", value + 1, state);
		}
		else {
			state->continuation = nullptr;
		}
	}

	void Filter(State* state) {
		if(get_ready<int>(state->inputs[0])) {
			int value = get<int>(state->inputs[0]);
			int prime = get<int>("prime", state);
			
			if(value % prime != 0) {
				put<int>(value, state->outputs[0]);
			}
		}
	}

	void Sift(State* state) {
		if(get_ready<int>(state->inputs[0])) {
			int prime = get<int>(state->inputs[0]);
			put<int>(prime, state->outputs[0]);
			
			size_t q = new_channel();
			
			size_t in = state->inputs[0];
			state->inputs.pop_back();
			
			State filter({in}, {q}, Filter);
			put<int>("prime", prime, &filter);
			
			doco(
				State({q}, {state->outputs[0]}, Sift),
				filter
			);
			
			state->continuation = nullptr;
		}
	}

	void primes() {
		size_t q1 = new_channel(), q2 = new_channel();
		
		State integers({}, {q1}, Integers);
		put<int>("value", 2, &integers);
		
		doco(
			integers, 
			State({q1}, {q2}, Sift),
			State({q2}, {}, Output)
		);
	}
};

size_t last_printed = 0;

namespace dumb {
	const int MAX_VALUE = 1000000;

	void Output(State* state) {
		if(get_ready<int>(state->inputs[0])) {
			size_t val = get<int>(state->inputs[0]);
			if(last_printed <= val) {
				cout << val << endl;
				last_printed = val;
			}
			else {
				exit(-1);
			}
		}
	}

	void Dumb(State* state) {
		if(get_ready<int>(state->inputs[0])) {
			int val = get<int>(state->inputs[0]);
			put<int>(val, state->outputs[0]);
		}
	}

	void Integers(State* state) {
		int value = get<int>("value", state);
		
		if(value <= MAX_VALUE) {
			put<int>(value, state->outputs[0]);
			put<int>("value", value + 1, state);
		}
		else {
			state->continuation = nullptr;
		}
	}
	
	void dumb() {
		size_t q1 = new_channel(), q2 = new_channel();
		
		State integers({}, {q1}, Integers);
		put<int>("value", 0, &integers);
		
		doco(
			integers,
			State({q1}, {q2}, Dumb),
			State({q2}, {}, Output)
		);
	}
};

vector<thread> threads;

int main(int argc, char* argv[]) {
	ios_base::sync_with_stdio(false);
	cin.tie(nullptr);
	cout.tie(nullptr);
	
	struct sockaddr_in serv_addr;
	int fd = socket(AF_INET, SOCK_STREAM, 0);
	
	const int port = stoi(getenv("PORT"));
	
	if(argc == 2) {
		// Server side
		est_serveur = true;
		
		serv_addr.sin_family = AF_INET;    
		serv_addr.sin_addr.s_addr = htonl(INADDR_ANY); 
		serv_addr.sin_port = htons(port);
		
		bind(fd, (struct sockaddr*)&serv_addr, sizeof(serv_addr));
		
		if(listen(fd, 10) == -1){
			cerr << "SOCKET ERROR" << endl;
			return -1;
		}
		
		primes::primes();
		threads.push_back(thread(run, 8));
		
		outputs_clients.push_back(nullptr);
		
		size_t iClient = 1;
		while(true) {
			int cfd = accept(fd, (struct sockaddr*)NULL, NULL);
			threads.push_back(thread(client_link, cfd, iClient));
			iClient++;
		}
	}
	else {
		// Client side
		est_serveur = false;
		
		serv_addr.sin_family = AF_INET;
		serv_addr.sin_port = htons(port);
		serv_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
		
		if(connect(fd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
			cerr << "SOCKET ERROR" << endl;
			return -1;
		}
		
		threads.push_back(thread(server_link, fd));
		run(8);
	}
	return 0;
}
