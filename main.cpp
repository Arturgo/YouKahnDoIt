#include <iostream>
#include "channel.h"
using namespace std;

mutex m;

void Output(State* state) {
	if(get_ready<int>(state->inputs[0]))
		cout << get<int>(state->inputs[0]) << endl;
}

void Integers(State* state) {
	int value = get<int>(0, state);
	
	if(put_ready<int>(state->outputs[0])) {
		m.lock();
		cerr << "GEN " << value << endl;
		m.unlock();
		
		put<int>(value, state->outputs[0]);
		
		put<int>(0, value + 1, state);
	}
}

void Filter(State* state) {
	if(get_ready<int>(state->inputs[0]) && put_ready<int>(state->outputs[0])) {
		int value = get<int>(state->inputs[0]);
		int prime = get<int>(0, state);
		
		m.lock();
		cerr << "FILTER " << value << " " << prime << endl;
		m.unlock();
		
		if(value % prime != 0) {
			put<int>(value, state->outputs[0]);
		}
	}
}

void Sift(State* state) {
	if(get_ready<int>(state->inputs[0]) && put_ready<int>(state->outputs[0])) {
		int prime = get<int>(state->inputs[0]);
		put<int>(prime, state->outputs[0]);
		
		m.lock();
		cerr << "SIFT " << prime << endl;
		m.unlock();
		
		Channel* q = new Channel();
		
		State filter({state->inputs[0]}, {q}, Filter, sizeof(int));
		put<int>(0, prime, &filter);
		
		doco(
			State({q}, {state->outputs[0]}, Sift),
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
	
	doco(
		integers, 
		State({&q1}, {&q2}, Sift),
		State({&q2}, {}, Output)
	);
	
	run(2);
	return 0;
}
