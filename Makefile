SRC = src
OBJ = obj
CFLAGS = -g -Wall
LDFLAGS =
LDFLAGS_TEST = -lcppunit

CORE_OBJS = \
	$(OBJ)/track.o \
	$(OBJ)/song.o \
	$(OBJ)/mml_input.o

UNITTEST_OBJS = \
	$(CORE_OBJS) \
	$(OBJ)/unittest/test_track.o \
	$(OBJ)/unittest/test_song.o \
	$(OBJ)/unittest/main.o

$(OBJ)/%.o: $(SRC)/%.cpp
	@mkdir -p $(@D)
	$(CXX) $(CFLAGS) -c $< -o $@

unittest: $(UNITTEST_OBJS)
	$(CXX) $(LDFLAGS) $(LDFLAGS_TEST) $(UNITTEST_OBJS) -o $@

SRC_OLD = .
OBJ_OLD = obj/old
CFLAGS_OLD = -g -Wall
LDFLAGS_OLD =

CTRMML_OBJS_OLD = \
			  $(OBJ_OLD)/ctrmml.o \
			  $(OBJ_OLD)/mml.o \
			  $(OBJ_OLD)/track.o \
			  $(OBJ_OLD)/song.o \
			  $(OBJ_OLD)/playback.o \
			  $(OBJ_OLD)/playback_md.o \
			  $(OBJ_OLD)/md.o \
			  $(OBJ_OLD)/vgm.o \
			  $(OBJ_OLD)/fileio.o

CTRMML_HEADERS_OLD = \
				 $(SRC_OLD)/ctrmml.h

ctrmml_old: $(CTRMML_OBJS_OLD) $(CTRMML_HEADERS_OLD)
	$(CC) $(LDFLAGS_OLD) $(CTRMML_OBJS_OLD) -o $@

$(OBJ_OLD)/%.o: $(SRC_OLD)/%.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS_OLD) -c $< -o $@

clean:
	rm -rf $(OBJ_OLD)
	rm -rf $(OBJ)

test: unittest
	./unittest

.PHONY: test clean

