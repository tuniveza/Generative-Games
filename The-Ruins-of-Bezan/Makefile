CC      := gcc
CFLAGS  := -std=c17 -Wall -Wextra -g -O2 -Ideps/glad/include -Ideps/cglm/include -Ideps/cgltf -Ideps/stb $(shell pkg-config --cflags sdl3)
LDLIBS  := $(shell pkg-config --libs sdl3) -lm

BUILD   := build
SRCS    := main.c game.c creatures.c beasts.c magic.c npc.c menu.c weather.c audio.c level.c hands.c items.c ui.c particles.c renderer.c meshgen.c \
           world.c coast.c village.c volcano.c undersea.c ocean.c player.c animals.c quest.c fishing.c shapes.c \
           model.c texture.c shader.c camera.c terrain.c deps/glad/src/gl.c deps/impl.c
OBJS    := $(SRCS:%.c=$(BUILD)/%.o)
TARGET  := $(BUILD)/bezan

.PHONY: all run clean

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $^ -o $@ $(LDLIBS)

# third-party code: don't show its warnings
$(BUILD)/deps/impl.o: CFLAGS += -w

$(BUILD)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

run: all
	./$(TARGET)

clean:
	rm -rf $(BUILD)

-include $(OBJS:.o=.d)
