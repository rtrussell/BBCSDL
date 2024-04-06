# Update these paths as necessary to match your installation
VPATH = ../../src ../../include ../../../BBCSDL/src ../../../BBCSDL/include
# Set the include paths
CXX = g++ -Wall -I ../../include -I ../../../BBCSDL/include

# Compiler and flags
CC = gcc
CFLAGS = -Wall -O2

# Source directories
SRCDIR_arm = src_arm
SRCDIR_x86_64 = src_x86_64
SRCDIR_RISCV = src_RISCV
SRCDIR_LoongArch = src_LoongArch

# Include directories
INCLUDEDIR_arm = include_arm
INCLUDEDIR_x86_64 = include_x86_64
INCLUDEDIR_RISCV = include_RISCV
INCLUDEDIR_LoongArch = include_LoongArch

# Output directories
OBJDIR_arm = obj_arm
OBJDIR_x86_64 = obj_x86_64
OBJDIR_RISCV = obj_RISCV
OBJDIR_LoongArch = obj_LoongArch

# Output files
TARGET_arm = program_arm
TARGET_x86_64 = program_x86_64
TARGET_RISCV = program_RISCV
TARGET_LoongArch = program_LoongArch

# Source files
SRC_arm = $(wildcard $(SRCDIR_arm)/*.c)
SRC_x86_64 = $(wildcard $(SRCDIR_x86_64)/*.c)
SRC_RISCV = $(wildcard $(SRCDIR_RISCV)/*.c)
SRC_LoongArch = $(wildcard $(SRCDIR_LoongArch)/*.c)

# Object files
OBJ_arm = $(patsubst $(SRCDIR_arm)/%.c, $(OBJDIR_arm)/%.o, $(SRC_arm))
OBJ_x86_64 = $(patsubst $(SRCDIR_x86_64)/%.c, $(OBJDIR_x86_64)/%.o, $(SRC_x86_64))
OBJ_RISCV = $(patsbust $(SRCDIR_RISCV)/*.c, $(OBJDIR_RISCV)/%.o, $(SRC_RISCV))
OBJ_LoongArch = $(patsbust $(SRCDIR_LoongArch)/*.c, $(OBJDIR_LoongArch)/%.o, $(SRC_LoongArch))

# Default target
all: $(TARGET_arm) $(TARGET_x86_64)

# Target for ARM
$(TARGET_arm): $(OBJ_arm)
	$(CC) $(CFLAGS) -I$(INCLUDEDIR_arm) $^ -o $@

# Target for x86_64
$(TARGET_x86_64): $(OBJ_x86_64)
	$(CC) $(CFLAGS) -I$(INCLUDEDIR_x86_64) $^ -o $@

# Target for RISCV
$(TARGET_RISCV): $(OBJ_RISCV)
	$(CC) $(CFLAGS) -I$(INCLUDEDIR_RISCV) $^ -o $@

# Target for LoongArch
$(TARGET_LoongArch): $(OBJ_LoongArch)
	$(CC) $(CFLAGS) -I$(INCLUDEDIR_LoongArch) $^ -o $@

# Compile ARM source files
$(OBJDIR_arm)/%.o: $(SRCDIR_arm)/%.c
	$(CC) $(CFLAGS) -I$(INCLUDEDIR_arm) -c $< -o $@

# Compile x86_64 source files
$(OBJDIR_x86_64)/%.o: $(SRCDIR_x86_64)/%.c
	$(CC) $(CFLAGS) -I$(INCLUDEDIR_x86_64) -c $< -o $@

# Compile for RISCV source files
$(OBJDIR_RISCV): $(SRCDIR_RISCV)
	$(CC) $(CFLAGS) -I$(INCLUDEDIR_RISCV) $^ -o $@

# Compile for LoongArch source files
$(OBJDIR_LoongArch): $(SRCDIR_LoongArch)
	$(CC) $(CFLAGS) -I$(INCLUDEDIR_LoongArch) $^ -o $@

# Clean rule
clean:
	rm -f $(OBJ_arm) $(OBJ_x86_64) $(OBJ_RISCV) $(OBJ_LoongArch) $(TARGET_arm) $(TARGET_x86_64) $(TARGET_RISCV) $(TARGET_LoongArch)

# Phony target
.PHONY: all clean

