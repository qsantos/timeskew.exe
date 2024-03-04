SRCD = Detours\src
timeskew.dll: timeskew.cpp $(SRCD)\detours.cpp $(SRCD)\modules.cpp $(SRCD)\disasm.cpp $(SRCD)\creatwth.cpp
    cl /LD /nologo /MT /W4 /O2 /I$(SRCD) /Fe$@ $** /link /export:DetourFinishHelperProcess,@1,NONAME
timeskew.exe: withdll.cpp $(SRCD)\detours.cpp $(SRCD)\modules.cpp $(SRCD)\disasm.cpp $(SRCD)\creatwth.cpp
    cl /nologo /MT /W4 /O2 /I$(SRCD) /Fe$@ $** /link /export:DetourFinishHelperProcess,@1,NONAME