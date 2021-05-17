#ifndef CHANNEL_H
#define CHANNEL_H

#include <vector>
#include <deque>
#include <thread>
#include <mutex>
#include <map>
#include <functional>
#include <list>
#include <cassert>
#include <condition_variable>
using namespace std;

// PROTOTYPES

struct Channel;
struct State;

// CHANNEL DEFINITION : PARALLEL

struct Channel {
	mutex mtx;
	State *entree, *sortie;
	deque<char> buffer;
	bool estsature=false;
};

// STATE DEFINITION : NOT PARALLEL

struct State {
	map<string, deque<char>> memory;
	
	// DUE TO VIRTUAL MEMORY, EVERY COMPUTER MUST RUN THE SAME BINARY !
	function<void(State*)> continuation;
	
	vector<Channel*> inputs, outputs;
	
	State(vector<Channel*> _inputs, vector<Channel*> _outputs, function<void(State*)> _continuation) {
		inputs = _inputs;
		outputs = _outputs;
		continuation = _continuation;
	}
	
	State() {}
};

// GET ANY OBJECT FROM MEMORY

template<typename T>
T get(string name, State* state) {
	T obj;
	
	char* ptr = (char*)&obj;
	for(size_t oct = 0;oct < sizeof(T);oct++) {
		ptr[oct] = state->memory[name][oct];
	} 
	
	return obj;
}

// PUT ANY OBJECT TO MEMORY

template<typename T>
void put(string name, T obj, State* state) {
	char* ptr = (char*)&obj;
	
	state->memory[name].clear();
	for(size_t oct = 0;oct < sizeof(T);oct++) {
		state->memory[name].push_back(ptr[oct]);
	}
}

template<typename T>
void push(string name, T obj, State* state) {
	char* ptr = (char*)&obj;
	
	for(size_t oct = 0;oct < sizeof(T);oct++) {
		state->memory[name].push_back(ptr[oct]);
	}
}

template<typename T>
void push_front(string name, T obj, State* state) {
	char* ptr = (char*)&obj;
	
	for(size_t oct = 0;oct < sizeof(T);oct++) {
		state->memory[name].push_front(ptr[sizeof(T) - oct - 1]);
	}
}

template<typename T>
T pop(string name, State* state) {
	T obj;
	
	char* ptr = (char*)&obj;
	for(size_t oct = 0;oct < sizeof(T);oct++) {
		ptr[oct] = state->memory[name].front();
		state->memory[name].pop_front();
	}
	
	return obj;
}

// PUT ANY OBJECT TO CHANNEL

template<typename T>
void put(T obj, Channel* channel) {
	channel->mtx.lock();
	channel->estsature=false;
	char* ptr = (char*)&obj;
	for(size_t oct = 0;oct < sizeof(T);oct++) {
		channel->buffer.push_front(ptr[oct]);
	}
	
	channel->mtx.unlock();
}

// GET ANY OBJECT FROM CHANNEL

template<typename T>
bool get_ready(Channel* channel) {
	channel->mtx.lock();
	bool estOk = channel->buffer.size() >= sizeof(T);
	if(!estOk)
		channel->estsature=true;
	channel->mtx.unlock();
	return estOk;
}

template<typename T>
T get(Channel* channel) {
	T obj;
	
	channel->mtx.lock();
	
	char* ptr = (char*)&obj;
	for(size_t oct = 0;oct < sizeof(T);oct++) {
		ptr[oct] = channel->buffer.back();
		channel->buffer.pop_back();
	} 
	
	channel->mtx.unlock();
	
	return obj;
}

//VERIFIE QU'ON PEUT CONTINUER A EXECUTER
bool peut_avancer(State *state){
	for(auto el : state->inputs)if(el->estsature)return false;
	return true;
}


// KAHN NETWORK IMPLEMENTATION FOR PARALLELISM

mutex mtx;
int nbRunning;
int nbProcess;

deque<State*> active_processes;

void add_process(State state) {
	State* cpy = new State();
	*cpy = state;
	mtx.lock();
	active_processes.push_back(cpy);
	mtx.unlock();
}

void doco() {}

template<typename ...T>
void doco(State state, T... states) {
	add_process(state);
	doco(states...);	
}

void worker(int num) {
	while(true) {
		if(active_processes.size() > 0) {
			mtx.lock();
			if(active_processes.empty()) {
				mtx.unlock();
				continue;
			}
			nbRunning++;
			State* proc = active_processes.front();
			active_processes.pop_front();
			mtx.unlock();
			
			while(peut_avancer(proc)){
				proc->continuation(proc);
				if(proc->continuation == nullptr)break;
				//printf("%d\n", proc->continuation);
			}
			
			mtx.lock();
			if(proc->continuation != nullptr) {
				active_processes.push_back(proc);
				nbRunning--;
			}
			else {
				nbRunning--;
			}
			mtx.unlock();
		}
		else if(nbRunning == 0) {
			mtx.lock();
			if(nbRunning != 0 || !active_processes.empty()) {
				mtx.unlock();
				continue;
			}
			mtx.unlock();
			
			break;
		}
	}
}

void run(size_t nbWorkers = 1) {
	vector<thread> workers;
	
	nbRunning = 0;
	nbProcess = nbWorkers;
	for(size_t iWorker = 0;iWorker < nbWorkers;iWorker++) {
		workers.push_back(thread(worker, iWorker));
	}

	for(auto it = workers.begin();it != workers.end();it++) {
		it->join();
	}
}

#endif
