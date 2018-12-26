# Render splines without meshing in Blender, using Cycles hair primitives

![](https://www.antillevisuals.com/technical-research/cycles_mod_splines_as_hair.jpg)

## What's this?
This is a Blender/Cycles modification that gives extra options for rendering splines.
Option 1: you may render curves as hair primitives without need for pre-meshing, allowing larger scale visualisations.
Option 2: you may pass any float between 0.5 and 100 to weight_softbody attribute of spline or b-spline points and it will be available in Cycles node system under HairInfo.

## How to use it?
You need to compile Blender with these sources. You need to have git installed on your system or skip step 1 and download sources from github.
In your console run the following commands (valid for Linux/MacOS/Windows):
1. `git clone git://github.com/nantille/blender_28_mod`
2. `cd blender_28_mod`
3. `make`

If everything compiles fine, you will have a new Blender executable in relative path `../build_[platform]/bin/`

If you would like to compile Blender with the latest sources published by Blender Institute,
you can try rebasing with the following command:
`git pull --rebase http://git.blender.org/blender.git master`

## Version
This is a modification for Blender 2.8.

## Usage
1. Open Blender
2. Select Cycles as render engine.
3. Select or add a curve object on the scene
4. Under `Object Data` panel at the bottom, enable `Render as Hair`.
5. Make sure that in panel `Scene`, under `Geometry`, the option `Use Hair` is enabled.
6. Tweak options under `Use Hair`, set `Shape` to `Thick` and `Primitive` to `Curve Segments`.

## Issues
None found at the moment. Don't hesitate to report any issue you see.