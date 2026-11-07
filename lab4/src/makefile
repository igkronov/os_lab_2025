CC      = gcc

# Флаги компиляции:
#   -Wall    — все предупреждения
#   -I.      — заголовки ищем в текущей директории
#   -pthread — POSIX threads: макросы + правильная компиляция 
CFLAGS  = -Wall -I. -pthread

# Флаги линковки: -pthread обязателен при сборке бинарника с потоками
LDLIBS  = -pthread

# Цель по умолчанию: собрать все три программы (задания 1, 3, 5)
all: parallel_min_max process_memory psum

# Задание 1: parallel_min_max 
# Линкуется из трёх объектников:
#   parallel_min_max.o — main с таймаутом, SIGALRM и SIGKILL
#   find_min_max.o     — функция GetMinMax
#   utils.o            — GenerateArray и прочее
# $@ — имя цели, $^ — все зависимости
parallel_min_max: parallel_min_max.o find_min_max.o utils.o
	$(CC) -o $@ $^

#  Задание 3: process_memory 
# Демонстрация виртуальной памяти: etext, edata, end, адреса переменных
process_memory: process_memory.o
	$(CC) -o $@ $^

#  Задание 5: psum (параллельная сумма) 
# Линкуется из трёх объектников + -pthread:
#   psum.o     — main: парсинг аргументов, создание/join потоков, замер времени
#   sum_lib.o  — библиотека: SumArray и SumWorker (потоковая функция)
#   utils.o    — GenerateArray из лабы №3
psum: psum.o sum_lib.o utils.o
	$(CC) -o $@ $^ $(LDLIBS)

#  Общее шаблонное правило компиляции 
# Любой X.c компилируется в X.o с флагами CFLAGS
# $< — первая зависимость (X.c), $@ — цель (X.o)
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

#  Очистка 
# Удаляет объектники, бинарники и временные файлы,
# которые создаёт parallel_min_max при --by_files
clean:
	rm -f *.o parallel_min_max process_memory psum minmax_part_*.txt

# all и clean — не файлы, а команды (иначе make может
# спутать их с одноимёнными файлами в директории)
.PHONY: all clean