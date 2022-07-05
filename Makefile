all: poc

poc: poc_spectre_btb_sa_ip.cpp
	g++ -O0 -g -std=c++11 -Wall -Werror -pedantic $< -o $@
