all: poc

poc: poc_spectre_btb_sa_ip.cpp
	g++ $< -o $@
