all: bar black_square

SRC = xw.cpp words.cpp
HDR = xw.h words.h

bar: bar.cpp $(SRC) $(HDR)
	g++ -g bar.cpp $(SRC) -lncurses -o bar
black_square: black_square.cpp $(SRC) $(HDR)
	g++ -g black_square.cpp $(SRC) -lncurses -o black_square

word_square: word_square.cpp $(SRC) $(HDR)
	g++ -g word_square.cpp $(SRC) -lncurses -o word_square
xwtest: xwtest.cpp $(SRC) $(HDR)
	g++ -g xwtest.cpp $(SRC) -lncurses -o xwtest
