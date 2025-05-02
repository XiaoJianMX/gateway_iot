# 交叉编译工具链
# export ARCH := arm
# export CROSS_COMPILE := /usr/local/arm/gcc-linaro-4.9.4-2017.01-x86_64_arm-linux-gnueabihf/bin/arm-linux-gnueabihf-
# CROSS_COMPILE := arm-linux-gnueabihf-


TARGET := Gateway_IOT


SRC := $(shell find  ./src -name "*.c")


OBJS := $(SRC:./%=./build/%)  
OBJS := $(OBJS:%.c=%.o)       


CFLAGS := -Wall -g -std=c11
CFLAGS += -I ./sqlite3/include \
          -I ./inc \
		  -DCOMPILE_TIME="\"$(shell date +'%Y-%m-%d %H:%M:%S')\""

LDFLAGS := -L./sqlite3/lib -l:libsqlite3.a -lpthread -lm
# LDFLAGS := -L./sqlite3-arm/lib -l:libsqlite3.a -lpthread -lm -ldl
all: $(TARGET)

debug: CFLAGS += -DDEBUG 
debug: $(TARGET)


$(TARGET): $(OBJS)
	@mkdir -p $(dir $@)
	$(CROSS_COMPILE)gcc -o $@ $^ $(LDFLAGS)


./build/%.o: %.c
	@mkdir -p $(dir $@)
	$(CROSS_COMPILE)gcc $(CFLAGS) -c $< -o $@


print:
	@echo "SRC: $(SRC)"
	@echo "OBJS: $(OBJS)"

# 清理生成的文件
clean:
	rm -rf ./build
	rm -f $(TARGET)

.PHONY: clean print debug