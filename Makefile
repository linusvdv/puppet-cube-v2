# CMAKE generated file: DO NOT EDIT!
# Generated by "Unix Makefiles" Generator, CMake Version 3.22

# Default target executed when no arguments are given to make.
default_target: all
.PHONY : default_target

# Allow only one "make -f Makefile2" at a time, but pass parallelism.
.NOTPARALLEL:

#=============================================================================
# Special targets provided by cmake.

# Disable implicit rules so canonical targets will work.
.SUFFIXES:

# Disable VCS-based implicit rules.
% : %,v

# Disable VCS-based implicit rules.
% : RCS/%

# Disable VCS-based implicit rules.
% : RCS/%,v

# Disable VCS-based implicit rules.
% : SCCS/s.%

# Disable VCS-based implicit rules.
% : s.%

.SUFFIXES: .hpux_make_needs_suffix_list

# Command-line flag to silence nested $(MAKE).
$(VERBOSE)MAKESILENT = -s

#Suppress display of executed commands.
$(VERBOSE).SILENT:

# A target that is always out of date.
cmake_force:
.PHONY : cmake_force

#=============================================================================
# Set environment variables for the build.

# The shell in which to execute make rules.
SHELL = /bin/sh

# The CMake executable.
CMAKE_COMMAND = /usr/bin/cmake

# The command to remove a file.
RM = /usr/bin/cmake -E rm -f

# Escaping for special characters.
EQUALS = =

# The top-level source directory on which CMake was run.
CMAKE_SOURCE_DIR = /mnt/c/Users/linus/Desktop/Schule/MNG_3,4/Maturarbeit/puppet-cube-v2

# The top-level build directory on which CMake was run.
CMAKE_BINARY_DIR = /mnt/c/Users/linus/Desktop/Schule/MNG_3,4/Maturarbeit/puppet-cube-v2

#=============================================================================
# Targets provided globally by CMake.

# Special rule for the target package
package: preinstall
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --cyan "Run CPack packaging tool..."
	/usr/bin/cpack --config ./CPackConfig.cmake
.PHONY : package

# Special rule for the target package
package/fast: package
.PHONY : package/fast

# Special rule for the target package_source
package_source:
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --cyan "Run CPack packaging tool for source..."
	/usr/bin/cpack --config ./CPackSourceConfig.cmake /mnt/c/Users/linus/Desktop/Schule/MNG_3,4/Maturarbeit/puppet-cube-v2/CPackSourceConfig.cmake
.PHONY : package_source

# Special rule for the target package_source
package_source/fast: package_source
.PHONY : package_source/fast

# Special rule for the target edit_cache
edit_cache:
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --cyan "No interactive CMake dialog available..."
	/usr/bin/cmake -E echo No\ interactive\ CMake\ dialog\ available.
.PHONY : edit_cache

# Special rule for the target edit_cache
edit_cache/fast: edit_cache
.PHONY : edit_cache/fast

# Special rule for the target rebuild_cache
rebuild_cache:
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --cyan "Running CMake to regenerate build system..."
	/usr/bin/cmake --regenerate-during-build -S$(CMAKE_SOURCE_DIR) -B$(CMAKE_BINARY_DIR)
.PHONY : rebuild_cache

# Special rule for the target rebuild_cache
rebuild_cache/fast: rebuild_cache
.PHONY : rebuild_cache/fast

# Special rule for the target list_install_components
list_install_components:
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --cyan "Available install components are: \"Unspecified\" \"devel\""
.PHONY : list_install_components

# Special rule for the target list_install_components
list_install_components/fast: list_install_components
.PHONY : list_install_components/fast

# Special rule for the target install
install: preinstall
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --cyan "Install the project..."
	/usr/bin/cmake -P cmake_install.cmake
.PHONY : install

# Special rule for the target install
install/fast: preinstall/fast
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --cyan "Install the project..."
	/usr/bin/cmake -P cmake_install.cmake
.PHONY : install/fast

# Special rule for the target install/local
install/local: preinstall
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --cyan "Installing only the local directory..."
	/usr/bin/cmake -DCMAKE_INSTALL_LOCAL_ONLY=1 -P cmake_install.cmake
