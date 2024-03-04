SRCD = Detours\src

all: timeskew.dll timeskew.exe

# NOTE: nmake does not support parallelization of targets; instead, we use cl's /MP flag
detours.lib: $(SRCD)\detours.cpp $(SRCD)\modules.cpp $(SRCD)\disasm.cpp $(SRCD)\creatwth.cpp
	cl /c /MP /nologo /MT /W4 /O2 /I$(SRCD) $**
	lib /out:$@ /nologo detours.obj modules.obj disasm.obj creatwth.obj

timeskew.dll: timeskew.cpp detours.lib
    cl /LD /nologo /MT /W4 /O2 /I$(SRCD) /Fe$@ $** /link /export:DetourFinishHelperProcess,@1,NONAME

timeskew.exe: withdll.cpp detours.lib
    cl /nologo /MT /W4 /O2 /I$(SRCD) /Fe$@ $**

clean:
	del *.exp *.obj *.lib

destroy: clean
	del *.exe *.dll