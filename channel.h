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

struct State ;
struct Channel {
	mutex mtx;
	size_t maxSize = 1024;
	State * entree, sortie;
	deque<char> buffer;
	bool estSature=false;
};

// STATE DEFINITION : NOT PARALLEL

struct State {
	vector<char> memory;
	
	// DUE TO VIRTUAL MEMORY, EVERY COMPUTER MUST RUN THE SAME BINARY !
	function<void(State*)> continuation;
	
	vector<Channel*> inputs, outputs;
	
	State(vector<Channel*> _inputs, vector<Channel*> _outputs, function<void(State*)> _continuation, size_t _memory_size = 0) {
		inputs = _inputs;
		outputs = _outputs;
		continuation = _continuation;
		memory.resize(_memory_size);
	}
	
	State() {}
};

// GET ANY OBJECT FROM MEMORY

template<typename T>
T get(size_t position, State* state) {
	T obj;
	
	char* ptr = (char*)&obj;
	for(size_t oct = 0;oct < sizeof(T);oct++) {
		ptr[oct] = state->memory[position + oct];
	} 
	
	return obj;
}

// PUT ANY OBJECT TO MEMORY

template<typename T>
void put(size_t position, T obj, State* state) {
	char* ptr = (char*)&obj;
	for(size_t oct = 0;oct < sizeof(T);oct++) {
		state->memory[position + oct] = ptr[oct];
	}
}

// PUT ANY OBJECT TO CHANNEL

template<typename T>
T put_ready(Channel* channel) {
	channel->mtx.lock();
	bool estOk = channel->buffer.size() + sizeof(T) < channel->maxSize;
	channel->mtx.unlock();
	return estOk;
}

template<typename T>
void put(T obj, Channel* channel) {
	channel->mtx.lock();
	
	char* ptr = (char*)&obj;
	for(size_t oct = 0;oct < sizeof(T);oct++) {
		channel->buffer.push_front(ptr[oct]);
	}
	
	channel->mtx.unlock();
}

// GET ANY OBJECT FROM CHANNEL

template<typename T>
T get_ready(Channel* channel) {
	channel->mtx.lock();
	bool estOk = channel->buffer.size() >= sizeof(T);
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

//Pour connaitre l'état d'un état

bool pret(State * state){
	return state->inputs
}


// KAHN NETWORK IMPLEMENTATION FOR PARALLELISM

mutex mtx;
int nbRunning;

State *sortieFinale;
deque<State*> active_processes;

void define_output(State *sortie){
	sortieFinale=sortie;
}

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

void worker() {
	while(true) {
		mtx.lock();
		
		if(active_processes.size() > 0) {
			nbRunning++;
			State* proc = active_processes.front();
			active_processes.pop_front();
			mtx.unlock();
			
			proc->continuation(proc);
			
			if(proc->continuation != nullptr) {
				mtx.lock();
				
				active_processes.push_back(proc);
				nbRunning--;
				
				mtx.unlock();
			}
		}
		else if(nbRunning == 0) {
			mtx.unlock();
			break;
		}
	}
}

void run(size_t nbWorkers = 1) {
	vector<thread> workers;
	
	nbRunning = 0;
	for(size_t iWorker = 0;iWorker < nbWorkers;iWorker++) {
		workers.push_back(thread(worker));
	}
	
	for(auto it = workers.begin();it != workers.end();it++) {
		it->join();
	}
}

#endif
