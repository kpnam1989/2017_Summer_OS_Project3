CFLAGS := -Wall --std=gnu99 -g3 -Werror -fPIC
ASAN_FLAGS = -fsanitize=address -fno-omit-frame-pointer
ASAN_LIBS = -static-libasan
CURL_LIBS := $(shell curl-config --libs)
CURL_CFLAGS := $(shell curl-config --cflags)

ARCH := $(shell uname)
ifneq ($(ARCH),Darwin)
  LDFLAGS += -lpthread -lrt
endif

PROXY_OBJ := webproxy.o steque.o

all: clean webproxy

all_noasan: clean webproxy_noasan

noasan: all_noasan

webproxy: $(PROXY_OBJ) handle_with_cache.o handle_with_curl.o gfserver.o 
	$(CC) -o $@ $(CFLAGS) $(ASAN_FLAGS) $(CURL_CFLAGS) $^ $(LDFLAGS) $(CURL_LIBS) $(ASAN_LIBS)

webproxy_noasan: $(PROXY_OBJ) handle_with_cache.o handle_with_curl.o gfserver_noasan.o 
	$(CC) -o $@ $(CFLAGS) $(CURL_CFLAGS) $^ $(LDFLAGS) $(CURL_LIBS)

%_noasan.o : %.c
	$(CC) -c -o $@ $(CFLAGS) $<

%.o : %.c
	$(CC) -c -o $@ $(CFLAGS) $(ASAN_FLAGS) $<

.PHONY: clean

clean:
	mv gfserver.o gfserver.tmpo 
	mv gfserver_noasan.o gfserver_noasan.tmpo
	rm -rf *.o webproxy webproxy_noasan
	mv gfserver.tmpo gfserver.o
	mv gfserver_noasan.tmpo gfserver_noasan.o
