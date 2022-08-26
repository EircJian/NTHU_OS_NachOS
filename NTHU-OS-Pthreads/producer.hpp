#include <pthread.h>
#include "thread.hpp"
#include "ts_queue.hpp"
#include "item.hpp"
#include "transformer.hpp"

#ifndef PRODUCER_HPP
#define PRODUCER_HPP

class Producer : public Thread {
public:
	// constructor
	Producer(TSQueue<Item*>* input_queue, TSQueue<Item*>* worker_queue, Transformer* transfomrer);

	// destructor
	~Producer();

	virtual void start();
private:
	TSQueue<Item*>* input_queue;
	TSQueue<Item*>* worker_queue;

	Transformer* transformer;

	// the method for pthread to create a producer thread
	static void* process(void* arg);
};

Producer::Producer(TSQueue<Item*>* input_queue, TSQueue<Item*>* worker_queue, Transformer* transformer)
	: input_queue(input_queue), worker_queue(worker_queue), transformer(transformer) {
}

Producer::~Producer() {}

void Producer::start() {
	// TODO: starts a Producer thread
	pthread_create(&t, 0, Producer::process, (void*)this);
}

void* Producer::process(void* arg) {
	// TODO: implements the Producer's work
	Producer *producer = (Producer*)arg;

	while(1){
		if(producer->input_queue->get_size() > 0) {
			//std::cout << "into Producer::process" << '\n';
			//while(worker_queue->get_size < worker_queue->g)
			
			//if(producer->worker_queue->get_size() < ) {

			//}
			Item *toBeTransformed = producer->input_queue->dequeue();
			unsigned long long int transformedValue = producer->transformer->producer_transform(toBeTransformed->opcode, toBeTransformed->val);

			Item *transformedItem = new Item(toBeTransformed->key, transformedValue, toBeTransformed->opcode);
			producer->worker_queue->enqueue(transformedItem);
			delete toBeTransformed;
		}
	}

	return nullptr;
}

#endif // PRODUCER_HPP
