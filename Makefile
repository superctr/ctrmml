SRC = src
OBJ = obj
CFLAGS = -g -Wall
LDFLAGS =
LDFLAGS_TEST = -lcppunit

CORE_OBJS = \
	$(OBJ)/track.o \
	$(OBJ)/song.o \
	$(OBJ)/input.o \
	$(OBJ)/mml_input.o \
	$(OBJ)/player.o \
	$(OBJ)/stringf.o \
	$(OBJ)/vgm.o \
	$(OBJ)/platform/md.o

MMLC_OBJS = \
	$(CORE_OBJS) \
	$(OBJ)/mmlc.o

UNITTEST_OBJS = \
	$(CORE_OBJS) \
	$(OBJ)/unittest/test_track.o \
	$(OBJ)/unittest/test_song.o \
	$(OBJ)/unittest/test_input.o \
	$(OBJ)/unittest/test_mml_input.o \
	$(OBJ)/unittest/test_player.o \
	$(OBJ)/unittest/test_vgm.o \
	$(OBJ)/unittest/main.o

$(OBJ)/%.o: $(SRC)/%.cpp
	@mkdir -p $(@D)
	$(CXX) $(CFLAGS) -c $< -o $@

mmlc: $(MMLC_OBJS)
	$(CXX) $(MMLC_OBJS) $(LDFLAGS) -o $@

unittest: $(UNITTEST_OBJS)
	$(CXX) $(UNITTEST_OBJS) $(LDFLAGS) $(LDFLAGS_TEST) -o $@

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

doc:
	doxygen Doxyfile

cleandoc:
	rm -rf doxygen

test: unittest
	./unittest

.PHONY: test clean doc cleandoc

