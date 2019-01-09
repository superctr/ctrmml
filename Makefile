SRC = .
OBJ = obj

ctrmml: \
	$(OBJ)/ctrmml.o \
	$(OBJ)/mml.o \
	$(OBJ)/track.o
	$(CC) $(LDFLAGS) $^ -o $@

$(OBJ)/%.o: $(SRC)/%.c
	mkdir -p $(@D)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf $(OBJ)

