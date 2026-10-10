# Project history

## Matura thesis (v1.0)

This project started as a matura thesis, a graded project at the end of my Gymnasium (high school). The Puppet Cube V2 is a shapeshifting variant of the Rubik's Cube. The program consists of two main parts: 3D rendering of the cube and solution finding with the help of a search. It quickly finds short solutions to randomly scrambled cubes and improves them with additional search time. Given enough time and memory, this implementation can even prove a solution optimal, i.e. shortest possible.

The state of the project from the matura thesis can be found at release [v1.0](https://github.com/linusvdv/puppet-cube-v2/tree/v1.0). The abstract of the thesis is in the [README](../README.md#abstract-of-the-matura-thesis).

## Redesign (v2.0)

The following year was spent redesigning the project from the ground up. The result can be found at [v2.0](https://github.com/linusvdv/puppet-cube-v2/tree/v2.0). Further breakthroughs in the heuristic algorithms were found, and GPU acceleration was added, which required writing a new search from scratch. This new implementation proves optimality directly, in a few seconds, and in under a second even for the hardest cubes on strong hardware.

Up to this point, no AI was used in this project.

## AI-assisted development (v2.1 onwards)

From v2.0 onwards, AI is being used. Release [v2.1](https://github.com/linusvdv/puppet-cube-v2/tree/v2.1) contains the following improvements:

- The search was tidied up so that the code can run on the CPU alone.
- A 3D rendering of the cube was re-added, now as a Qt GUI.
- Memory usage can optionally be reduced (selected at compile time) by using two smaller edge heuristics.

## Future plans

- Solving arbitrary user-defined positions, after checking that the cube is in a legal, solvable state
- A neural network for image recognition to detect the user's position
- Possibly a web interface
