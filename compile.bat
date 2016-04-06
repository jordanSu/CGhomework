gcc -DGLEW_STATIC -I. -c glew.c
g++ -o HW2 -static -O2 -std=c++0x -DGLEW_STATIC main.cpp tiny_obj_loader.cc glew.o -I.  -lglfw3 -lopengl32 -lgdi32 
strip HW2.exe
