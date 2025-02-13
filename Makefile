# Makefile for unix systems
# this requires GNU make

APPNAME=fastcrop

# Compiler flags
# enable all warnings in general
WARNFLAGS= -Wall -Wno-comment -Wno-unused-function

SHAREDFLAGS = $(WARNFLAGS) -pthread

ifeq ($(RELEASE), 1)
CFLAGS =   $(SHAREDFLAGS) -ffast-math -s -O4 -DNDEBUG
CXXFLAGS = $(SHAREDFLAGS) -ffast-math -s -O4 -DNDEBUG
else
CFLAGS =   $(SHAREDFLAGS) -g
CXXFLAGS = $(SHAREDFLAGS) -g
endif

#check if Dear ImGui is available
IMGUI_SRCFILES=$(wildcard imgui/*.cpp)
ifneq ("$(IMGUI_SRCFILES)","")
    WITH_IMGUI=1
    IMGUI_MISSING=0
else
    WITH_IMGUI=0
    IMGUI_MISSING=1
endif

# include paths for the builtin libraries glad and glm. We do not link them,
# as we directly incorporated the source code into our project
CPPFLAGS += -I glad/include

ifeq ($(WITH_IMGUI), 1)
CPPFLAGS +=  -I imgui -I imgui/backends -I imgui/misc/cpp -DWITH_IMGUI
endif

# Try to find the system's GLFW3 library via pkg-config
CPPFLAGS += $(shell pkg-config --cflags glfw3)
LDFLAGS += $(shell pkg-config --static --libs glfw3) 

#check if libjpeg is available
$(shell pkg-config --exists libjpeg)
ifeq ($(.SHELLSTATUS), 0)
    WITH_LIBJPEG=1
else
    WITH_LIBJPEG=0
endif

ifeq ($(WITH_LIBJPEG), 1)
    CPPFLAGS += -DWITH_LIBJPEG $(shell pkg-config --cflags libjpeg)
    LDFLAGS += $(shell pkg-config --libs libjpeg)
endif

#check if libswscale and libavutil are available
$(shell pkg-config --exists libswscale)
ifeq ($(.SHELLSTATUS), 0)
    WITH_LIBSWSCALE=1
else
    WITH_LIBSWSCALE=0
endif
$(shell pkg-config --exists libavutil)
ifeq ($(.SHELLSTATUS), 0)
    WITH_LIBAVUTIL=1
else
    WITH_LIBAVUTIL=0
    WITH_LIBSWSCALE=0
endif

ifeq ($(WITH_LIBSWSCALE), 1)
    CPPFLAGS += -DWITH_LIBSWSCALE $(shell pkg-config --cflags libswscale)
    LDFLAGS += $(shell pkg-config --libs libswscale)
endif
ifeq ($(WITH_LIBAVUTIL), 1)
    CPPFLAGS += $(shell pkg-config --cflags libavutil)
    LDFLAGS += $(shell pkg-config --libs libavutil)
endif

# additional libraries
LDFLAGS += -lrt -lm

CFILES=$(wildcard *.c) glad/src/gl.c
CPPFILES=$(wildcard *.cpp)
INCFILES=$(wildcard *.h)	
SRCFILES = $(CFILES) $(CPPFILES)
PRJFILES = Makefile $(wildcard *.vcxproj) $(wildcard *.sln)
ALLFILES = $(SRCFILES) $(INCFILES) $(PRJFILES)
OBJECTS = $(patsubst %.cpp,%.o,$(CPPFILES)) $(patsubst %.c,%.o,$(CFILES))
	   
ifeq ($(WITH_IMGUI), 1)
CPPFILES += $(IMGUI_SRCFILES) imgui/misc/cpp/imgui_stdlib.cpp imgui/backends/imgui_impl_glfw.cpp imgui/backends/imgui_impl_opengl3.cpp
endif
# build rules
.PHONY: all
all:	$(APPNAME)

# build and start with "make run"
.PHONY: run
run:	all
	./$(APPNAME)

# automatic dependency generation
# create $(DEPDIR) (and an empty file dir)
# create a .d file for every .c source file which contains
# 		   all dependencies for that file
.PHONY: depend
depend:	$(DEPDIR)/dependencies
DEPDIR   = ./dep
DEPFILES = $(patsubst %.c,$(DEPDIR)/%.d,$(CFILES)) $(patsubst %.cpp,$(DEPDIR)/%.d,$(CPPFILES))
$(DEPDIR)/dependencies: $(DEPDIR)/dir $(DEPFILES)
	@cat $(DEPFILES) > $(DEPDIR)/dependencies
$(DEPDIR)/dir:
	@mkdir -p $(DEPDIR)
	@mkdir -p $(DEPDIR)/glad/src
	@mkdir -p $(DEPDIR)/imgui
	@mkdir -p $(DEPDIR)/imgui/backends
	@mkdir -p $(DEPDIR)/imgui/misc/cpp
	@touch $(DEPDIR)/dir
$(DEPDIR)/%.d: %.c $(DEPDIR)/dir
	@echo rebuilding dependencies for $*
	@set -e; $(CC) -M $(CPPFLAGS) $<	\
	| sed 's,\($*\)\.o[ :]*,\1.o $@ : ,g' \
	> $@; [ -s $@ ] || rm -f $@
$(DEPDIR)/%.d: %.cpp $(DEPDIR)/dir
	@echo rebuilding dependencies for $*
	@set -e; $(CXX) -M $(CPPFLAGS) $<	\
	| sed 's,\($*\)\.o[ :]*,\1.o $@ : ,g' \
	> $@; [ -s $@ ] || rm -f $@
-include $(DEPDIR)/dependencies

# rule to build application
$(APPNAME): $(OBJECTS) $(DEPDIR)/dependencies
	$(CXX) $(CFLAGS) $(OBJECTS) $(LDFLAGS) -o$(APPNAME)
ifeq ($(IMGUI_MISSING), 1)
	@echo "WARNING: Build without imgui, did you forget to check out the submodule?"
endif
ifneq ($(WITH_LIBJPEG), 1)
	@echo "WARNING: Build without libjpeg"
endif
ifneq ($(WITH_LIBSWSCALE), 1)
ifneq ($(WITH_LIBAVUTIL), 1)
	@echo "WARNING: Build without libswscale (libavutil missing)"
else
	@echo "WARNING: Build without libswscale"
endif
endif

# remove all unneeded files
.PHONY: clean
clean:
	@echo removing binary: $(APPNAME)
	@rm -f $(APPNAME)
	@echo removing object files: $(OBJECTS)
	@rm -f $(OBJECTS)
	@echo removing dependency files
	@rm -rf $(DEPDIR)
	@echo removing tags
	@rm -f tags

# update the tags file
.PHONY: TAGS
TAGS:	tags

tags:	$(SRCFILES) $(INCFILES) 
	ctags $(SRCFILES) $(INCFILES)

# look for 'TODO' in all relevant files
.PHONY: todo
todo:
	-egrep -in "TODO" $(SRCFILES) $(INCFILES)

