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
#include <sys/socket.h>
#include <sys/types.h>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
using namespace std;

// PROTOTYPES

struct State;

void FPtrRef(State* st) {};

// CHANNEL DEFINITION : PARALLEL

struct Channel {
	mutex mtx;
	deque<char> buffer;
	
	bool estsature = false;	
};

// GLOBAL VARIABLES

bool est_serveur;
size_t nbChannels = 0;
map<size_t, Channel*> channels;

// Channel creation

int new_channel() {
	channels[nbChannels] = new Channel();
	return nbChannels++;
}

// STATE DEFINITION : NOT PARALLEL

struct State {
	map<string, deque<char>> memory;
	
	// DUE TO VIRTUAL MEMORY, EVERY COMPUTER MUST RUN THE SAME BINARY !
	void (*continuation)(State*);
	
	vector<size_t> inputs, outputs;
	
	State(vector<size_t> _inputs, vector<size_t> _outputs, void (*_continuation)(State*) ) {
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
void put(T obj, size_t chan) {
	Channel* channel = channels[chan];
	
	channel->mtx.lock();
	channel->estsature = false;
	char* ptr = (char*)&obj;
	for(size_t oct = 0;oct < sizeof(T);oct++) {
		channel->buffer.push_front(ptr[oct]);
	}
	
	channel->mtx.unlock();
}

// GET ANY OBJECT FROM CHANNEL

template<typename T>
bool get_ready(size_t chan) {
	Channel* channel = channels[chan];
	
	channel->mtx.lock();
	bool estOk = channel->buffer.size() >= sizeof(T);
	if(!estOk)
		channel->estsature=true;
	channel->mtx.unlock();
	return estOk;
}

template<typename T>
T get(size_t chan) {
	Channel* channel = channels[chan];
	
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
	for(size_t chan : state->inputs)
		if(channels[chan]->estsature)
			return false;
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
			
			sleep(1);
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

const int BUF_SIZE = 1024;

/* Network Output */

struct Output {
	int fd;
	size_t sz;
	char buffer[BUF_SIZE];
	
	Output(int _fd) {
		sz = 0;
		fd = _fd;
	}
};

void flush(Output* out) {
	send(out->fd, out->buffer, out->sz, 0);
	out->sz = 0;
}

template<typename T>
void put(T value, Output* out) {
	char* ptr = (char*)&value;
	
	for(size_t i = 0;i < sizeof(T);i++) {
		out->buffer[out->sz++] = ptr[i];
		if(out->sz == BUF_SIZE) {
			flush(out);
		}
	}
}

template<>
void put<string>(string value, Output* out) {
	put<size_t>(value.size(), out);
	
	for(char car : value) {
		put<char>(car, out);
	}
}

template<>
void put<deque<char>>(deque<char> value, Output* out) {
	put<size_t>(value.size(), out);
	
	for(char car : value) {
		put<char>(car, out);
	}
}

/* Network Input */

struct Input {
	int fd;
	size_t sz, pos;
	char buffer[BUF_SIZE];
	
	Input(int _fd) {
		sz = 0;
		pos = 0;
		fd = _fd;
	}
};

template<typename T>
T get(Input* in) {
	T res;
	char* ptr = (char*)&res;
	
	for(size_t i = 0;i < sizeof(T);i++) {
		while(in->pos == in->sz) {
			in->sz = read(in->fd, in->buffer, BUF_SIZE);
			in->pos = 0;
		}
		
		ptr[i] = in->buffer[in->pos++];
	}
	
	return res;
}

template<>
string get<string>(Input* in) {
	string res;
	
	size_t sz = get<size_t>(in);
	
	for(size_t i = 0;i < sz;i++) {
		res.push_back(get<char>(in));
	}
	
	return res;
}

template<>
deque<char> get<deque<char>>(Input* in) {
	deque<char> res;
	
	size_t sz = get<size_t>(in);
	
	for(size_t i = 0;i < sz;i++) {
		res.push_back(get<char>(in));
	}
	
	return res;
}

/* Send states through network */

void send_state(Output* out, State* st) {
	put<size_t>((size_t)st->continuation - (size_t)FPtrRef, out);
	
	put<size_t>(st->memory.size(), out);
	
	for(pair<string, deque<char>> var : st->memory) {
		put<string>(var.first, out);
		put<deque<char>>(var.second, out);
	}
	
	flush(out);
}

State* recv_state(Input* in) {
	State* st = new State();
	
	st->continuation = (void (*)(State*))(get<size_t>(in) + (size_t)FPtrRef);
	
	size_t memorySz = get<size_t>(in);
	
	for(size_t iVar = 0;iVar < memorySz;iVar++) {
		string name = get<string>(in);
		deque<char> content = get<deque<char>>(in);
		st->memory[name] = content;
	}
	
	return st;
}

/* New link */

void client_link(int fd) {
	cerr << "CONNEXION" << endl;
	
	Output out(fd);
	Input in(fd);
	
	do {
		put<string>("ping", &out);
		flush(&out);
		
		string answer = get<string>(&in);
		cerr << answer << endl;
	} while(true);

	close(fd);
}

void server_link(int fd) {
	Input in(fd);
	Output out(fd);
	
	do {
		string question = get<string>(&in);
		cerr << question << endl;
		
		put<string>("pong", &out);
		flush(&out);
	} while(true);
}

#endif
