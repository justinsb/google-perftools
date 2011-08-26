#include <iostream>
#include <algorithm>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>
#include <errno.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <memory.h>
#include <google/profiler.h>

using namespace std;

//static const int ARRAY_SIZE = 10000000;
static const int ARRAY_SIZE = 100000;
int data[ARRAY_SIZE];

class TimingTimestamp {
	timespec _time;

public:
	TimingTimestamp() {
		mark();
	}

	void mark() {
		clock_gettime(CLOCK_MONOTONIC, &_time);
	}

	const timespec& value() const {
		return _time;
	}

	uint64_t operator-(const TimingTimestamp& rhs) const {
		uint64_t delta = _time.tv_sec - rhs._time.tv_sec;
		delta *= 1000000000L;
		delta += _time.tv_nsec - rhs._time.tv_nsec;
		return delta;
	}

	static uint64_t since(const TimingTimestamp& base) {
		TimingTimestamp now;
		now.mark();
		return now - base;
	}

	static uint32_t micros_since(const TimingTimestamp& base) {
		uint64_t nanos = nanos_since(base);
		return nanos / 1000L;
	}

	static uint64_t nanos_since(const TimingTimestamp& base) {
		TimingTimestamp now;
		now.mark();

		return subtract_nanos(now, base);
	}

	static uint64_t subtract_nanos(const TimingTimestamp& lhs, const TimingTimestamp& rhs) {
		uint64_t delta = lhs._time.tv_sec - rhs._time.tv_sec;
		delta *= 1000000000L;
		delta += lhs._time.tv_nsec;
		delta -= rhs._time.tv_nsec;
		return delta;
	}
};

int do_cpu_bound() {
	srand(0);

	for (int i = 0; i < ARRAY_SIZE; i++) {
		data[i] = rand();
	}

	cout << "Starting sort" << endl;

	{
		TimingTimestamp start;
		std::sort(&data[0], &data[ARRAY_SIZE]);

		cout << "Sort took: " << TimingTimestamp::micros_since(start) << endl;
	}

	return 0;
}

int do_io_bound() {
	static const int bufferSize = 256 * 1024;
	char * buffer = new char[bufferSize];

	char * filename = new char[256];

	sprintf(filename, "tempfile%ld", pthread_self());

	printf("Using tmpfile %s\n", filename);

	int fd;
	if ((fd = creat(filename, S_IWUSR)) < 0) {
		perror("Error creating file");
		return 1;
	}

	memset(buffer, 'J', bufferSize);

	for (int i = 0; i < 100; i++) {
		int wrote = write(fd, buffer, bufferSize);
		if (wrote < 0) {
			perror("error writing file");
			return 1;
		}

		printf("write() wrote %d bytes, doing fsync\n", wrote);
		if (fsync(fd)) {
			perror("fsync() error");
			return 1;
		}
	}

	close(fd);
	if (unlink(filename)) {
		perror("Error deleting temp file");
		return 1;
	}

	delete[] filename;
	delete[] buffer;

	return 0;
}

//atomic<int> threads_done;
int threads_done;

int do_test() {
	do_io_bound();
	do_cpu_bound();
}

void * thread_start(void*) {
	ProfilerRegisterThread();
	do_test();
	__sync_fetch_and_add(&threads_done, 1);
}

int main() {
	const int rep_count = 2;

	pthread_t threads[rep_count];

	for (int i = 0; i < rep_count; i++) {
		//do_test();
		const pthread_attr_t * attr = 0;
		if (pthread_create(&threads[i], attr, &thread_start, 0)) {
			perror("Could not create thread");
			exit(1);
		}
	}


#ifdef USE_BUSY_WAIT
	// If we want the main thread to be CPU bound
	while (__sync_fetch_and_add(&threads_done, 0) != rep_count) {
	}
#else
	for (int i = 0; i < rep_count; i++) {
		if (pthread_join(threads[i], 0)) {
			perror("Error waiting for thread");
			exit(1);
		}
	}
#endif

	cout << "Done!";
}

