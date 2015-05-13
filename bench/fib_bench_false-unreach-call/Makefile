all: named.ll

orig.ll: orig.c
	clang -S -emit-llvm orig.c -o orig.ll

named.ll: orig.ll
	opt orig.ll -instnamer -S -o named.ll
