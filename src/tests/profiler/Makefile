all : test profile_normal.pdf profile_rt.pdf profile_timer.pdf
	echo "Done"
.PHONY : clean

clean :
	rm -f *.prof
	rm -f *.pdf
	rm -f test
	rm -f fsync.tmp

test : test.cc
	gcc $< -lprofiler -lpthread -lrt -o test

profile_normal.prof : test
	CPUPROFILE=$@ ./test

profile_rt.prof : test
	CPUPROFILE=$@ CPUPROFILE_REALTIME=1 ./test

profile_timer.prof : test
	CPUPROFILE=$@ CPUPROFILE_EVENT=thread-wallclock ./test

%.pdf: %.prof
	pprof ./test $< -pdf > $@





