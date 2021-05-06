#include <iostream>
#include "channel.h"
using namespace std;

mutex m;

void Output(State* state, int num) {
	if(get_ready<int>(state->inputs[0]))
		cout << get<int>(state->inputs[0]) << endl;
}

void Integers(State* state, int num) {
	int value = get<int>(0, state);
	
		put<int>(value, state->outputs[0]);
		
		put<int>(0, value + 1, state);
}

void Filter(State* state, int num) {
	if(get_ready<int>(state->inputs[0])) {
		int value = get<int>(state->inputs[0]);
		int prime = get<int>(0, state);
		
		if(value % prime != 0) {
			put<int>(value, state->outputs[0]);
		}
	}
}

void Sift(State* state, int num) {
	if(get_ready<int>(state->inputs[0])) {
		int prime = get<int>(state->inputs[0]);
		put<int>(prime, state->outputs[0]);
		
		Channel* q = new Channel();

		State *filter= new_process({state->inputs[0]}, {q}, Filter);
		put<int>(0, prime, filter);
		doco(
			new_process({q},{state->outputs[0]} , Sift),
			filter
		);
		
		state->continuation = nullptr;
	}
}

int main() {
	ios_base::sync_with_stdio(false);
	cin.tie(nullptr);
	cout.tie(nullptr);
	
	Channel q1, q2;
	
	State integers({}, {&q1}, Integers, sizeof(size_t));
	put<int>(0, 2, &integers);
	
	State debut=State({&q1}, {&q2}, Sift), output =State({&q2}, {}, Output);
	doco(
		&integers, 
		&debut,
		&output
	);
	
	define_output(&output);
	run(1);
	return 0;
}
