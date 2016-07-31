.PHONY: all tests notests deps cleandeps clean
all: notests tests
tests: common_tests client_tests server_tests
notests: client server

define compile
@echo -e '\033[32mCXX $@\033[0m'
$(CXX) $(CXXFLAGS) -c -o $@ $<
endef

define deps
@echo -e '\033[32mDEPS $@\033[0m'
$(CXX) $(CXXFLAGS) -MM -MP -MT '$(<:.cpp=.o) $(<:.cpp=.d)' -o $@ $<
endef

##########################################################################

common_DIR:=common_src/
client_DIR:=client_src/
server_DIR:=server_src/
serializetest_DIR:=serializetestsrc/

INCS:=-I$(common_DIR)
override CXXFLAGS+= \
  -std=c++17 \
  -pthread \
  -O1 \
  -Wall \
  -Wextra \
  -Wno-unused-parameter \
  $(INCS)
# -DNDEBUG \
# -DBOOST_ASIO_ENABLE_HANDLER_TRACKING \

VPATH=$(common_DIR) $(client_DIR) $(server_DIR)

##########################################################################
# common

common_LDLIBS:=-lboost_system
common_SRCS:=$(filter-out %_tests.cpp,$(wildcard $(common_DIR)*.cpp))
common_OBJS:=$(common_SRCS:.cpp=.o)
common_DEPS:=$(common_SRCS:.cpp=.d)

common_tests_LDLIBS:=$(common_LDLIBS)
common_tests_SRCS:=$(wildcard $(common_DIR)*_tests.cpp)
common_tests_OBJS:=$(common_tests_SRCS:.cpp=.o)
common_tests_DEPS:=$(common_tests_SRCS:.cpp=.d)

common_tests: $(common_OBJS) $(common_tests_OBJS)
	@$(CXX) $(CXXFLAGS) -o $@ $^ $(common_tests_LDLIBS) 
	@echo 'LINK common_tests'
$(common_DIR)%.o: $(common_DIR)%.cpp
	$(compile)
$(common_DIR)%.d: $(common_DIR)%.cpp
	$(deps)

-include $(common_DEPS)
-include $(common_tests_DEPS)

##########################################################################
# client

client_LDLIBS:=-lboost_system -lsfml-graphics -lsfml-window -lsfml-system
client_SRCS:=$(filter-out %_tests.cpp,$(wildcard $(client_DIR)*.cpp))
client_OBJS:=$(client_SRCS:.cpp=.o)
client_DEPS:=$(client_SRCS:.cpp=.d)

client_tests_LDLIBS:=$(client_LDLIBS)
client_tests_SRCS:=$(wildcard $(client_DIR)*_tests.cpp)
client_tests_OBJS:=$(client_tests_SRCS:.cpp=.o)
client_tests_DEPS:=$(client_tests_SRCS:.cpp=.d)

client: $(client_OBJS) $(common_OBJS)
	@echo 'LINK client'
	$(CXX) $(CXXFLAGS) -o $@ $^ $(client_LDLIBS) 
client_tests: $(client_tests_OBJS) $(common_OBJS)
	@echo 'LINK client_tests'
	$(CXX) $(CXXFLAGS) -o $@ $^ $(client_tests_LDLIBS) 
$(client_DIR)%.o: $(client_DIR)%.cpp
	$(compile)
$(client_DIR)%.d: $(client_DIR)%.cpp
	$(deps)

-include $(client_DEPS)
-include $(client_tests_DEPS)

##########################################################################
# server

server_LDLIBS:=-lboost_system -lluajit-5.1
server_SRCS:=$(filter-out %_tests.cpp,$(wildcard $(server_DIR)*.cpp))
server_OBJS:=$(server_SRCS:.cpp=.o)
server_DEPS:=$(server_SRCS:.cpp=.d)

server_tests_LDLIBS:=$(server_LDLIBS)
server_tests_SRCS:=$(wildcard $(server_DIR)*_tests.cpp)
server_tests_OBJS:=$(server_tests_SRCS:.cpp=.o)
server_tests_DEPS:=$(server_tests_SRCS:.cpp=.d)

server: $(server_OBJS) $(common_OBJS)
	@echo 'LINK server'
	@$(CXX) $(CXXFLAGS) $(server_LDLIBS) -o $@ $^
server_tests: $(server_tests_OBJS) $(common_OBJS)
	@echo 'LINK server_tests'
	@$(CXX) $(CXXFLAGS) $(server_tests_LDLIBS) -o $@ $^
$(server_DIR)%.o: $(server_DIR)%.cpp
	$(compile)
$(server_DIR)%.d: $(server_DIR)%.cpp
	$(deps)

-include $(server_DEPS)
-include $(server_tests_DEPS)

##########################################################################	

deps: $(common_DEPS) $(common_tests_DEPS) $(client_DEPS) $(client_tests_DEPS) $(server_DEPS) $(server_tests_DEPS)
	@echo 'dependencies created'

cleandeps:
	rm -f $(wildcard $(common_DIR)*.d)
	rm -f $(wildcard $(client_DIR)*.d)
	rm -f $(wildcard $(server_DIR)*.d)

clean: cleandeps
	rm -f $(wildcard $(common_DIR)*.o)
	rm -f client
	rm -f $(wildcard $(client_DIR)*.o)
	rm -f server
	rm -f $(wildcard $(server_DIR)*.o)
	
