![](https://www.antillevisuals.com/technical-research/cycles_mod_splines_as_hair.jpg)

# Render splines using Cycles hair primitives
I found no render engine that renders mesh-less splines as hair primitives with custom radii and data. 
So I edited Cycles and Blender, initially 2.79 and now 2.8 to do this. 

This modification allows you to skip meshing splines entirely and render them as hair. 
Needless to say, with this method you can render *lots* of them (hello large-scale).

This is particularly useful in the visualisation of graph structures such as trees, random walks, cells, etc.

## Features
This is a Cycles modification that allows you to skip meshing splines entirely and render them as hair. Needless to say, with this method you can render *lots* of them (hello large-scale).

1. Splines to hair: render all splines within a curve object as hair in one click in the UI, also easily scriptable.
2. Radius variation: splines are rendered with their radii so this is more than simple tapering hair.
3. Spline interpolation: all curve types are supported. Two interpolations may occur and affect hair, see Usage below.
4. Per-spline material: supported. You need to add the materials to the curve object and then set the material_index of each spline.
5. Two custom values on spline points: `key` and `value`, two floats. This takes 8 bytes extra per spline point in memory but gives you the ability to modulate your shaders based on these values as they're available in `HairInfo` node.
6. Extra spline metrics available in HairInfo node: `Spline Index`, `Splines Count`, `Spline Length`
7. Increased transparency limit to enormous numbers to let the user play with transparency + many splines

*Warning* When you save .blend files with this version, *do not save them with another version of Blender* or custom values will be lost! You've been warned.

## How to use it?
You need to compile Blender with these sources. You need to have git installed on your system or skip step 1 and download sources from github.
Make sure you have python3.7+ installed as this is required by Blender 2.8.

In your console run the following commands (valid for Linux/MacOS/Windows):

1. `git clone git://github.com/nantille/blender_28_mod`
2. `cd blender_28_mod`
3. `make update`

After that, you need to fetch submodules like addons, translation, etc.:

4. `git remote set-url origin git://git.blender.org/blender.git`
5. `git submodule sync`
6. `git submodule update --init --recursive`
7. `git submodule foreach git checkout master`
8. `git submodule foreach git pull --rebase origin master`
9. `git remote set-url origin https://github.com/nantille/blender_28_mod.git`
10. `make`

If you would like to compile Blender with the latest sources published by Blender Institute,
you can try rebasing with the following command but there might be conflicts:
`git pull --rebase http://git.blender.org/blender.git master`

If everything compiles fine (fingers crossed!), you will have a new Blender executable in relative path `../build_[platform]/bin/`
The path to the blender executable is given at the end of the compilation.

If you have a Cmake Error mentioning `Python executable missing` and you have Python installed, you probably have the wrong version. Blender 2.8 requires Python 3.7+.

## Version
This is a modification for Blender 2.8+.

## Usage
1. Open this custom version of Blender 2.8 and make sure Cycles is your render engine.
2. Select or add a curve object on the scene.
3. Under `Object Data` panel at the bottom, enable `Render as Hair`.
4. Make sure that in panel `Scene`, under `Geometry`, the option `Use Hair` is enabled.
5. Tweak options under `Use Hair`, set `Shape` to `Thick` and `Primitive` to `Curve Segments`.
6. Tweak the `Subdivision` parameter and also the `Resolution Preview U` and/or `Render U` parameters of your curve/splines

In Python:
`bpy.context.scene.cycles_curves.primitive = 'CURVE_SEGMENTS'`
`bpy.context.scene.cycles_curves.subdivisions = 3`


## Hair != beveled curves
Please note that when a spline is rendered as a hair primitive, it has *no mesh* and so all bevel/taper and meshing related features of splines/curves will be ignored.
So with this method:
- no mesh-related feature like fill, twisting, taper, bevel, ...
- no uv mapping, though you can get U coordinate with HairInfo.Intercept multiplied with the new HairInfo.SplineLength.
- no cyclic property (you can manually move the last point of your curve to match the first one or...do a PR on this code)

## Testing
1. Copy-paste or import the code of `/blender/intern/cycles/blender/addon/splines_as_hair_sample.py`
2. At the bottom of the script, tweak how many splines you want to have generated.
3. Run the script. If you generate a large number of splines, like 1 million, it will easily take 15 minutes.
4. Add a material and play with HairInfo.Intercept, HairInfo.Key and HairInfo.Value

## Issues
One issue: if you go in Preview mode and assign a material to a curve that had none, Blender crashes. 
Solution: you have to assign the material before Preview mode.
Don't hesitate to report any issue you see.