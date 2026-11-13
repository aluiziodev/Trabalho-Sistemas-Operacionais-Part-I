CC = gcc
CFLAGS = -Wall -Wextra -O2 -std=gnu11 -I./src

SRC_DIR = src
TEST_DIR = tests

CLIENTE = cliente
ANALISTA = analista
ATENDIMENTO = atendimento

UTIL_OBJ = $(SRC_DIR)/util.o
SCHED_OBJ = $(SRC_DIR)/sched.o
QUEUE_OBJ = $(SRC_DIR)/queue.o
LNG_OBJ = $(SRC_DIR)/lng.o
ANALYST_OBJ = $(SRC_DIR)/analyst_ctl.o
RECEPTION_OBJ = $(SRC_DIR)/reception.o
ATTENDANT_OBJ = $(SRC_DIR)/attendant.o
KEYBOARD_OBJ = $(SRC_DIR)/keyboard.o

MODULE_OBJS = $(UTIL_OBJ) $(SCHED_OBJ) $(QUEUE_OBJ) $(LNG_OBJ) $(ANALYST_OBJ) $(RECEPTION_OBJ) $(ATTENDANT_OBJ) $(KEYBOARD_OBJ)

# Mantém apenas os testes que realmente existem no repositório.
# Isso evita quebrar o build enquanto o projeto ainda não possui os arquivos
# restantes da suíte completa.
TESTS = \
	$(TEST_DIR)/test_lng \
	$(TEST_DIR)/test_analyst \
	$(TEST_DIR)/test_attendant

.PHONY: all test clean

all: $(CLIENTE) $(ANALISTA) $(ATENDIMENTO) $(TESTS)

$(CLIENTE): $(SRC_DIR)/cliente.c
	$(CC) $(CFLAGS) -o $@ $^

$(ANALISTA): $(SRC_DIR)/analista.c
	$(CC) $(CFLAGS) -o $@ $^

$(ATENDIMENTO): $(SRC_DIR)/atendimento.c $(UTIL_OBJ) $(SCHED_OBJ) $(QUEUE_OBJ) $(LNG_OBJ) $(ANALYST_OBJ) $(RECEPTION_OBJ) $(ATTENDANT_OBJ) $(KEYBOARD_OBJ)
	$(CC) $(CFLAGS) -o $@ $(SRC_DIR)/atendimento.c $(UTIL_OBJ) $(SCHED_OBJ) $(QUEUE_OBJ) $(LNG_OBJ) $(ANALYST_OBJ) $(RECEPTION_OBJ) $(ATTENDANT_OBJ) $(KEYBOARD_OBJ)

$(UTIL_OBJ): $(SRC_DIR)/util.c $(SRC_DIR)/util.h
	$(CC) $(CFLAGS) -c -o $@ $(SRC_DIR)/util.c

$(SCHED_OBJ): $(SRC_DIR)/sched.c $(SRC_DIR)/sched.h
	$(CC) $(CFLAGS) -c -o $@ $(SRC_DIR)/sched.c

$(QUEUE_OBJ): $(SRC_DIR)/queue.c $(SRC_DIR)/queue.h
	$(CC) $(CFLAGS) -c -o $@ $(SRC_DIR)/queue.c

$(LNG_OBJ): $(SRC_DIR)/lng.c $(SRC_DIR)/lng.h
	$(CC) $(CFLAGS) -c -o $@ $(SRC_DIR)/lng.c

$(ANALYST_OBJ): $(SRC_DIR)/analyst_ctl.c $(SRC_DIR)/analyst_ctl.h
	$(CC) $(CFLAGS) -c -o $@ $(SRC_DIR)/analyst_ctl.c

$(RECEPTION_OBJ): $(SRC_DIR)/reception.c $(SRC_DIR)/reception.h
	$(CC) $(CFLAGS) -c -o $@ $(SRC_DIR)/reception.c

$(ATTENDANT_OBJ): $(SRC_DIR)/attendant.c $(SRC_DIR)/attendant.h
	$(CC) $(CFLAGS) -c -o $@ $(SRC_DIR)/attendant.c

$(KEYBOARD_OBJ): $(SRC_DIR)/keyboard.c $(SRC_DIR)/keyboard.h
	$(CC) $(CFLAGS) -c -o $@ $(SRC_DIR)/keyboard.c

$(TEST_DIR)/test_util: $(TEST_DIR)/test_util.c $(UTIL_OBJ)
	$(CC) $(CFLAGS) -o $@ $(TEST_DIR)/test_util.c $(UTIL_OBJ)

$(TEST_DIR)/test_sched: $(TEST_DIR)/test_sched.c $(SCHED_OBJ)
	$(CC) $(CFLAGS) -o $@ $(TEST_DIR)/test_sched.c $(SCHED_OBJ)

$(TEST_DIR)/test_queue: $(TEST_DIR)/test_queue.c $(QUEUE_OBJ)
	$(CC) $(CFLAGS) -o $@ $(TEST_DIR)/test_queue.c $(QUEUE_OBJ)

$(TEST_DIR)/test_lng: $(TEST_DIR)/test_lng.c $(LNG_OBJ)
	$(CC) $(CFLAGS) -o $@ $(TEST_DIR)/test_lng.c $(LNG_OBJ)

$(TEST_DIR)/test_analyst: $(TEST_DIR)/test_analyst.c $(ANALYST_OBJ)
	$(CC) $(CFLAGS) -o $@ $(TEST_DIR)/test_analyst.c $(ANALYST_OBJ)

$(TEST_DIR)/test_reception: $(TEST_DIR)/test_reception.c $(RECEPTION_OBJ)
	$(CC) $(CFLAGS) -o $@ $(TEST_DIR)/test_reception.c $(RECEPTION_OBJ)

$(TEST_DIR)/test_attendant: $(TEST_DIR)/test_attendant.c $(ATTENDANT_OBJ)
	$(CC) $(CFLAGS) -o $@ $(TEST_DIR)/test_attendant.c $(ATTENDANT_OBJ)

test: all
	@for t in $(TESTS); do \
		echo "==> $$t"; \
		$$t || exit $$?; \
	done

clean:
	rm -f $(CLIENTE) $(ANALISTA) $(ATENDIMENTO) $(TESTS) $(MODULE_OBJS)
	rm -f demanda.txt lng.txt pidanalista.txt tmp_lng_test.txt