.PHONY : install/local

# Special rule for the target install/local
install/local/fast: preinstall/fast
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --cyan "Installing only the local directory..."
	/usr/bin/cmake -DCMAKE_INSTALL_LOCAL_ONLY=1 -P cmake_install.cmake
.PHONY : install/local/fast

# Special rule for the target install/strip
install/strip: preinstall
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --cyan "Installing the project stripped..."
	/usr/bin/cmake -DCMAKE_INSTALL_DO_STRIP=1 -P cmake_install.cmake
.PHONY : install/strip

# Special rule for the target install/strip
install/strip/fast: preinstall/fast
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --cyan "Installing the project stripped..."
	/usr/bin/cmake -DCMAKE_INSTALL_DO_STRIP=1 -P cmake_install.cmake
.PHONY : install/strip/fast

# The main all target
all: cmake_check_build_system
	$(CMAKE_COMMAND) -E cmake_progress_start /mnt/c/Users/linus/Desktop/Schule/MNG_3,4/Maturarbeit/puppet-cube-v2/CMakeFiles /mnt/c/Users/linus/Desktop/Schule/MNG_3,4/Maturarbeit/puppet-cube-v2//CMakeFiles/progress.marks
	$(MAKE) $(MAKESILENT) -f CMakeFiles/Makefile2 all
	$(CMAKE_COMMAND) -E cmake_progress_start /mnt/c/Users/linus/Desktop/Schule/MNG_3,4/Maturarbeit/puppet-cube-v2/CMakeFiles 0
.PHONY : all

# The main clean target
clean:
	$(MAKE) $(MAKESILENT) -f CMakeFiles/Makefile2 clean
.PHONY : clean

# The main clean target
clean/fast: clean
.PHONY : clean/fast

# Prepare targets for installation.
preinstall: all
	$(MAKE) $(MAKESILENT) -f CMakeFiles/Makefile2 preinstall
.PHONY : preinstall

# Prepare targets for installation.
preinstall/fast:
	$(MAKE) $(MAKESILENT) -f CMakeFiles/Makefile2 preinstall
.PHONY : preinstall/fast

# clear depends
depend:
	$(CMAKE_COMMAND) -S$(CMAKE_SOURCE_DIR) -B$(CMAKE_BINARY_DIR) --check-build-system CMakeFiles/Makefile.cmake 1
.PHONY : depend

#=============================================================================
# Target rules for targets named PuppetCubeV2

# Build rule for target.
PuppetCubeV2: cmake_check_build_system
	$(MAKE) $(MAKESILENT) -f CMakeFiles/Makefile2 PuppetCubeV2
.PHONY : PuppetCubeV2

# fast build rule for target.
PuppetCubeV2/fast:
	$(MAKE) $(MAKESILENT) -f CMakeFiles/PuppetCubeV2.dir/build.make CMakeFiles/PuppetCubeV2.dir/build
.PHONY : PuppetCubeV2/fast

#=============================================================================
# Target rules for targets named sfml-system

# Build rule for target.
sfml-system: cmake_check_build_system
	$(MAKE) $(MAKESILENT) -f CMakeFiles/Makefile2 sfml-system
.PHONY : sfml-system

# fast build rule for target.
sfml-system/fast:
	$(MAKE) $(MAKESILENT) -f _deps/sfml-build/src/SFML/System/CMakeFiles/sfml-system.dir/build.make _deps/sfml-build/src/SFML/System/CMakeFiles/sfml-system.dir/build
.PHONY : sfml-system/fast

#=============================================================================
# Target rules for targets named sfml-window

# Build rule for target.
sfml-window: cmake_check_build_system
	$(MAKE) $(MAKESILENT) -f CMakeFiles/Makefile2 sfml-window
.PHONY : sfml-window

# fast build rule for target.
sfml-window/fast:
	$(MAKE) $(MAKESILENT) -f _deps/sfml-build/src/SFML/Window/CMakeFiles/sfml-window.dir/build.make _deps/sfml-build/src/SFML/Window/CMakeFiles/sfml-window.dir/build
