# QuickTray Win32 Unicode C Makefile
# Compatible with mingw32-make.exe and mingw64-make

CC      = gcc
WINDRES = windres

SRC_DIR = src
OBJ_DIR = obj
TARGET  = QuickTray.exe

CFLAGS  = -O2 -std=gnu11 -DUNICODE -D_UNICODE -D_WIN32_WINNT=0x0601 -D_WIN32_IE=0x0800 -finput-charset=UTF-8 -I$(SRC_DIR)
LDFLAGS = -s -mwindows -municode
LIBS    = -luser32 -lgdi32 -lshell32 -lcomctl32 -lcomdlg32 -lole32 -luuid -ladvapi32 -luxtheme -lshlwapi

SRCS    = $(SRC_DIR)/main.c \
          $(SRC_DIR)/config.c \
          $(SRC_DIR)/i18n.c \
          $(SRC_DIR)/theme.c \
          $(SRC_DIR)/tray_menu.c \
          $(SRC_DIR)/manager_dlg.c

HEADERS = $(SRC_DIR)/config.h \
          $(SRC_DIR)/i18n.h \
          $(SRC_DIR)/theme.h \
          $(SRC_DIR)/tray_menu.h \
          $(SRC_DIR)/manager_dlg.h \
          $(SRC_DIR)/resource.h

OBJS    = $(OBJ_DIR)/main.o \
          $(OBJ_DIR)/config.o \
          $(OBJ_DIR)/i18n.o \
          $(OBJ_DIR)/theme.o \
          $(OBJ_DIR)/tray_menu.o \
          $(OBJ_DIR)/manager_dlg.o

RES_OBJ = $(OBJ_DIR)/resource.res

.PHONY: all clean rebuild

all: $(TARGET)

$(TARGET): $(OBJS) $(RES_OBJ)
	$(CC) $(LDFLAGS) -o $@ $(OBJS) $(RES_OBJ) $(LIBS)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c $(HEADERS)
	-@$(MKDIR_OBJ)
	$(CC) $(CFLAGS) -c $< -o $@

$(RES_OBJ): $(SRC_DIR)/resource.rc $(SRC_DIR)/resource.h $(SRC_DIR)/app.ico $(SRC_DIR)/app.manifest
	-@$(MKDIR_OBJ)
	$(WINDRES) -O coff -I $(SRC_DIR) $< -o $@

clean:
	-$(RM_CMD)

rebuild: clean all
