CXX=g++
INCLUDE = -I./lib/jsoncpp/src/json
LDFLAGS= -Wl,-Bstatic -lboost_log -lboost_log_setup -lboost_filesystem -lboost_system -lboost_regex -lboost_thread -Wl,-Bdynamic -lpthread -O2
CXXFLAGS= -O2 -Wall -std=c++11 -DUSE_BOOST_REGEX -DUSE_MMAP_ANON -DUSE_SYSV_SEM
SRC=src
TEST=test
BIN=bin
TEMPLATE=template
INSTALL=/home/confd/
SOURCE=$(wildcard $(SRC)/*.cc ./lib/jsoncpp/src/*.cc)
OBJS=$(patsubst %.cc,%.o, $(SOURCE))
TARGET=confd

build:$(OBJS)
	@test -d $(BIN) || mkdir -p $(BIN)
	@echo LINK $(BIN)/$(TARGET)
	@$(CXX) -o $(BIN)/$(TARGET) $^ $(LDFLAGS) 
	@cp $(SRC)/config.json $(BIN)
	@cp $(TEMPLATE)/nginx_*.conf $(BIN)

install:
	@test -d $(INSTALL) || mkdir -p $(INSTALL)
	cp $(BIN)/$(TARGET) $(INSTALL)
	cp $(SRC)/config.json $(INSTALL)
	cp $(TEMPLATE)/nginx_*.conf $(INSTALL)

test_opt: $(SRC)/nginx_opt.o $(TEST)/nginx_opt_test.o $(SRC)/util.o
	@test -d $(BIN) || mkdir -p $(BIN)
	$(CXX) -g -o $(BIN)/opt $^ $(LDFLAGS)
	@cp $(TEMPLATE)/nginx_standard.conf $(BIN)

test_parse: $(SRC)/util.o $(TEST)/parse_test.o
	@test -d $(BIN) || mkdir -p $(BIN)
	$(CXX) -g -o $(BIN)/parse $^ $(LDFLAGS)

test_lock: $(SRC)/confd_shmtx.o $(TEST)/confd_lock.o
	@test -d $(BIN) || mkdir -p $(BIN)
	$(CXX) -g -o $(BIN)/lock $^

test_util: $(SRC)/util.o $(TEST)/util_test.o
	@test -d $(BIN) || mkdir -p $(BIN)
	$(CXX) -g -o $(BIN)/util $^ $(LDFLAGS)

.PHONY: test
test:
	make test_opt
	make test_parse
	make test_lock
	make test_util
	cd bin && ./opt 
	cd bin && ./parse 
	cd bin && ./lock 
	cd bin && ./util

%.o: %.cc
	@echo CXX $<
	@$(CXX) $(INCLUDE) $(CXXFLAGS) -c $^ -o $@

.PHONY: clean
clean:
	rm -f  $(OBJS) $(BIN)/$(TARGET) $(BIN)/opt $(BIN)/parse  $(BIN)/lock $(TEST)/*.o

rebuild: clean
	@make build -j16
