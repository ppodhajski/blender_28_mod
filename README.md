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
*Warning* Do not open save your file with another version of Blender or custom values will be lost! You've been warned.

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
This example creates 1000 (n) splines within one curve object and sets random radii and custom values (for HairInfo.Value).

    # Blender 2.8 modification
    # Splines as hair example

    import numpy as np
    import bpy
    import random

    def create_spline(curve_data, s_type='NURBS', len_nodes=100, bud_position=None):
        s_type = 'NURBS'
        spline = curve_data.splines.new(type=s_type)

        # Regular spline points need xyz + weight
        got_points = 1
        co_dimension = 4
        pts = spline.points
        if s_type == 'BEZIER':
            got_points = 2
            # Bezier control points accept only xyz
            co_dimension = 3
            # Left and right handles are not handled here
            pts = spline.bezier_points

        # Spline already has got point(s) when created
        spline.points.add(len_nodes - got_points)

        if bud_position is None:
            bud_position = np.random.rand(1, co_dimension) * 100

        # Random + sin + cos just for demo, replace with your data
        # or what you generate with Sverchok or Animation Nodes
        radii = np.random.rand(len_nodes)
        radii *= radii * 4
        rnd_walk = np.arange(len_nodes)
        rnd_walk += 1
        rnd_walk *= int(3.1415926535 / len_nodes)
        nodes = np.random.rand(len_nodes, co_dimension)
        rf = random.random() + 1
        rf2 = random.random() + 1
        nodes[:, 0] += np.sin(np.cos(rnd_walk)) * 40
        nodes[:, 1] += (np.cos(np.sin(rnd_walk)**rf) + np.sin(rnd_walk*rf2)) * 40
        nodes[:, 2] += np.sin(rnd_walk*rf2) * np.cos(rnd_walk*rf) * 40
        nodes[:, :] *= 10
        nodes += bud_position
        # weight_softbody modifies values below 0.5, max is 100
        values = np.random.rand(len_nodes) + 0.5

        pts.foreach_set('radius', radii.ravel())
        pts.foreach_set('co', nodes.ravel())
        pts.foreach_set('value', values.ravel())

        spline.use_endpoint_u = True
        return spline

    def create_random_splines_within_curve(n_splines=100, n_nodes_per_spline=100, name='Curve > hair'):
        curve_data = bpy.data.curves.new(name=name, type='CURVE')
        # Tell Cycles to render all splines within this curve as hair 
        curve_data.cycles_curves.render_as_hair = True
        curve_data.dimensions = '3D'
        curve = bpy.data.objects.new(name=name, object_data=curve_data)

        # Alternate creation of bezier vs nurbs splines
        bezier_or_not = np.random.rand(n_splines)
        for spline_id in range(n_splines):
            len_nodes = int(n_nodes_per_spline + random.random() * 10)
            s_type = 'NURBS' if bezier_or_not[spline_id] < 0.5 else 'BEZIER'
            # curve_data equivalent to accessing curve.data
            spline = create_spline(curve_data, s_type, len_nodes)

        # Used to be bpy.data.scenes['Scene'].objects.link(curve)
        # Collection 1 or any existing collection on scene
        bpy.data.collections['Collection 1'].objects.link(curve)
        return curve

    # Main call to draw all the splines in a single curve object
    curve = create_random_splines_within_curve(1000, 100)

## Issues
One minor issue with Bezier splines: the rendered hair will be cut slightly before the last control point. Increasing the spline resolution reduces the issue. Don't hesitate to report any issue you see.