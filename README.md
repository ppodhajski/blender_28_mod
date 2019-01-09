![](https://www.antillevisuals.com/technical-research/cycles_mod_splines_as_hair.jpg)

# Render splines using Cycles hair primitives

## Features
This is a Cycles modification that allows you to skip meshing splines entirely and render them as hair. Needless to say, with this method you can render *lots* of them (hello large-scale).
1. Splines to hair:
Render all splines within a curve object as hair in one click in the UI, also easily scriptable.
2. Radius variation:
Splines are rendered with their radii so this is more than simple tapering hair.
3. Per-spline material:
This modification allows to have per-spline materials. You need to add the materials to the curve object and then set the material_index of each spline.
4. Two custom values on spline points:
In the Hair Info node of Cycles, the Value slot is now added. It allows you to pass a float value per spline point to the `value` property and use it for modulating materials. 
*Warning* When you save .blend files with this version, *do not save them with another version of Blender* or custom values will be lost! You've been warned.

## How to use it?
You need to compile Blender with these sources. You need to have git installed on your system or skip step 1 and download sources from github.
Make sure you have python3.7+ installed as this is required by Blender 2.8.

In your console run the following commands (valid for Linux/MacOS/Windows):
1. `git clone git://github.com/nantille/blender_28_mod`
2. `cd blender_28_mod`
3. `make update`
4. `make`

If everything compiles fine (fingers crossed!), you will have a new Blender executable in relative path `../build_[platform]/bin/`
The path to the blender executable is given at the end of the compilation.

If you have a Cmake Error mentioning `Python executable missing` and you have Python installed, you probably have the wrong version.

If you would like to compile Blender with the latest sources published by Blender Institute,
you can try rebasing with the following command:
`git pull --rebase http://git.blender.org/blender.git master`

## Version
This is a modification for Blender 2.8+.

## Usage
1. Open this custom version of Blender 2.8.
2. Select Cycles as render engine.
3. Select or add a curve object on the scene.
4. Under `Object Data` panel at the bottom, enable `Render as Hair`.
5. Make sure that in panel `Scene`, under `Geometry`, the option `Use Hair` is enabled.
6. Tweak options under `Use Hair`, set `Shape` to `Thick` and `Primitive` to `Curve Segments`.

## Hair != beveled curves
Please note that when a spline is rendered as a hair primitive, it has *no mesh* and so all bevel/taper and meshing related features of splines/curves will be ignored.
So with this method:
- no mesh-related like fill, twisting, resolution, taper, bevel, ...
- no uv mapping, though you can get U coordinate with HairInfo.Intercept multiplied with the new HairInfo.SplineLength.
- no cyclic property (you can manually move the last point of your curve to match the first one or...do a PR on this code)

## Testing
1. Copy-paste or import the code of `/blender/intern/cycles/blender/addon/splines_as_hair_sample.py`
2. At the bottom of the script, tweak how many splines you want to have generated.
3. Run the script. If you generate large number of splines, like 1 million, it will easily take 15 minutes.
4. Add a material and play with HairInfo.Intercept, HairInfo.Key and HairInfo.Value

## Issues
One issue: if you go in Preview mode and assign a material to a curve that had none, Blender crashes. 
Solution: you have to assign the material before Preview mode.
Don't hesitate to report any issue you see.