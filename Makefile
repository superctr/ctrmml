SRC = src
OBJ = obj

CFLAGS = -Wall
LDFLAGS =

ifneq ($(RELEASE),1)
CFLAGS += -g
else
CFLAGS += -O2
LDFLAGS += -s
endif


LDFLAGS_TEST = -lcppunit
ifeq ($(OS),Windows_NT)
	LDFLAGS += -static-libgcc -static-libstdc++ -Wl,-Bstatic -lstdc++ -lpthread -Wl,-Bdynamic
endif

CORE_OBJS = \
	$(OBJ)/track.o \
	$(OBJ)/song.o \
	$(OBJ)/input.o \
	$(OBJ)/mml_input.o \
	$(OBJ)/player.o \
	$(OBJ)/stringf.o \
	$(OBJ)/vgm.o \
	$(OBJ)/driver.o \
	$(OBJ)/wave.o \
	$(OBJ)/riff.o \
	$(OBJ)/conf.o \
	$(OBJ)/platform/md.o \
	$(OBJ)/platform/mdsdrv.o

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
	$(OBJ)/unittest/test_riff.o \
	$(OBJ)/unittest/test_conf.o \
	$(OBJ)/unittest/test_mdsdrv.o \
	$(OBJ)/unittest/main.o

$(OBJ)/%.o: $(SRC)/%.cpp
	@mkdir -p $(@D)
	$(CXX) $(CFLAGS) -c $< -o $@

mmlc: $(MMLC_OBJS)
	$(CXX) $(MMLC_OBJS) $(LDFLAGS) -o $@

unittest: $(UNITTEST_OBJS)
	$(CXX) $(UNITTEST_OBJS) $(LDFLAGS) $(LDFLAGS_TEST) -o $@

clean:
	rm -rf $(OBJ)

doc:
	doxygen Doxyfile

cleandoc:
	rm -rf doxygen

test: unittest
	./unittest

.PHONY: test clean doc cleandoc

