all: barrier black_square

SRC = xw.cpp
HDR = xw.h

xword: xword.cpp
	g++ -g xword.cpp -o xword

barrier: barrier.cpp $(SRC) $(HDR)
	g++ -g barrier.cpp $(SRC) -lncurses -o barrier
black_square: black_square.cpp $(SRC) $(HDR)
	g++ -g black_square.cpp $(SRC) -lncurses -o black_square

word_square: word_square.cpp $(SRC) $(HDR)
	g++ -g word_square.cpp $(SRC) -lncurses -o word_square
xwtest: xwtest.cpp $(SRC) $(HDR)
	g++ -g xwtest.cpp $(SRC) -lncurses -o xwtest
