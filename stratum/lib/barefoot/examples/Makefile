

test_c: test.c
	gcc -o test_c test.c -lstratum_bfrt 

test_cc: test.cc
	g++ -o test_cc test.cc -lstratum_bfrt  \
		-I/stratum/bazel-stratum/external/com_google_absl \
		-I/stratum/bazel-bin/external/com_github_p4lang_p4runtime \
		-I/stratum/bazel-stratum/external/com_google_protobuf/src \
		-I/stratum/bazel-bin/external/com_google_googleapis

clean :
	rm -f test_c test_cc 