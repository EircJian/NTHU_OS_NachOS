#include <assert.h>
#include <stdlib.h>
#include "ts_queue.hpp"
#include "item.hpp"
#include "reader.hpp"
#include "writer.hpp"
#include "producer.hpp"
#include "consumer_controller.hpp"

#define READER_QUEUE_SIZE 200
#define WORKER_QUEUE_SIZE 200
#define WRITER_QUEUE_SIZE 4000
#define CONSUMER_CONTROLLER_LOW_THRESHOLD_PERCENTAGE 20
#define CONSUMER_CONTROLLER_HIGH_THRESHOLD_PERCENTAGE 80
#define CONSUMER_CONTROLLER_CHECK_PERIOD 1000000

int main(int argc, char** argv) {
	assert(argc == 4);

	int n = atoi(argv[1]);
	std::string input_file_name(argv[2]);
	std::string output_file_name(argv[3]);

	// TODO: implements main function
	
	// three queue: input, worker, writer
	TSQueue<Item *> *input_queue;
	TSQueue<Item *> *worker_queue;
	TSQueue<Item *> *writer_queue;

	input_queue = new TSQueue<Item*>(READER_QUEUE_SIZE);
	worker_queue = new TSQueue<Item*>(WORKER_QUEUE_SIZE);
	writer_queue = new TSQueue<Item*>(WRITER_QUEUE_SIZE);

	// transformer: to transform items
	Transformer *transformer = new Transformer;

	// reader thread to read in items, writer thread to write out items
	Reader *reader = new Reader(n, input_file_name, input_queue);
	Writer *writer = new Writer(n, output_file_name, writer_queue);

	// four producer threads to perform transform function
	Producer *p1 = new Producer(input_queue, worker_queue, transformer);
	Producer *p2 = new Producer(input_queue, worker_queue, transformer);
	Producer *p3 = new Producer(input_queue, worker_queue, transformer);
	Producer *p4 = new Producer(input_queue, worker_queue, transformer);
	
	// consumer controller thread to contol the number of consumers
	
	ConsumerController *controller;
	controller = new ConsumerController(worker_queue, writer_queue, transformer,
										CONSUMER_CONTROLLER_CHECK_PERIOD,
										CONSUMER_CONTROLLER_LOW_THRESHOLD_PERCENTAGE,
										CONSUMER_CONTROLLER_HIGH_THRESHOLD_PERCENTAGE);
	
	
//Consumer *c1 = new Consumer(worker_queue, writer_queue, transformer);


	// start reader/writer threads
	reader->start();
	writer->start();
	

	// start consumer contoller threads
	controller->start();

	// start producer threads
	p1->start();
	p2->start();
	p3->start();
	p4->start();
	//controller->start();
//c1->start();
	// wait reader/writer to finish
	reader->join();
	//controller->join();
	writer->join();

	// delete four producer threads
	delete p1;
	delete p2;
	delete p3;
	delete p4;
//delete c1;
	// delete controller threads
	delete controller;

	// delete reader/writer threads
	delete writer;
	delete reader;

	// delete three queues
	delete input_queue;
	delete worker_queue;
	delete writer_queue;

	return 0;
}
