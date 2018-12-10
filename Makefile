CC=g++
LDFLAGS= -Wl,-Bstatic -lboost_log -lboost_log_setup -lboost_filesystem -lboost_system -lboost_regex -lboost_thread -Wl,-Bdynamic -lpthread -O2
CXXFLAGS= -O2 -Wall -std=c++11 -DUSE_BOOST_REGEX
SRC=src
BIN=bin
SOURCE=$(wildcard $(SRC)/*.cc ./lib/jsoncpp/src/*.cc)
OBJS=$(patsubst %.cc,%.o, $(SOURCE))
TARGET=confd

%.o:$(SOURCE)%.c
	$(CC) $(CXXFLAGS) $< -o $@

all:$(OBJS)
	@test -d $(BIN) || mkdir -p $(BIN)
	$(CC) -o $(BIN)/$(TARGET) $^ $(LDFLAGS) 
	@cp $(SRC)/config.json $(BIN)
	@cp $(SRC)/nginx_standard.conf $(BIN)

test_opt:
	$(CC) test/nginx_opt_test.cc ./src/nginx_opt.cc -lboost_filesystem -lboost_system -std=c++11 -o $(BIN)/opt_test
	@cp $(SRC)/nginx_standard.conf $(BIN)

test_parse:
	$(CC) test/parse_test.cc -std=c++11 -lboost_regex -lboost_filesystem -lboost_system ./src/util.cc -o $(BIN)/parse_test


clean:
	rm -f  $(OBJS) $(TARGET) $(BIN)/test_opt $(BIN)/test_parse
