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
#include <iostream>
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

const int BUF_SIZE = 1024;

/* Network Output */

struct Output {
	int fd;
	size_t sz;
	char buffer[BUF_SIZE];
	mutex mtx;
	
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

// PROTOTYPES

struct State;

void FPtrRef(State* st) {};

// CHANNEL DEFINITION : PARALLEL

struct Channel {
	mutex mtx;
	deque<char> buffer;
	
	bool estsature = false;	
};

// GLOBAL VARIABLES FOR WORKERS

bool est_serveur;
size_t nbChannels = 0;

mutex glob_mtx;
map<size_t, Channel*> channels;

// GLOBAL VARIABLES FOR SERVER

map<size_t, size_t> owners;
deque<Output*> outputs_clients;

// GLOBAL VARIABLES FOR CHANNEL

Output* output_serv;

// Channel creation

int new_channel() {
	if(est_serveur) {
		glob_mtx.lock();
		channels[nbChannels] = new Channel();
		owners[nbChannels] = 0;
		size_t res = nbChannels++;
		glob_mtx.unlock();
		return res;
	}
	else {
		//TODO
	}
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
	glob_mtx.lock();
	if(channels.count(chan)) {
		Channel* channel = channels[chan];
		glob_mtx.unlock();
		
		channel->mtx.lock();
		
		channel->estsature = false;
		char* ptr = (char*)&obj;
		for(size_t oct = 0;oct < sizeof(T);oct++) {
			channel->buffer.push_front(ptr[oct]);
		}
		
		channel->mtx.unlock();
	}
	else {
		Output* out;
		
		if(est_serveur)
			out = outputs_clients[owners[chan]];
		else
			out = output_serv;
		
		glob_mtx.unlock();
		
		out->mtx.lock();
		
		put<char>('P', out);
		put<size_t>(chan, out);
		put<size_t>(sizeof(T), out);
		put<T>(obj, out);
		flush(out);
		
		out->mtx.unlock();
	}
	
}

// GET ANY OBJECT FROM CHANNEL

template<typename T>
bool get_ready(size_t chan) {
	glob_mtx.lock();
	Channel* channel = channels[chan];
	glob_mtx.unlock();
	
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

/* Send states through network */

void send_state(Output* out, State* st) {
	out->mtx.lock();
	
	put<size_t>((size_t)st->continuation - (size_t)FPtrRef, out);
	
	put<size_t>(st->memory.size(), out);
	
	for(pair<string, deque<char>> var : st->memory) {
		put<string>(var.first, out);
		put<deque<char>>(var.second, out);
	}
	
	put<size_t>(st->outputs.size(), out);

	for(size_t output : st->outputs) {
		put<size_t>(output, out);
	}
	
	put<size_t>(st->inputs.size(), out);
	
	for(size_t input : st->inputs) {
		glob_mtx.lock();
		delete channels[input];
		channels.erase(input);
		glob_mtx.unlock();
		
		if(est_serveur) {
			//TODO : FAIRE QUELQUE CHOSE DE CORRECT
			owners[input] = 1;
		}
		
		put<size_t>(input, out);
	}
	
	flush(out);
	
	out->mtx.unlock();
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
	
	size_t nbOutputs = get<size_t>(in);
	
	for(size_t iOutput = 0;iOutput < nbOutputs;iOutput++) {
		st->outputs.push_back(get<size_t>(in));
	}
	
	size_t nbInputs = get<size_t>(in);
	
	for(size_t iInput = 0;iInput < nbInputs;iInput++) {
		size_t input = get<size_t>(in);
		st->inputs.push_back(input);
		
		if(est_serveur) {
			owners[input] = 0;
		}
		
		glob_mtx.lock();
		channels[input] = new Channel();
		glob_mtx.unlock();
	}
	
	return st;
}

/* New link */

void Integers(State* state) {
	int value = get<int>("value", state);
	put<int>(value, state->outputs[0]);
	put<int>("value", value + 1, state);
}

void Out(State* state) {
	if(get_ready<int>(state->inputs[0])) {
		cout << get<int>(state->inputs[0]) << endl;
	}
}

void client_link(int fd, int iClient) {
	cerr << "CONNEXION " << iClient << endl;
	Output out(fd);
	Input in(fd);
	
	outputs_clients.push_back(&out);
	
	size_t q = new_channel();
	
	State* st = new State({}, {q}, Integers);
	put<int>("value", 0, st);
	
	State* st2 = new State({q}, {}, Out);
	doco(*st);
	
	send_state(&out, st2);
	
	do {
		char car = get<char>(&in);
		if(car == 'P') {
			size_t chan = get<size_t>(&in);
			size_t size = get<size_t>(&in);
			
			deque<char> buffer;
			
			for(size_t i = 0;i < size;i++) {
				buffer.push_back(get<char>(&in));
			}
			
			glob_mtx.lock();
			if(channels.count(chan)) {
				Channel* channel = channels[chan];
				glob_mtx.unlock();
				
				channel->mtx.lock();
				
				channel->estsature = false;
				for(char car : buffer)
					channel->buffer.push_front(car);
				
				channel->mtx.unlock();
			}
			else {
				Output* out = outputs_clients[owners[chan]];
				glob_mtx.unlock();
				
				out->mtx.lock();
				
				put<char>('P', out);
				put<size_t>(chan, out);
				put<size_t>(size, out);
				for(char car : buffer)
					put<char>(car, out);
				
				flush(out);
				
				out->mtx.unlock();
			}
		}
	} while(true);
}

void server_link(int fd) {
	Input in(fd);
	Output out(fd);
	
	output_serv = &out;
	
	State* st = recv_state(&in);
	
	doco(*st);
	
	do {
		char car = get<char>(&in);
		if(car == 'P') {
			size_t chan = get<size_t>(&in);
			size_t size = get<size_t>(&in);
			
			deque<char> buffer;
			
			for(size_t i = 0;i < size;i++) {
				buffer.push_back(get<char>(&in));
			}
			
			glob_mtx.lock();
			if(channels.count(chan)) {
				Channel* channel = channels[chan];
				glob_mtx.unlock();
				
				channel->mtx.lock();
				
				channel->estsature = false;
				for(char car : buffer)
					channel->buffer.push_front(car);
				
				channel->mtx.unlock();
			}
			else {
				Output* out = output_serv;
				glob_mtx.unlock();
				
				out->mtx.lock();
				
				put<char>('P', out);
				put<size_t>(chan, out);
				put<size_t>(size, out);
				for(char car : buffer)
					put<char>(car, out);
				
				flush(out);
				
				out->mtx.unlock();
			}
		}
	} while(true);
}

#endif