.PHONY : sfml-window/fast

#=============================================================================
# Target rules for targets named sfml-network

# Build rule for target.
sfml-network: cmake_check_build_system
	$(MAKE) $(MAKESILENT) -f CMakeFiles/Makefile2 sfml-network
.PHONY : sfml-network

# fast build rule for target.
sfml-network/fast:
	$(MAKE) $(MAKESILENT) -f _deps/sfml-build/src/SFML/Network/CMakeFiles/sfml-network.dir/build.make _deps/sfml-build/src/SFML/Network/CMakeFiles/sfml-network.dir/build
.PHONY : sfml-network/fast

#=============================================================================
# Target rules for targets named sfml-graphics

# Build rule for target.
sfml-graphics: cmake_check_build_system
	$(MAKE) $(MAKESILENT) -f CMakeFiles/Makefile2 sfml-graphics
.PHONY : sfml-graphics

# fast build rule for target.
sfml-graphics/fast:
	$(MAKE) $(MAKESILENT) -f _deps/sfml-build/src/SFML/Graphics/CMakeFiles/sfml-graphics.dir/build.make _deps/sfml-build/src/SFML/Graphics/CMakeFiles/sfml-graphics.dir/build
.PHONY : sfml-graphics/fast

#=============================================================================
# Target rules for targets named sfml-audio

# Build rule for target.
sfml-audio: cmake_check_build_system
	$(MAKE) $(MAKESILENT) -f CMakeFiles/Makefile2 sfml-audio
.PHONY : sfml-audio

# fast build rule for target.
sfml-audio/fast:
	$(MAKE) $(MAKESILENT) -f _deps/sfml-build/src/SFML/Audio/CMakeFiles/sfml-audio.dir/build.make _deps/sfml-build/src/SFML/Audio/CMakeFiles/sfml-audio.dir/build
.PHONY : sfml-audio/fast

src/main.o: src/main.cpp.o
.PHONY : src/main.o

# target to build an object file
src/main.cpp.o:
	$(MAKE) $(MAKESILENT) -f CMakeFiles/PuppetCubeV2.dir/build.make CMakeFiles/PuppetCubeV2.dir/src/main.cpp.o
.PHONY : src/main.cpp.o

src/main.i: src/main.cpp.i
.PHONY : src/main.i

# target to preprocess a source file
src/main.cpp.i:
	$(MAKE) $(MAKESILENT) -f CMakeFiles/PuppetCubeV2.dir/build.make CMakeFiles/PuppetCubeV2.dir/src/main.cpp.i
.PHONY : src/main.cpp.i

src/main.s: src/main.cpp.s
.PHONY : src/main.s

# target to generate assembly for a file
src/main.cpp.s:
	$(MAKE) $(MAKESILENT) -f CMakeFiles/PuppetCubeV2.dir/build.make CMakeFiles/PuppetCubeV2.dir/src/main.cpp.s
.PHONY : src/main.cpp.s

# Help Target
help:
	@echo "The following are some of the valid targets for this Makefile:"
	@echo "... all (the default if no target is provided)"
	@echo "... clean"
	@echo "... depend"
	@echo "... edit_cache"
	@echo "... install"
	@echo "... install/local"
	@echo "... install/strip"
	@echo "... list_install_components"
	@echo "... package"
	@echo "... package_source"
	@echo "... rebuild_cache"
	@echo "... PuppetCubeV2"
	@echo "... sfml-audio"
	@echo "... sfml-graphics"
	@echo "... sfml-network"
	@echo "... sfml-system"
	@echo "... sfml-window"
	@echo "... src/main.o"
	@echo "... src/main.i"
	@echo "... src/main.s"
.PHONY : help



#=============================================================================
# Special targets to cleanup operation of make.

# Special rule to run CMake to check the build system integrity.
# No rule that depends on this can have commands that come from listfiles
# because they might be regenerated.
cmake_check_build_system:
	$(CMAKE_COMMAND) -S$(CMAKE_SOURCE_DIR) -B$(CMAKE_BINARY_DIR) --check-build-system CMakeFiles/Makefile.cmake 0
.PHONY : cmake_check_build_system

