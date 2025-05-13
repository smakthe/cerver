CC = gcc
CFLAGS = -Wall -Wextra -g -pthread
LDFLAGS = -pthread

# Directory paths
SRC_DIR = .
SERVER_DIR = $(SRC_DIR)/server
MODELS_DIR = $(SRC_DIR)/models
CONTROLLERS_DIR = $(SRC_DIR)/controllers
ROUTES_DIR = $(SRC_DIR)/routes
DATABASE_DIR = $(SRC_DIR)/database
DB_APP_DIR = $(DATABASE_DIR)/application
DB_LOGICAL_DIR = $(DATABASE_DIR)/logical
DB_PHYSICAL_DIR = $(DATABASE_DIR)/physical

# Source files
SRCS = $(wildcard $(SRC_DIR)/*.c) \
       $(wildcard $(SERVER_DIR)/*.c) \
       $(filter-out $(MODELS_DIR)/default_models.c, $(wildcard $(MODELS_DIR)/*.c)) \
       $(wildcard $(CONTROLLERS_DIR)/*.c) \
       $(wildcard $(ROUTES_DIR)/*.c) \
       $(wildcard $(DB_APP_DIR)/*.c) \
       $(wildcard $(DB_LOGICAL_DIR)/*.c) \
       $(wildcard $(DB_PHYSICAL_DIR)/*.c)

# Object files
OBJS = $(SRCS:.c=.o)

# Executable
TARGET = cerver

# Header directories for include
INCLUDES = -I$(SRC_DIR) -I$(SERVER_DIR) -I$(MODELS_DIR) -I$(CONTROLLERS_DIR) -I$(ROUTES_DIR) -I$(DATABASE_DIR) -I$(DB_APP_DIR) -I$(DB_LOGICAL_DIR) -I$(DB_PHYSICAL_DIR)

# Default target
all: $(TARGET)

# Linking rule
$(TARGET): $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $^

# Compilation rule
%.o: %.c
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

# Clean rule
clean:
	rm -f $(OBJS) $(TARGET)

# Phony targets
.PHONY: all clean
