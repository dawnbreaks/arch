include ../Inc.mk

CPP_VPATH=exception ipc buffer net logging \
       common channel channel/socket channel/fifo\
       channel/codec channel/timer channel/signal \
       channel/http channel/inotify \
       thread concurrent util timer process \
       framework framework/process framework/handler framework/admin\
       framework/event framework/vcs util/so \
       misc/crypto database protocol/redis
       
C_VPATH=channel/redis mem/mm mem concurrent database \
        util util/container misc/http misc/crypto misc/sds misc/cjson 
        

CPPFILES := $(foreach dir, $(CPP_VPATH), $(wildcard $(dir)/*.cpp))
CFILES := $(foreach dir, $(C_VPATH), $(wildcard $(dir)/*.c))

CPP_OBJECTS := $(patsubst %.cpp, %.o, $(CPPFILES))
C_OBJECTS := $(patsubst %.c, %.o, $(CFILES))

DIST_LIB = $(INSTALL_DIR)/libarch.so
DIST_LIBA = $(INSTALL_DIR)/libarch.a

all: $(DIST_LIBA) 

$(DIST_LIB):$(CPP_OBJECTS) ${C_OBJECTS}
	${CXX} -shared -o $@ $^

$(DIST_LIBA):$(CPP_OBJECTS) ${C_OBJECTS}
	ar rcs $@ $^

clean:
	rm -f $(CPP_OBJECTS) ${C_OBJECTS} $(DIST_LIB) $(DIST_LIBA)
