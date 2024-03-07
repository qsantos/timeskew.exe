SRCD = Detours\src
OBJ = creatwth.o detours.o disasm.o modules.o

all: timeskew.zip

%.o: $(SRCD)/%.cpp
	g++ -c -w -O3 -isystem$(SRCD) $< -o $@

timeskew.dll: timeskew.cpp $(OBJ)
	g++ -shared -o $@ -isystemDetours/src $^ -lmincore -luser32 -static-libgcc -static-libstdc++ -static

timeskew.exe: withdll.cpp $(OBJ)
	@# For some reason, compiling Detours to object files first lead to multiple definitions of StringCchPrintfA, StringCchPrintfW
	g++ -o $@ -isystemDetours/src $^ -Wl,--allow-multiple-definition -static-libgcc -static-libstdc++ -static

timeskew.zip: timeskew.exe timeskew.dll
	zip -r $@ $^

clean:
	rm *.o

destroy: clean
	rm *.exe *.dll
