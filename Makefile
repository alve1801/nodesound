CFLAGS = -lm -lGL -lX11 -lpng -lpthread -lasound -Wno-deprecated -g -ffast-math
# did you know the math header also defines faster versions of some functions if you dont need scientific precision?

all: main.cpp
	g++ -o a main.cpp $(CFLAGS)
	#vblank_mode=0 ./a
	./a
